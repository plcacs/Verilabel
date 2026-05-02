    bool read(ReadonlyBytes buffer)
    {
        auto fields_size = sizeof(EndOfCentralDirectory) - sizeof(u8*);
        if (buffer.size() < sizeof(end_of_central_directory_signature) + fields_size)
            return false;
        if (memcmp(buffer.data(), end_of_central_directory_signature, sizeof(end_of_central_directory_signature)) != 0)
            return false;
        memcpy(reinterpret_cast<void*>(&disk_number), buffer.data() + sizeof(end_of_central_directory_signature), fields_size);
        if (buffer.size() < sizeof(end_of_central_directory_signature) + fields_size + comment_length)
            return false;
        comment = buffer.data() + sizeof(end_of_central_directory_signature) + fields_size;
        return true;
    }