    std::string& attrf(int ncid, int varId, const char * attrName, std::string& alloc)
    {
        alloc = "";

        size_t len = 0;
        nc_inq_attlen(ncid, varId, attrName, &len);
        
        if(len < 1)
        {
            return alloc;
        }

        char attr_vals[NC_MAX_NAME + 1];
        memset(attr_vals, 0, NC_MAX_NAME + 1);

        // Now look through this variable for the attribute
        if(nc_get_att_text(ncid, varId, attrName, attr_vals) != NC_NOERR)
        {
            return alloc;
        }

        alloc = std::string(attr_vals);
        return alloc;
    }