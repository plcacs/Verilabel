void Node::RunForwardTypeInference() {
  VLOG(4) << "Forward type inference: " << props_->node_def.DebugString();

  if (props_->fwd_type_fn == nullptr) {
    return;
  }

  std::vector<Node*> input_nodes(props_->input_types.size(), nullptr);
  std::vector<int> input_idx(props_->input_types.size(), 0);
  for (const auto& edge : in_edges_) {
    if (edge->IsControlEdge()) {
      continue;
    }
    DCHECK(edge->dst_input() < input_nodes.size()) << DebugString();
    int i = edge->dst_input();
    input_nodes.at(i) = edge->src();
    input_idx.at(i) = edge->src_output();
  }

  // Note: technically, we could use a very generic type when some of the inputs
  // are unknown. But there is an expectation that a node will have complete
  // inputs soon, so updating intermediate types is largely unnecessary.

  for (const auto* node : input_nodes) {
    if (node == nullptr) {
      // Incomplete inputs, bail.
      ClearTypeInfo();
      return;
    }
  }

  static FullTypeDef* no_type = new FullTypeDef();

  std::vector<std::reference_wrapper<const FullTypeDef>> input_types;
  for (int i = 0; i < input_nodes.size(); i++) {
    const auto* node = input_nodes[i];
    if (node->def().has_experimental_type()) {
      const auto& node_t = node->def().experimental_type();
      if (node_t.type_id() != TFT_UNSET) {
        int ix = input_idx[i];
        if (ix >= node_t.args_size()) {
          LOG(WARNING) << name() << " has bad type information: input " << i
                       << " should have an output " << ix
                       << " but instead only has " << node_t.args_size()
                       << " outputs: " << node_t.DebugString()
                       << "\nThis indicates either "
                          "a bug in op registration or a corrupted graph.";
          ClearTypeInfo();
          return;
        }
        input_types.emplace_back(node_t.args(ix));
      } else {
        input_types.emplace_back(*no_type);
      }
    } else {
      // Incomplete inputs, bail.
      ClearTypeInfo();
      return;
    }
  }

  const auto infer_type = props_->fwd_type_fn(input_types);
  const FullTypeDef infer_typedef = infer_type.ValueOrDie();
  if (infer_typedef.type_id() != TFT_UNSET) {
    MaybeCopyOnWrite();
    *(props_->node_def.mutable_experimental_type()) = infer_typedef;
  }
}