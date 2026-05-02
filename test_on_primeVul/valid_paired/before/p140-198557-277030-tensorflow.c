inline Status SparseTensor::Split(const SparseTensor& input_tensor,
                                  const int split_dim, const int num_split,
                                  std::vector<SparseTensor>* result) {
  std::vector<Tensor> output_indices;
  std::vector<Tensor> output_values;
  std::vector<TensorShape> output_shapes;
  output_indices.reserve(num_split);
  output_values.reserve(num_split);
  output_shapes.reserve(num_split);

  std::vector<typename TTypes<int64>::Matrix> output_indices_t;
  std::vector<typename TTypes<T>::Vec> output_values_t;
  output_indices_t.reserve(num_split);
  output_values_t.reserve(num_split);
  auto input_values_t = input_tensor.values().vec<T>();
  auto input_indices_t = input_tensor.indices().matrix<int64>();

  std::vector<int> num_values(num_split, 0);
  const int num_dim = input_tensor.shape().size();
  const int split_dim_size = input_tensor.shape()[split_dim];
  const int split_size = split_dim_size / num_split;

  if (!(num_split > 0 && num_split <= split_dim_size)) {
    return errors::InvalidArgument("num_split must be in the interval (0, ",
                                   split_dim_size, "]");
  }
  if (!(split_dim >= 0 && split_dim < num_dim)) {
    return errors::InvalidArgument("num_dim must be in the interval [0, ",
                                   num_dim, ")");
  }

  const int residual = split_dim_size % num_split;
  for (int i = 0; i < input_tensor.indices().dim_size(0); ++i) {
    const int dim = input_tensor.indices().matrix<int64>()(i, split_dim);
    int slice_index = GetSliceIndex(dim, split_size, residual);
    num_values[slice_index]++;
  }

  for (int i = 0; i < num_split; ++i) {
    // TODO(ataei): Pass an allocator to avoid allocating large memory buffer.
    output_indices.emplace_back(DT_INT64,
                                TensorShape({num_values[i], num_dim}));
    output_values.emplace_back(DataTypeToEnum<T>::v(),
                               TensorShape({num_values[i]}));
    output_shapes.emplace_back(input_tensor.shape());
    output_indices_t.emplace_back(output_indices[i].matrix<int64>());
    output_values_t.emplace_back(output_values[i].vec<T>());
    const int size = GetSliceShape(i, split_size, residual);
    output_shapes[i].set_dim(split_dim, size);
  }

  std::vector<int> values_inserted_in_slice(num_split, 0);
  for (int i = 0; i < input_tensor.indices().dim_size(0); ++i) {
    const int dim = input_indices_t(i, split_dim);
    const int slice_index = GetSliceIndex(dim, split_size, residual);
    const int slice_dim = values_inserted_in_slice[slice_index]++;
    output_values_t[slice_index](slice_dim) = input_values_t(i);
    for (int j = 0; j < num_dim; ++j) {
      const int64 original_dim = input_indices_t(i, j);
      output_indices_t[slice_index](slice_dim, j) =
          (j == split_dim)
              ? GetDimensionInSlice(original_dim, split_size, residual)
              : original_dim;
    }
  }

  result->clear();
  result->reserve(num_split);
  for (int i = 0; i < num_split; ++i) {
    SparseTensor tensor;
    Status create_status =
        Create(output_indices[i], output_values[i], output_shapes[i], &tensor);
    if (!create_status.ok()) {
      return create_status;
    }
    result->push_back(std::move(tensor));
  }
  return Status::OK();
}