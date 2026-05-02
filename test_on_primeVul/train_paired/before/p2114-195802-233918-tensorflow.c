  explicit AsStringOp(OpKernelConstruction* ctx) : OpKernel(ctx) {
    int32 precision;
    bool scientific;
    bool shortest;
    int32 width;
    string fill_string;
    DataType dtype;
    OP_REQUIRES_OK(ctx, ctx->GetAttr("T", &dtype));
    OP_REQUIRES_OK(ctx, ctx->GetAttr("precision", &precision));
    OP_REQUIRES_OK(ctx, ctx->GetAttr("scientific", &scientific));
    OP_REQUIRES_OK(ctx, ctx->GetAttr("shortest", &shortest));
    OP_REQUIRES_OK(ctx, ctx->GetAttr("width", &width));
    OP_REQUIRES_OK(ctx, ctx->GetAttr("fill", &fill_string));
    switch (dtype) {
      case DT_FLOAT:
      case DT_DOUBLE:
      case DT_COMPLEX64:
      case DT_COMPLEX128:
        break;
      default:
        OP_REQUIRES(ctx, !(scientific || shortest),
                    errors::InvalidArgument("scientific and shortest format "
                                            "not supported for datatype ",
                                            DataTypeString(dtype)));
        OP_REQUIRES(ctx, precision < 0,
                    errors::InvalidArgument("precision not supported "
                                            "for datatype ",
                                            DataTypeString(dtype)));
    }
    OP_REQUIRES(
        ctx, fill_string.size() <= 1,
        errors::InvalidArgument("Fill string must be one or fewer characters"));
    OP_REQUIRES(ctx, !(scientific && shortest),
                errors::InvalidArgument(
                    "Cannot select both scientific and shortest notation"));
    format_ = "%";
    if (width > -1) {
      strings::Appendf(&format_, "%s%d", fill_string.c_str(), width);
    }
    if (precision > -1) {
      strings::Appendf(&format_, ".%d", precision);
    }
    switch (dtype) {
      case DT_INT8:
      case DT_INT16:
      case DT_INT32:
        strings::Appendf(&format_, "d");
        break;
      case DT_INT64:
        strings::Appendf(&format_, "lld");
        break;
      case DT_FLOAT:
      case DT_DOUBLE:
      case DT_COMPLEX64:
      case DT_COMPLEX128:
        if (shortest) {
          strings::Appendf(&format_, "g");
        } else if (scientific) {
          strings::Appendf(&format_, "e");
        } else {
          strings::Appendf(&format_, "f");
        }
        break;
      case DT_BOOL:
        break;
      default:
        bool type_not_supported = true;
        OP_REQUIRES(ctx, !type_not_supported,
                    errors::InvalidArgument("Type not supported: ",
                                            DataTypeString(dtype)));
    }

    if (dtype == DT_COMPLEX64 || dtype == DT_COMPLEX128) {
      format_ = strings::Printf("(%s,%s)", format_.c_str(), format_.c_str());
    }
  }