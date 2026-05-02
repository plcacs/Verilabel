  Status BuildFeatureReaders(const OpInputList& ragged_values_list,
                             const OpInputList& ragged_splits_list,
                             const OpInputList& sparse_indices_list,
                             const OpInputList& sparse_values_list,
                             const OpInputList& dense_list, int64 batch_size,
                             FeatureReaders* features) {
    features->reserve(input_order_.size());

    int next_ragged = 0;
    int next_sparse = 0;
    int next_dense = 0;
    for (char c : input_order_) {
      if (c == 'R') {
        if (next_ragged >= ragged_values_list.size())
          return errors::InvalidArgument(
              "input_order \"", input_order_,
              "\" specifies reading a ragged tensor value at index ",
              next_ragged, " from a list of ", ragged_values_list.size(),
              " values.");
        if (next_ragged >= ragged_splits_list.size())
          return errors::InvalidArgument(
              "input_order \"", input_order_,
              "\" specifies reading a ragged tensor split at index ",
              next_ragged, " from a list of ", ragged_splits_list.size(),
              " splits.");
        TF_RETURN_IF_ERROR(BuildRaggedFeatureReader(
            ragged_values_list[next_ragged], ragged_splits_list[next_ragged],
            features));
        next_ragged++;
      } else if (c == 'S') {
        if (next_sparse >= sparse_values_list.size())
          return errors::InvalidArgument(
              "input_order \"", input_order_,
              "\" specifies reading a sparse tensor value at index ",
              next_sparse, " from a list of ", sparse_values_list.size(),
              " values.");
        if (next_sparse >= sparse_indices_list.size())
          return errors::InvalidArgument(
              "input_order \"", input_order_,
              "\" specifies reading a sparse tensor index at index ",
              next_sparse, " from a list of ", sparse_indices_list.size(),
              " indices.");
        TF_RETURN_IF_ERROR(BuildSparseFeatureReader(
            sparse_indices_list[next_sparse], sparse_values_list[next_sparse],
            batch_size, features));
        next_sparse++;
      } else if (c == 'D') {
        if (next_dense >= dense_list.size())
          return errors::InvalidArgument(
              "input_order \"", input_order_,
              "\" specifies reading a dense tensor at index ", next_dense,
              " from a list of ", dense_list.size(), " tensors.");
        TF_RETURN_IF_ERROR(
            BuildDenseFeatureReader(dense_list[next_dense++], features));
      } else {
        return errors::InvalidArgument("Unexpected input_order value.");
      }
    }

    return Status::OK();
  }