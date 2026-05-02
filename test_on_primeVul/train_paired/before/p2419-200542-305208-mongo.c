Status renameCollectionForApplyOps(OperationContext* opCtx,
                                   const std::string& dbName,
                                   const BSONElement& ui,
                                   const BSONObj& cmd,
                                   const repl::OpTime& renameOpTime) {

    const auto sourceNsElt = cmd.firstElement();
    const auto targetNsElt = cmd["to"];
    uassert(ErrorCodes::TypeMismatch,
            "'renameCollection' must be of type String",
            sourceNsElt.type() == BSONType::String);
    uassert(ErrorCodes::TypeMismatch,
            "'to' must be of type String",
            targetNsElt.type() == BSONType::String);

    NamespaceString sourceNss(sourceNsElt.valueStringData());
    NamespaceString targetNss(targetNsElt.valueStringData());
    NamespaceString uiNss(getNamespaceFromUUIDElement(opCtx, ui));

    if ((repl::ReplicationCoordinator::get(opCtx)->getReplicationMode() ==
         repl::ReplicationCoordinator::modeNone) &&
        targetNss.isOplog()) {
        return Status(ErrorCodes::IllegalOperation,
                      str::stream() << "Cannot rename collection to the oplog");
    }

    // If the UUID we're targeting already exists, rename from there no matter what.
    if (!uiNss.isEmpty()) {
        sourceNss = uiNss;
    }

    OptionalCollectionUUID targetUUID;
    if (!ui.eoo())
        targetUUID = uassertStatusOK(UUID::parse(ui));

    RenameCollectionOptions options;
    options.dropTarget = cmd["dropTarget"].trueValue();
    if (cmd["dropTarget"].type() == BinData) {
        auto uuid = uassertStatusOK(UUID::parse(cmd["dropTarget"]));
        options.dropTargetUUID = uuid;
    }

    const Collection* const sourceColl =
        AutoGetCollectionForRead(opCtx, sourceNss, AutoGetCollection::ViewMode::kViewsPermitted)
            .getCollection();

    if (sourceNss.isDropPendingNamespace() || sourceColl == nullptr) {
        NamespaceString dropTargetNss;

        if (options.dropTarget)
            dropTargetNss = targetNss;

        if (options.dropTargetUUID) {
            dropTargetNss = getNamespaceFromUUID(opCtx, options.dropTargetUUID.get());
        }

        // Downgrade renameCollection to dropCollection.
        if (!dropTargetNss.isEmpty()) {
            BSONObjBuilder unusedResult;
            return dropCollection(opCtx,
                                  dropTargetNss,
                                  unusedResult,
                                  renameOpTime,
                                  DropCollectionSystemCollectionMode::kAllowSystemCollectionDrops);
        }

        return Status(ErrorCodes::NamespaceNotFound,
                      str::stream()
                          << "renameCollection() cannot accept a source "
                             "collection that does not exist or is in a drop-pending state: "
                          << sourceNss.toString());
    }

    const std::string dropTargetMsg =
        options.dropTargetUUID ? " and drop " + options.dropTargetUUID->toString() + "." : ".";
    const std::string uuidString = targetUUID ? targetUUID->toString() : "UUID unknown";
    log() << "renameCollectionForApplyOps: rename " << sourceNss << " (" << uuidString << ") to "
          << targetNss << dropTargetMsg;

    options.stayTemp = cmd["stayTemp"].trueValue();
    return renameCollectionCommon(opCtx, sourceNss, targetNss, targetUUID, renameOpTime, options);
}