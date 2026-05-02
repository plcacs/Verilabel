bool WddxPacket::recursiveAddVar(const String& varName,
                                 const Variant& varVariant,
                                 bool hasVarTag) {

  bool isArray = varVariant.isArray();
  bool isObject = varVariant.isObject();

  if (isArray || isObject) {
    if (hasVarTag) {
      m_packetString += "<var name='";
      m_packetString += varName.data();
      m_packetString += "'>";
    }

    Array varAsArray;
    Object varAsObject = varVariant.toObject();
    if (isArray) varAsArray = varVariant.toArray();
    if (isObject) varAsArray = varAsObject.toArray();

    int length = varAsArray.length();
    if (length > 0) {
      ArrayIter it = ArrayIter(varAsArray);
      if (it.first().isString()) isObject = true;
      if (isObject) {
        m_packetString += "<struct>";
        if (!isArray) {
          m_packetString += "<var name='php_class_name'><string>";
          m_packetString += varAsObject->o_getClassName().c_str();
          m_packetString += "</string></var>";
        }
      } else {
        m_packetString += "<array length='";
        m_packetString += std::to_string(length);
        m_packetString += "'>";
      }
      for (ArrayIter it(varAsArray); it; ++it) {
        Variant key = it.first();
        Variant value = it.second();
        recursiveAddVar(key.toString(), value, isObject);
      }
      if (isObject) {
        m_packetString += "</struct>";
      }
      else {
        m_packetString += "</array>";
      }
    }
    else {
      //empty object
      if (isObject) {
        m_packetString += "<struct>";
        if (!isArray) {
          m_packetString += "<var name='php_class_name'><string>";
          m_packetString += varAsObject->o_getClassName().c_str();
          m_packetString += "</string></var>";
        }
        m_packetString += "</struct>";
      }
    }
    if (hasVarTag) {
      m_packetString += "</var>";
    }
    return true;
  }

  std::string varType = getDataTypeString(varVariant.getType()).data();
  if (!getWddxEncoded(varType, "", varName, false).empty()) {
    std::string varValue = varVariant.toString().data();
    if (varType.compare("boolean") == 0) {
      varValue = varVariant.toBoolean() ? "true" : "false";
    }
    m_packetString += getWddxEncoded(varType, varValue, varName, hasVarTag);
    return true;
  }

  return false;
}