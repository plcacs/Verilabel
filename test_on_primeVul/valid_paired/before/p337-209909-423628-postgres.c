ExecAlterObjectDependsStmt(AlterObjectDependsStmt *stmt, ObjectAddress *refAddress)
{
	ObjectAddress address;
	ObjectAddress refAddr;
	Relation	rel;

	address =
		get_object_address_rv(stmt->objectType, stmt->relation, (List *) stmt->object,
							  &rel, AccessExclusiveLock, false);

	/*
	 * If a relation was involved, it would have been opened and locked. We
	 * don't need the relation here, but we'll retain the lock until commit.
	 */
	if (rel)
		table_close(rel, NoLock);

	refAddr = get_object_address(OBJECT_EXTENSION, (Node *) stmt->extname,
								 &rel, AccessExclusiveLock, false);
	Assert(rel == NULL);
	if (refAddress)
		*refAddress = refAddr;

	recordDependencyOn(&address, &refAddr, DEPENDENCY_AUTO_EXTENSION);

	return address;
}