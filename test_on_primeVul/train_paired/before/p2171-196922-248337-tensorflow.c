  void Compute(OpKernelContext* ctx) final {
    const Tensor& indices = ctx->input(0);
    const Tensor& values = ctx->input(1);
    const Tensor& dense_shape = ctx->input(2);
    const int rank = dense_shape.NumElements();
    OP_REQUIRES(ctx, rank == 2 || rank == 3,
                errors::InvalidArgument("SparseTensor must have rank 2 or 3; ",
                                        "but indices has rank: ", rank));
    auto dense_shape_vec = dense_shape.vec<int64_t>();
    const int64_t batch_size = (rank == 2) ? 1 : dense_shape_vec(0);
    const int64_t num_rows = dense_shape_vec((rank == 2) ? 0 : 1);
    const int64_t total_nnz = values.NumElements();

    // Allocate output Tensors.
    TensorShape batch_ptr_shape;
    OP_REQUIRES_OK(
        ctx, TensorShape::BuildTensorShape({batch_size + 1}, &batch_ptr_shape));
    Tensor batch_ptr(cpu_allocator(), DT_INT32, batch_ptr_shape);
    TensorShape csr_col_ind_shape;
    OP_REQUIRES_OK(
        ctx, TensorShape::BuildTensorShape({total_nnz}, &csr_col_ind_shape));
    Tensor csr_col_ind(cpu_allocator(), DT_INT32, csr_col_ind_shape);
    TensorShape csr_row_ind_shape;
    OP_REQUIRES_OK(ctx, TensorShape::BuildTensorShape(
                            {(num_rows + 1) * batch_size}, &csr_row_ind_shape));
    Tensor csr_row_ptr(cpu_allocator(), DT_INT32, csr_row_ind_shape);

    // Fill the row pointers with zeros.
    functor::SetZeroFunctor<CPUDevice, int32> set_zero;
    set_zero(ctx->eigen_device<CPUDevice>(), csr_row_ptr.flat<int32>());

    // Convert from COO to CSR format.
    functor::SparseTensorToCSRSparseMatrixCPUFunctor coo_to_csr;
    OP_REQUIRES_OK(
        ctx,
        coo_to_csr(batch_size, num_rows, indices.template matrix<int64_t>(),
                   batch_ptr.vec<int32>(), csr_row_ptr.vec<int32>(),
                   csr_col_ind.vec<int32>()));

    // Create the CSRSparseMatrix object from its component Tensors and prepare
    // the Variant output Tensor.
    CSRSparseMatrix output_csr_matrix;
    OP_REQUIRES_OK(
        ctx, CSRSparseMatrix::CreateCSRSparseMatrix(
                 DataTypeToEnum<T>::value, dense_shape, batch_ptr, csr_row_ptr,
                 csr_col_ind, values, &output_csr_matrix));
    Tensor* output_csr_matrix_tensor;
    AllocatorAttributes cpu_alloc;
    cpu_alloc.set_on_host(true);
    OP_REQUIRES_OK(
        ctx, ctx->allocate_output(0, TensorShape({}), &output_csr_matrix_tensor,
                                  cpu_alloc));
    output_csr_matrix_tensor->scalar<Variant>()() =
        std::move(output_csr_matrix);
  }