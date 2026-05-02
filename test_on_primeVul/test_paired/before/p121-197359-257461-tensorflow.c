Status AutoParallel::Initialize(const GrapplerItem& item) {
  num_gpus_ = GetNumAvailableGPUs();
  LOG(INFO) << "Number of GPUs: " << num_gpus_;
  item_ = &item;
  graph_ = item.graph;
  LOG(INFO) << "Original graph size: " << graph_.node_size();
  if (item.fetch.empty()) {
    return Status(error::INVALID_ARGUMENT, "No fetch nodes provided.");
  }

  if (item.MainVariables().empty()) {
    return Status(error::INVALID_ARGUMENT, "No variables provided.");
  }

  for (const auto& init : item.init_ops) {
    VLOG(1) << "Init node: " << init;
  }

  for (const auto& fetch : item.fetch) {
    VLOG(1) << "Fetch node: " << fetch;
  }

  for (const auto& var : item.MainVariables()) {
    VLOG(2) << "Variable: " << var->name();
  }

  const std::set<string> apply_gradients_ops = {"ApplyGradientDescent",
                                                "ApplyProximalGradientDescent",
                                                "ApplyAdadelta",
                                                "ApplyAdagrad",
                                                "ApplyProximalAdagrad",
                                                "ApplyAdagradDA",
                                                "ApplyFtrl",
                                                "ApplyMomentum",
                                                "ApplyAdam",
                                                "ApplyRMSProp",
                                                "ApplyCenteredRMSProp"};
  for (int i = 0; i < graph_.node_size(); i++) {
    all_nodes_.insert(
        std::make_pair(graph_.node(i).name(), graph_.mutable_node(i)));
    if (apply_gradients_ops.find(graph_.node(i).op()) !=
        apply_gradients_ops.end()) {
      apply_gradients_nodes_.insert(graph_.node(i).name());
      VLOG(2) << "Apply gradients node: " << graph_.node(i).name();
    }
  }

  auto div_const_node = AddNodeDivConst();
  all_nodes_.insert(std::make_pair(div_const_node->name(), div_const_node));
  std::map<string, int> gradient_pos = {{"ApplyGradientDescent", 2},
                                        {"ApplyProximalGradientDescent", 4},
                                        {"ApplyAdadelta", 6},
                                        {"ApplyAdagrad", 3},
                                        {"ApplyProximalAdagrad", 5},
                                        {"ApplyAdagradDA", 3},
                                        {"ApplyFtrl", 3},
                                        {"ApplyMomentum", 3},
                                        {"ApplyAdam", 9},
                                        {"ApplyRMSProp", 7},
                                        {"ApplyCenteredRMSProp", 8}};
  for (const auto& apply_gradient_node_name : apply_gradients_nodes_) {
    auto apply_gradients_op = all_nodes_[apply_gradient_node_name]->op();
    auto apply_gradients_node = all_nodes_[apply_gradient_node_name];

    auto div_node = AddNodeDiv(
        apply_gradient_node_name,
        apply_gradients_node->input(gradient_pos[apply_gradients_op]),
        div_const_node->name());
    all_nodes_.insert(std::make_pair(div_node->name(), div_node));
    *apply_gradients_node->mutable_input(gradient_pos[apply_gradients_op]) =
        div_node->name();
  }
  LOG(INFO) << "Graph size after adding div nodes: " << all_nodes_.size();

  std::vector<const NodeDef*> train_nodes;
  TF_RETURN_IF_ERROR(ComputeTransitiveFanin(graph_, item.fetch, &train_nodes));
  LOG(INFO) << "Number of training nodes: " << train_nodes.size();

  const NodeDef* dequeue_node;
  for (const auto& train_node : train_nodes) {
    if (IsDequeueOp(*train_node)) {
      dequeue_node = train_node;
      break;
    }
  }

  std::vector<const NodeDef*> input_nodes;
  if (dequeue_node) {
    LOG(INFO) << "Dequeue node: " << dequeue_node->name();
    TF_RETURN_IF_ERROR(ComputeTransitiveFanin(graph_, {dequeue_node->name()},
                                              {}, &input_nodes));
  }
  LOG(INFO) << "Number of input nodes: " << input_nodes.size();

  std::set<string> dont_replicate_nodes;
  for (const auto& variable : item.MainVariables()) {
    dont_replicate_nodes.insert(variable->name());
  }

  for (const auto& init : item.init_ops) {
    dont_replicate_nodes.insert(NodeName(init));
  }

  // Don't replicate all input nodes, except the dequeue node.
  for (const auto& input_node : input_nodes) {
    if (input_node->name() != dequeue_node->name()) {
      dont_replicate_nodes.insert(input_node->name());
    }
  }

  for (const auto& node : train_nodes) {
    if (dont_replicate_nodes.find(node->name()) == dont_replicate_nodes.end()) {
      replica_nodes_.insert(node->name());
    }
  }
  LOG(INFO) << "Number of replica nodes: " << replica_nodes_.size();

  for (const auto& node : all_nodes_) {
    if (replica_nodes_.find(node.first) == replica_nodes_.end()) {
      shared_nodes_.insert(node.first);
    }
  }
  LOG(INFO) << "Number of shared nodes: " << shared_nodes_.size();
  return Status::OK();
}