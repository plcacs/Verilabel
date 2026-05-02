uint32_t skip(Protocol_& prot, TType arg_type) {
  switch (arg_type) {
    case T_BOOL: {
      bool boolv;
      return prot.readBool(boolv);
    }
    case T_BYTE: {
      int8_t bytev = 0;
      return prot.readByte(bytev);
    }
    case T_I16: {
      int16_t i16;
      return prot.readI16(i16);
    }
    case T_I32: {
      int32_t i32;
      return prot.readI32(i32);
    }
    case T_I64: {
      int64_t i64;
      return prot.readI64(i64);
    }
    case T_DOUBLE: {
      double dub;
      return prot.readDouble(dub);
    }
    case T_FLOAT: {
      float flt;
      return prot.readFloat(flt);
    }
    case T_STRING: {
      std::string str;
      return prot.readBinary(str);
    }
    case T_STRUCT: {
      uint32_t result = 0;
      std::string name;
      int16_t fid;
      TType ftype;
      result += prot.readStructBegin(name);
      while (true) {
        result += prot.readFieldBegin(name, ftype, fid);
        if (ftype == T_STOP) {
          break;
        }
        result += skip(prot, ftype);
        result += prot.readFieldEnd();
      }
      result += prot.readStructEnd();
      return result;
    }
    case T_MAP: {
      uint32_t result = 0;
      TType keyType;
      TType valType;
      uint32_t i, size;
      bool sizeUnknown;
      result += prot.readMapBegin(keyType, valType, size, sizeUnknown);
      if (!sizeUnknown) {
        for (i = 0; i < size; i++) {
          result += skip(prot, keyType);
          result += skip(prot, valType);
        }
      } else {
        while (prot.peekMap()) {
          result += skip(prot, keyType);
          result += skip(prot, valType);
        }
      }
      result += prot.readMapEnd();
      return result;
    }
    case T_SET: {
      uint32_t result = 0;
      TType elemType;
      uint32_t i, size;
      bool sizeUnknown;
      result += prot.readSetBegin(elemType, size, sizeUnknown);
      if (!sizeUnknown) {
        for (i = 0; i < size; i++) {
          result += skip(prot, elemType);
        }
      } else {
        while (prot.peekSet()) {
          result += skip(prot, elemType);
        }
      }
      result += prot.readSetEnd();
      return result;
    }
    case T_LIST: {
      uint32_t result = 0;
      TType elemType;
      uint32_t i, size;
      bool sizeUnknown;
      result += prot.readListBegin(elemType, size, sizeUnknown);
      if (!sizeUnknown) {
        for (i = 0; i < size; i++) {
          result += skip(prot, elemType);
        }
      } else {
        while (prot.peekList()) {
          result += skip(prot, elemType);
        }
      }
      result += prot.readListEnd();
      return result;
    }
    default:
      return 0;
  }
}