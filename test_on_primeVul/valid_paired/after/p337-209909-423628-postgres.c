ExecAlterObjectDependsStmt(AlterObjectDependsStmt *stmt, ObjectAddress *refAddress)
{
	ObjectAddress address;
	ObjectAddress refAddr;
	Relation	rel;

	address =
		get_object_address_rv(stmt->objectType, stmt->relation, (List *) stmt->object,
							  &rel, AccessExclusiveLock, false);

	/*
	 * Verify that the user is entitled to run the command.
	 *
	 * We don't check any privileges on the extension, because that's not
	 * needed.  The object owner is stipulating, by running this command, that
	 * the extension owner can drop the object whenever they feel like it,
	 * which is not considered a problem.
	 */
	check_object_ownership(GetUserId(),
						   stmt->objectType, address, stmt->object, rel);

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