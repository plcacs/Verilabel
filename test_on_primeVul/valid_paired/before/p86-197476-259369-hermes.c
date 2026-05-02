CallResult<bool> JSObject::putComputedWithReceiver_RJS(
    Handle<JSObject> selfHandle,
    Runtime *runtime,
    Handle<> nameValHandle,
    Handle<> valueHandle,
    Handle<> receiver,
    PropOpFlags opFlags) {
  assert(
      !opFlags.getMustExist() &&
      "mustExist flag cannot be used with computed properties");

  // Try the fast-path first: has "index-like" properties, the "name"
  // already is a valid integer index, selfHandle and receiver are the
  // same, and it is present in storage.
  if (selfHandle->flags_.fastIndexProperties) {
    if (auto arrayIndex = toArrayIndexFastPath(*nameValHandle)) {
      if (selfHandle.getHermesValue().getRaw() == receiver->getRaw()) {
        if (haveOwnIndexed(selfHandle.get(), runtime, *arrayIndex)) {
          auto result =
              setOwnIndexed(selfHandle, runtime, *arrayIndex, valueHandle);
          if (LLVM_UNLIKELY(result == ExecutionStatus::EXCEPTION))
            return ExecutionStatus::EXCEPTION;
          if (LLVM_LIKELY(*result))
            return true;
          if (opFlags.getThrowOnError()) {
            // TODO: better message.
            return runtime->raiseTypeError(
                "Cannot assign to read-only property");
          }
          return false;
        }
      }
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

  // Look for the property in this object or along the prototype chain.
  MutableHandle<JSObject> propObj{runtime};
  if (LLVM_UNLIKELY(
          getComputedPrimitiveDescriptor(
              selfHandle, runtime, nameValPrimitiveHandle, propObj, desc) ==
          ExecutionStatus::EXCEPTION)) {
    return ExecutionStatus::EXCEPTION;
  }

  // If the property exists (or, we hit a proxy/hostobject on the way
  // up the chain)
  if (propObj) {
    // Get the simple case out of the way: If the property already
    // exists on selfHandle, is not an accessor, selfHandle and
    // receiver are the same, selfHandle is not a host
    // object/proxy/internal setter, and the property is writable,
    // just write into the same slot.

    if (LLVM_LIKELY(
            selfHandle == propObj &&
            selfHandle.getHermesValue().getRaw() == receiver->getRaw() &&
            !desc.flags.accessor && !desc.flags.internalSetter &&
            !desc.flags.hostObject && !desc.flags.proxyObject &&
            desc.flags.writable)) {
      if (LLVM_UNLIKELY(
              setComputedSlotValue(selfHandle, runtime, desc, valueHandle) ==
              ExecutionStatus::EXCEPTION)) {
        return ExecutionStatus::EXCEPTION;
      }
      return true;
    }

    // Is it an accessor?
    if (LLVM_UNLIKELY(desc.flags.accessor)) {
      auto *accessor = vmcast<PropertyAccessor>(
          getComputedSlotValue(propObj.get(), runtime, desc));

      // If it is a read-only accessor, fail.
      if (!accessor->setter) {
        if (opFlags.getThrowOnError()) {
          return runtime->raiseTypeErrorForValue(
              "Cannot assign to property ",
              nameValPrimitiveHandle,
              " which has only a getter");
        }
        return false;
      }

      // Execute the accessor on this object.
      if (accessor->setter.get(runtime)->executeCall1(
              runtime->makeHandle(accessor->setter),
              runtime,
              receiver,
              valueHandle.get()) == ExecutionStatus::EXCEPTION) {
        return ExecutionStatus::EXCEPTION;
      }
      return true;
    }

    if (LLVM_UNLIKELY(desc.flags.proxyObject)) {
      assert(
          !opFlags.getMustExist() &&
          "MustExist cannot be used with Proxy objects");
      CallResult<Handle<>> key = toPropertyKey(runtime, nameValPrimitiveHandle);
      if (key == ExecutionStatus::EXCEPTION)
        return ExecutionStatus::EXCEPTION;
      CallResult<bool> setRes =
          JSProxy::setComputed(propObj, runtime, *key, valueHandle, receiver);
      if (LLVM_UNLIKELY(setRes == ExecutionStatus::EXCEPTION)) {
        return ExecutionStatus::EXCEPTION;
      }
      if (!*setRes && opFlags.getThrowOnError()) {
        // TODO: better message.
        return runtime->raiseTypeError(
            TwineChar16("Proxy trap returned false for property"));
      }
      return setRes;
    }

    if (LLVM_UNLIKELY(!desc.flags.writable)) {
      if (desc.flags.staticBuiltin) {
        SymbolID id{};
        LAZY_TO_IDENTIFIER(runtime, nameValPrimitiveHandle, id);
        return raiseErrorForOverridingStaticBuiltin(
            selfHandle, runtime, runtime->makeHandle(id));
      }
      if (opFlags.getThrowOnError()) {
        return runtime->raiseTypeErrorForValue(
            "Cannot assign to read-only property ", nameValPrimitiveHandle, "");
      }
      return false;
    }

    if (selfHandle == propObj && desc.flags.internalSetter) {
      SymbolID id{};
      LAZY_TO_IDENTIFIER(runtime, nameValPrimitiveHandle, id);
      return internalSetter(
          selfHandle,
          runtime,
          id,
          desc.castToNamedPropertyDescriptorRef(),
          valueHandle,
          opFlags);
    }
  }

  // The property does not exist as an conventional own property on
  // this object.

  MutableHandle<JSObject> receiverHandle{runtime, *selfHandle};
  if (selfHandle.getHermesValue().getRaw() != receiver->getRaw() ||
      receiverHandle->isHostObject() || receiverHandle->isProxyObject()) {
    if (selfHandle.getHermesValue().getRaw() != receiver->getRaw()) {
      receiverHandle = dyn_vmcast<JSObject>(*receiver);
    }
    if (!receiverHandle) {
      return false;
    }
    CallResult<bool> descDefinedRes = getOwnComputedPrimitiveDescriptor(
        receiverHandle, runtime, nameValPrimitiveHandle, IgnoreProxy::No, desc);
    if (LLVM_UNLIKELY(descDefinedRes == ExecutionStatus::EXCEPTION)) {
      return ExecutionStatus::EXCEPTION;
    }
    DefinePropertyFlags dpf;
    if (*descDefinedRes) {
      if (LLVM_UNLIKELY(desc.flags.accessor || !desc.flags.writable)) {
        return false;
      }

      if (LLVM_LIKELY(
              !desc.flags.internalSetter && !receiverHandle->isHostObject() &&
              !receiverHandle->isProxyObject())) {
        if (LLVM_UNLIKELY(
                setComputedSlotValue(
                    receiverHandle, runtime, desc, valueHandle) ==
                ExecutionStatus::EXCEPTION)) {
          return ExecutionStatus::EXCEPTION;
        }
        return true;
      }
    }

    if (LLVM_UNLIKELY(
            desc.flags.internalSetter || receiverHandle->isHostObject() ||
            receiverHandle->isProxyObject())) {
      SymbolID id{};
      LAZY_TO_IDENTIFIER(runtime, nameValPrimitiveHandle, id);
      if (desc.flags.internalSetter) {
        return internalSetter(
            receiverHandle,
            runtime,
            id,
            desc.castToNamedPropertyDescriptorRef(),
            valueHandle,
            opFlags);
      } else if (receiverHandle->isHostObject()) {
        return vmcast<HostObject>(receiverHandle.get())->set(id, *valueHandle);
      }
      assert(
          receiverHandle->isProxyObject() && "descriptor flags are impossible");
      if (*descDefinedRes) {
        dpf.setValue = 1;
      } else {
        dpf = DefinePropertyFlags::getDefaultNewPropertyFlags();
      }
      return JSProxy::defineOwnProperty(
          receiverHandle,
          runtime,
          nameValPrimitiveHandle,
          dpf,
          valueHandle,
          opFlags);
    }
  }

  /// Can we add more properties?
  if (LLVM_UNLIKELY(!receiverHandle->isExtensible())) {
    if (opFlags.getThrowOnError()) {
      return runtime->raiseTypeError(
          "cannot add a new property"); // TODO: better message.
    }
    return false;
  }

  // If we have indexed storage we must check whether the property is an index,
  // and if it is, store it in indexed storage.
  if (receiverHandle->flags_.indexedStorage) {
    OptValue<uint32_t> arrayIndex;
    MutableHandle<StringPrimitive> strPrim{runtime};
    TO_ARRAY_INDEX(runtime, nameValPrimitiveHandle, strPrim, arrayIndex);
    if (arrayIndex) {
      // Check whether we need to update array's ".length" property.
      if (auto *array = dyn_vmcast<JSArray>(receiverHandle.get())) {
        if (LLVM_UNLIKELY(*arrayIndex >= JSArray::getLength(array))) {
          auto cr = putNamed_RJS(
              receiverHandle,
              runtime,
              Predefined::getSymbolID(Predefined::length),
              runtime->makeHandle(
                  HermesValue::encodeNumberValue(*arrayIndex + 1)),
              opFlags);
          if (LLVM_UNLIKELY(cr == ExecutionStatus::EXCEPTION))
            return ExecutionStatus::EXCEPTION;
          if (LLVM_UNLIKELY(!*cr))
            return false;
        }
      }

      auto result =
          setOwnIndexed(receiverHandle, runtime, *arrayIndex, valueHandle);
      if (LLVM_UNLIKELY(result == ExecutionStatus::EXCEPTION))
        return ExecutionStatus::EXCEPTION;
      if (LLVM_LIKELY(*result))
        return true;

      if (opFlags.getThrowOnError()) {
        // TODO: better message.
        return runtime->raiseTypeError("Cannot assign to read-only property");
      }
      return false;
    }
  }

  SymbolID id{};
  LAZY_TO_IDENTIFIER(runtime, nameValPrimitiveHandle, id);

  // Add a new named property.
  return addOwnProperty(
      receiverHandle,
      runtime,
      id,
      DefinePropertyFlags::getDefaultNewPropertyFlags(),
      valueHandle,
      opFlags);
}