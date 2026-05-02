  void Compute(OpKernelContext* ctx) override {
    Buffer* buf = nullptr;
    OP_REQUIRES_OK(ctx, GetBuffer(ctx, def(), &buf));
    core::ScopedUnref scope(buf);
    Buffer::Tuple tuple;

    std::size_t index = ctx->input(0).scalar<int>()();

    OP_REQUIRES_OK(ctx, buf->Peek(index, &tuple));

    OP_REQUIRES(
        ctx, tuple.size() == (size_t)ctx->num_outputs(),
        errors::InvalidArgument("Mismatch stage/unstage: ", tuple.size(),
                                " vs. ", ctx->num_outputs()));

    for (size_t i = 0; i < tuple.size(); ++i) {
      ctx->set_output(i, tuple[i]);
    }
  }