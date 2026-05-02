void AcceptRoutingHandler<Pipeline, R>::onRoutingData(
    uint64_t connId,
    typename RoutingDataHandler<R>::RoutingData& routingData) {
  // Get the routing pipeline corresponding to this connection
  auto routingPipelineIter = routingPipelines_.find(connId);
  if (routingPipelineIter == routingPipelines_.end()) {
    VLOG(2) << "Connection has already been closed, "
               "or routed to a worker thread.";
    return;
  }
  auto routingPipeline = std::move(routingPipelineIter->second);
  routingPipelines_.erase(routingPipelineIter);

  // Fetch the socket from the pipeline and pause reading from the
  // socket
  auto socket = std::dynamic_pointer_cast<folly::AsyncSocket>(
      routingPipeline->getTransport());
  routingPipeline->transportInactive();
  socket->detachEventBase();

  // Hash based on routing data to pick a new acceptor
  uint64_t hash = std::hash<R>()(routingData.routingData);
  auto acceptor = acceptors_[hash % acceptors_.size()];

  // Switch to the new acceptor's thread
  acceptor->getEventBase()->runInEventBaseThread(
      [ =, routingData = std::move(routingData) ]() mutable {
        socket->attachEventBase(acceptor->getEventBase());

        auto routingHandler =
            routingPipeline->template getHandler<RoutingDataHandler<R>>();
        DCHECK(routingHandler);
        auto transportInfo = routingPipeline->getTransportInfo();
        auto pipeline = childPipelineFactory_->newPipeline(
            socket, routingData.routingData, routingHandler, transportInfo);

        auto connection =
            new typename ServerAcceptor<Pipeline>::ServerConnection(pipeline);
        acceptor->addConnection(connection);

        pipeline->transportActive();

        // Pass in the buffered bytes to the pipeline
        pipeline->read(routingData.bufQueue);
      });
}