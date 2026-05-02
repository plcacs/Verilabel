    std::string& attrf(int ncid, int varId, const char * attrName, std::string& alloc)
    {
        size_t len = 0;
        nc_inq_attlen(ncid, varId, attrName, &len);

        if(len < 1)
        {
            alloc.clear();
            return alloc;
        }

        alloc.resize(len);
        memset(&alloc[0], 0, len);

        // Now look through this variable for the attribute
        nc_get_att_text(ncid, varId, attrName, &alloc[0]);

        return alloc;
    }