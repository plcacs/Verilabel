void skip(Protocol_& prot, TType arg_type) {
  switch (arg_type) {
    case TType::T_BOOL: {
      bool boolv;
      prot.readBool(boolv);
      return;
    }
    case TType::T_BYTE: {
      int8_t bytev;
      prot.readByte(bytev);
      return;
    }
    case TType::T_I16: {
      int16_t i16;
      prot.readI16(i16);
      return;
    }
    case TType::T_I32: {
      int32_t i32;
      prot.readI32(i32);
      return;
    }
    case TType::T_I64: {
      int64_t i64;
      prot.readI64(i64);
      return;
    }
    case TType::T_DOUBLE: {
      double dub;
      prot.readDouble(dub);
      return;
    }
    case TType::T_FLOAT: {
      float flt;
      prot.readFloat(flt);
      return;
    }
    case TType::T_STRING: {
      std::string str;
      prot.readBinary(str);
      return;
    }
    case TType::T_STRUCT: {
      std::string name;
      int16_t fid;
      TType ftype;
      prot.readStructBegin(name);
      while (true) {
        prot.readFieldBegin(name, ftype, fid);
        if (ftype == TType::T_STOP) {
          break;
        }
        apache::thrift::skip(prot, ftype);
        prot.readFieldEnd();
      }
      prot.readStructEnd();
      return;
    }
    case TType::T_MAP: {
      TType keyType;
      TType valType;
      uint32_t i, size;
      prot.readMapBegin(keyType, valType, size);
      for (i = 0; i < size; i++) {
        apache::thrift::skip(prot, keyType);
        apache::thrift::skip(prot, valType);
      }
      prot.readMapEnd();
      return;
    }
    case TType::T_SET: {
      TType elemType;
      uint32_t i, size;
      prot.readSetBegin(elemType, size);
      for (i = 0; i < size; i++) {
        apache::thrift::skip(prot, elemType);
      }
      prot.readSetEnd();
      return;
    }
    case TType::T_LIST: {
      TType elemType;
      uint32_t i, size;
      prot.readListBegin(elemType, size);
      for (i = 0; i < size; i++) {
        apache::thrift::skip(prot, elemType);
      }
      prot.readListEnd();
      return;
    }
    default:
      return;
  }
}