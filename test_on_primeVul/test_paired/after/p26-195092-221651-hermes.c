Literal *hermes::evalUnaryOperator(
    UnaryOperatorInst::OpKind kind,
    IRBuilder &builder,
    Literal *operand) {
  switch (kind) {
    case UnaryOperatorInst::OpKind::MinusKind:
      // Negate constant integers.
      switch (operand->getKind()) {
        case ValueKind::LiteralNumberKind:
          if (auto *literalNum = llvh::dyn_cast<LiteralNumber>(operand)) {
            auto V = -literalNum->getValue();
            return builder.getLiteralNumber(V);
          }
          break;
        case ValueKind::LiteralUndefinedKind:
          return builder.getLiteralNaN();
        case ValueKind::LiteralBoolKind:
          if (evalIsTrue(builder, operand)) {
            return builder.getLiteralNumber(-1);
          } else { // evalIsFalse(operand)
            return builder.getLiteralNegativeZero();
          }
        case ValueKind::LiteralNullKind:
          return builder.getLiteralNegativeZero();
        default:
          break;
      }
      break;
    case UnaryOperatorInst::OpKind::TypeofKind:
      switch (operand->getKind()) {
        case ValueKind::GlobalObjectKind:
        case ValueKind::LiteralNullKind:
          return builder.getLiteralString("object");
        case ValueKind::LiteralUndefinedKind:
          return builder.getLiteralString("undefined");
        case ValueKind::LiteralBoolKind:
          return builder.getLiteralString("boolean");
        case ValueKind::LiteralNumberKind:
          return builder.getLiteralString("number");
        case ValueKind::LiteralStringKind:
          return builder.getLiteralString("string");
        default:
          break;
      }
      break;

    case UnaryOperatorInst::OpKind::BangKind:
      if (evalIsTrue(builder, operand)) {
        return builder.getLiteralBool(false);
      }
      if (evalIsFalse(builder, operand)) {
        return builder.getLiteralBool(true);
      }
      break;

    case UnaryOperatorInst::OpKind::VoidKind:
      return builder.getLiteralUndefined();

    default:
      break;
  }

  return nullptr;
}