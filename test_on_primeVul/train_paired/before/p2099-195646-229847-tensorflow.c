  Status DoCompute(OpKernelContext* ctx) {
    tensorflow::ResourceTagger tag(kTFDataResourceTag,
                                   ctx->op_kernel().type_string());
    tstring filename;
    TF_RETURN_IF_ERROR(
        ParseScalarArgument<tstring>(ctx, "filename", &filename));
    tstring compression_type;
    TF_RETURN_IF_ERROR(ParseScalarArgument<tstring>(ctx, "compression_type",
                                                    &compression_type));
    std::unique_ptr<WritableFile> file;
    TF_RETURN_IF_ERROR(ctx->env()->NewWritableFile(filename, &file));
    auto writer = absl::make_unique<io::RecordWriter>(
        file.get(),
        io::RecordWriterOptions::CreateRecordWriterOptions(compression_type));

    DatasetBase* dataset;
    TF_RETURN_IF_ERROR(GetDatasetFromVariantTensor(ctx->input(0), &dataset));

    IteratorContext::Params params(ctx);
    FunctionHandleCache function_handle_cache(params.flr);
    params.function_handle_cache = &function_handle_cache;
    ResourceMgr resource_mgr;
    params.resource_mgr = &resource_mgr;
    CancellationManager cancellation_manager(ctx->cancellation_manager());
    params.cancellation_manager = &cancellation_manager;

    IteratorContext iter_ctx(std::move(params));
    DatasetBase* finalized_dataset;
    TF_RETURN_IF_ERROR(FinalizeDataset(ctx, dataset, &finalized_dataset));

    std::unique_ptr<IteratorBase> iterator;
    TF_RETURN_IF_ERROR(finalized_dataset->MakeIterator(
        &iter_ctx, /*parent=*/nullptr, "ToTFRecordOpIterator", &iterator));

    std::vector<Tensor> components;
    components.reserve(finalized_dataset->output_dtypes().size());
    bool end_of_sequence;
    do {
      TF_RETURN_IF_ERROR(
          iterator->GetNext(&iter_ctx, &components, &end_of_sequence));

      if (!end_of_sequence) {
        TF_RETURN_IF_ERROR(
            writer->WriteRecord(components[0].scalar<tstring>()()));
      }
      components.clear();
    } while (!end_of_sequence);
    return Status::OK();
  }