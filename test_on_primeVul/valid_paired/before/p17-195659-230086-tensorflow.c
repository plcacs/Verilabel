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
        TF_RETURN_IF_ERROR(BuildRaggedFeatureReader(
            ragged_values_list[next_ragged], ragged_splits_list[next_ragged],
            features));
        next_ragged++;
      } else if (c == 'S') {
        TF_RETURN_IF_ERROR(BuildSparseFeatureReader(
            sparse_indices_list[next_sparse], sparse_values_list[next_sparse],
            batch_size, features));
        next_sparse++;
      } else if (c == 'D') {
        TF_RETURN_IF_ERROR(
            BuildDenseFeatureReader(dense_list[next_dense++], features));
      } else {
        return errors::InvalidArgument("Unexpected input_order value.");
      }
    }

    return Status::OK();
  }