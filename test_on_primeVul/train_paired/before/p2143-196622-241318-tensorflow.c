Status NestedStackRaggedTensors(
    const std::vector<RaggedTensorVariant>& ragged_components,
    const std::vector<int>& nested_dim_sizes, const int input_ragged_rank,
    const int output_ragged_rank, RaggedTensorVariant* output_ragged) {
  output_ragged->mutable_nested_splits()->reserve(output_ragged_rank);
  const int dims = nested_dim_sizes.size();

  // Populate first `dims - 1` splits.
  for (int i = 0; i < dims - 1; i++) {
    int dims_splits_size = nested_dim_sizes[i] + 1;
    output_ragged->append_splits(Tensor(DataTypeToEnum<SPLIT_TYPE>::value,
                                        TensorShape({dims_splits_size})));
    auto splits_vec = output_ragged->mutable_splits(i)->vec<SPLIT_TYPE>();
    int split_diff = nested_dim_sizes[i + 1];
    for (int j = 0; j < dims_splits_size; j++) {
      splits_vec(j) = j * split_diff;
    }
  }

  // Populate `dims`-th split.
  int splits_size = ragged_components.size() + 1;
  output_ragged->append_splits(
      Tensor(DataTypeToEnum<SPLIT_TYPE>::value, TensorShape({splits_size})));
  auto dims_splits_vec =
      output_ragged->mutable_splits(dims - 1)->vec<SPLIT_TYPE>();
  dims_splits_vec(0) = 0;
  for (int i = 0; i < ragged_components.size(); i++) {
    int split_val = ragged_components[i].values().shape().dim_size(0);
    if (input_ragged_rank != 0 && ragged_components[i].ragged_rank() > 0) {
      split_val = ragged_components[i].splits(0).NumElements() - 1;
    }
    dims_splits_vec(i + 1) = dims_splits_vec(i) + split_val;
  }

  // Populate last `input_ragged_rank` splits.
  for (int i = 0; i < input_ragged_rank; i++) {
    int split_index = dims + i;
    int split_size = 1;
    for (int j = 0; j < ragged_components.size(); j++) {
      if (!ragged_components[j].nested_splits().empty()) {
        split_size += ragged_components[j].splits(i).NumElements() - 1;
      }
    }
    output_ragged->append_splits(
        Tensor(DataTypeToEnum<SPLIT_TYPE>::value, TensorShape({split_size})));
    auto splits_vec =
        output_ragged->mutable_splits(split_index)->vec<SPLIT_TYPE>();
    splits_vec(0) = 0;
    SPLIT_TYPE last_split_value = 0;
    int index = 1;
    for (int j = 0; j < ragged_components.size(); j++) {
      if (ragged_components[j].nested_splits().empty()) {
        // Corner case: empty row. e.g [ [[x], [x]], [] ]
        continue;
      }
      auto component_splits_vec =
          ragged_components[j].splits(i).vec<SPLIT_TYPE>();
      for (int k = 1; k < component_splits_vec.size(); k++, index++) {
        splits_vec(index) = component_splits_vec(k) + last_split_value;
      }
      last_split_value = splits_vec(index - 1);
    }
  }

  // If the variant tensor input is empty, then we have no way to determine
  // the correct shape for the dense_values.  (It must have rank>=1, and its
  // outer dimension must be 0, but we don't know its shape beyond that.)
  // For now, we just use a shape of `[0]` in this case.
  // TODO(edloper): Update this op with an attribute containing information
  // about dense_values shape.  If it's `None`, then we'll probably still have
  // to use shape=[0] here, but if we have more info, then we can use it.
  // E.g., in map_fn, we may have shape info from the RaggedTensorSpec.
  TensorShape component_values_shape;
  if (ragged_components.empty()) {
    component_values_shape = TensorShape({0});
  } else {
    component_values_shape = ragged_components[0].values().shape();
  }

  // Populate values.
  int values_size = component_values_shape.dim_size(0);
  for (int i = 1; i < ragged_components.size(); i++) {
    if (ragged_components[i].values().dims() != component_values_shape.dims()) {
      return errors::InvalidArgument(
          "Rank of values must match for all "
          "components; values shape at index 0: ",
          component_values_shape.DebugString(), ", values shape at index ", i,
          ": ", ragged_components[i].values().shape().DebugString());
    }
    values_size += ragged_components[i].values().shape().dim_size(0);
  }
  component_values_shape.set_dim(0, values_size);
  output_ragged->set_values(
      Tensor(DataTypeToEnum<VALUE_TYPE>::value, component_values_shape));
  auto output_values_flat =
      output_ragged->mutable_values()->flat_outer_dims<VALUE_TYPE, 2>();
  int values_index = 0;
  for (int i = 0; i < ragged_components.size(); i++) {
    auto component_values_flat =
        ragged_components[i].values().flat_outer_dims<VALUE_TYPE, 2>();
    int num_inner_elements = ragged_components[i].values().NumElements();
    if (ragged_components[i].values().dim_size(0) > 0) {
      num_inner_elements /= ragged_components[i].values().dim_size(0);
    }
    for (int j = 0; j < ragged_components[i].values().dim_size(0);
         j++, values_index++) {
      for (int k = 0; k < num_inner_elements; k++) {
        output_values_flat(values_index, k) = component_values_flat(j, k);
      }
    }
  }
  return Status::OK();
}