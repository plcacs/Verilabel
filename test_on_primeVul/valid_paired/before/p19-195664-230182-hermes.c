CallResult<PseudoHandle<>> JSObject::getComputedWithReceiver_RJS(
    Handle<JSObject> selfHandle,
    Runtime *runtime,
    Handle<> nameValHandle,
    Handle<> receiver) {
  // Try the fast-path first: no "index-like" properties and the "name" already
  // is a valid integer index.
  if (selfHandle->flags_.fastIndexProperties) {
    if (auto arrayIndex = toArrayIndexFastPath(*nameValHandle)) {
      // Do we have this value present in our array storage? If so, return it.
      PseudoHandle<> ourValue = createPseudoHandle(
          getOwnIndexed(selfHandle.get(), runtime, *arrayIndex));
      if (LLVM_LIKELY(!ourValue->isEmpty()))
        return ourValue;
    }
  }

  // If nameValHandle is an object, we should convert it to string now,
  // because toString may have side-effect, and we want to do this only
  // once.
  auto converted = toPropertyKeyIfObject(runtime, nameValHandle);
  if (LLVM_UNLIKELY(converted == ExecutionStatus::EXCEPTION)) {
    return ExecutionStatus::EXCEPTION;
  }
  auto nameValPrimitiveHandle = *converted;

  ComputedPropertyDescriptor desc;

  // Locate the descriptor. propObj contains the object which may be anywhere
  // along the prototype chain.
  MutableHandle<JSObject> propObj{runtime};
  if (LLVM_UNLIKELY(
          getComputedPrimitiveDescriptor(
              selfHandle, runtime, nameValPrimitiveHandle, propObj, desc) ==
          ExecutionStatus::EXCEPTION)) {
    return ExecutionStatus::EXCEPTION;
  }

  if (!propObj)
    return createPseudoHandle(HermesValue::encodeUndefinedValue());

  if (LLVM_LIKELY(
          !desc.flags.accessor && !desc.flags.hostObject &&
          !desc.flags.proxyObject))
    return createPseudoHandle(
        getComputedSlotValue(propObj.get(), runtime, desc));

  if (desc.flags.accessor) {
    auto *accessor = vmcast<PropertyAccessor>(
        getComputedSlotValue(propObj.get(), runtime, desc));
    if (!accessor->getter)
      return createPseudoHandle(HermesValue::encodeUndefinedValue());

    // Execute the accessor on this object.
    return accessor->getter.get(runtime)->executeCall0(
        runtime->makeHandle(accessor->getter), runtime, receiver);
  } else if (desc.flags.hostObject) {
    SymbolID id{};
    LAZY_TO_IDENTIFIER(runtime, nameValPrimitiveHandle, id);
    auto propRes = vmcast<HostObject>(selfHandle.get())->get(id);
    if (propRes == ExecutionStatus::EXCEPTION)
      return ExecutionStatus::EXCEPTION;
    return createPseudoHandle(*propRes);
  } else {
    assert(desc.flags.proxyObject && "descriptor flags are impossible");
    CallResult<Handle<>> key = toPropertyKey(runtime, nameValPrimitiveHandle);
    if (key == ExecutionStatus::EXCEPTION)
      return ExecutionStatus::EXCEPTION;
    return JSProxy::getComputed(propObj, runtime, *key, receiver);
  }
}