bool RepeatedAttrDefEqual(
    const protobuf::RepeatedPtrField<OpDef::AttrDef>& a1,
    const protobuf::RepeatedPtrField<OpDef::AttrDef>& a2) {
  std::unordered_map<string, const OpDef::AttrDef*> a1_set;
  for (const OpDef::AttrDef& def : a1) {
    if (a1_set.find(def.name()) != a1_set.end()) {
      LOG(ERROR) << "AttrDef names must be unique, but '" << def.name()
                 << "' appears more than once";
    }
    a1_set[def.name()] = &def;
  }
  for (const OpDef::AttrDef& def : a2) {
    auto iter = a1_set.find(def.name());
    if (iter == a1_set.end()) return false;
    if (!AttrDefEqual(*iter->second, def)) return false;
    a1_set.erase(iter);
  }
  if (!a1_set.empty()) return false;
  return true;
}