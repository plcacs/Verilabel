  Status BuildInputArgIndex(const OpDef::ArgDef& arg_def, AttrSlice attr_values,
                            const FunctionDef::ArgAttrs* arg_attrs,
                            bool ints_on_device,
                            int64_t resource_arg_unique_id) {
    bool is_type_list;
    DataTypeVector dtypes;
    TF_RETURN_IF_ERROR(
        ArgNumType(attr_values, arg_def, &is_type_list, &dtypes));
    if (dtypes.size() < size_t{1}) {
      return errors::Internal("Expected a list of at least one dtype");
    }
    int arg_index = result_.nodes.size();
    TF_RETURN_IF_ERROR(
        AddItem(arg_def.name(), {true, arg_index, 0, is_type_list, dtypes}));
    // Creates dtypes.size() nodes in the graph.
    for (size_t i = 0; i < dtypes.size(); ++i) {
      TF_RETURN_IF_ERROR(AddItem(strings::StrCat(arg_def.name(), ":", i),
                                 {true, arg_index, 0, false, {dtypes[i]}}));
      if (arg_index != result_.nodes.size()) {
        return errors::Internal(
            "Expected arg_index to be equal to the number of nodes in result.",
            " Got ", arg_index, " and ", result_.nodes.size());
      }
      string name = arg_def.name();
      if (dtypes.size() > 1) {
        strings::StrAppend(&name, "_", i);
      }
      NodeDef* gnode = AddNode(name);
      if (ints_on_device && dtypes[i] == DataType::DT_INT32) {
        gnode->set_op(FunctionLibraryDefinition::kDeviceArgOp);
      } else {
        gnode->set_op(FunctionLibraryDefinition::kArgOp);
      }
      DataType dtype = arg_def.is_ref() ? MakeRefType(dtypes[i]) : dtypes[i];
      AddAttr("T", dtype, gnode);
      AddAttr("index", arg_index, gnode);
      if (resource_arg_unique_id >= 0) {
        AddAttr("_resource_arg_unique_id", resource_arg_unique_id, gnode);
      }
      if (arg_attrs) {
        for (const auto& arg_attr : arg_attrs->attr()) {
          AddAttr(arg_attr.first, arg_attr.second, gnode->mutable_attr());
        }
      }
      result_.arg_types.push_back(dtypes[i]);
      ++arg_index;
    }
    return Status::OK();
  }