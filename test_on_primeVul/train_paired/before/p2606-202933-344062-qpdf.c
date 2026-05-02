QPDF::resolve(int objid, int generation)
{
    // Check object cache before checking xref table.  This allows us
    // to insert things into the object cache that don't actually
    // exist in the file.
    QPDFObjGen og(objid, generation);
    if (! this->obj_cache.count(og))
    {
	if (! this->xref_table.count(og))
	{
	    // PDF spec says unknown objects resolve to the null object.
	    return new QPDF_Null;
	}

	QPDFXRefEntry const& entry = this->xref_table[og];
	switch (entry.getType())
	{
	  case 1:
	    {
		qpdf_offset_t offset = entry.getOffset();
		// Object stored in cache by readObjectAtOffset
		int aobjid;
		int ageneration;
		QPDFObjectHandle oh =
		    readObjectAtOffset(true, offset, "", objid, generation,
				       aobjid, ageneration);
	    }
	    break;

	  case 2:
	    resolveObjectsInStream(entry.getObjStreamNumber());
	    break;

	  default:
	    throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(), "", 0,
			  "object " +
			  QUtil::int_to_string(objid) + "/" +
			  QUtil::int_to_string(generation) +
			  " has unexpected xref entry type");
	}
    }

    return this->obj_cache[og].object;
}