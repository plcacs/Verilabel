static int directblockRead(struct READER *reader, struct DATAOBJECT *dataobject,
		struct FRACTALHEAP *fractalheap) {

	char buf[4], *name, *value;
	int size, offset_size, length_size, err, len;
	uint8_t typeandversion;
	uint64_t unknown, heap_header_address, block_offset, block_size, offset,
			length;
	long store;
	struct DIR *dir;
	struct MYSOFA_ATTRIBUTE *attr;

	UNUSED(offset);
	UNUSED(block_size);
	UNUSED(block_offset);

	/* read signature */
	if (fread(buf, 1, 4, reader->fhd) != 4 || strncmp(buf, "FHDB", 4)) {
		log("cannot read signature of fractal heap indirect block\n");
		return MYSOFA_INVALID_FORMAT;
	}
	log("%08" PRIX64 " %.4s\n", (uint64_t )ftell(reader->fhd) - 4, buf);

	if (fgetc(reader->fhd) != 0) {
		log("object FHDB must have version 0\n");
		return MYSOFA_UNSUPPORTED_FORMAT;
	}

	/* ignore heap_header_address */
	if (fseek(reader->fhd, reader->superblock.size_of_offsets, SEEK_CUR) < 0)
		return errno;

	size = (fractalheap->maximum_heap_size + 7) / 8;
	block_offset = readValue(reader, size);

	if (fractalheap->flags & 2)
		if (fseek(reader->fhd, 4, SEEK_CUR))
			return errno;

	offset_size = ceilf(log2f(fractalheap->maximum_heap_size) / 8);
	if (fractalheap->maximum_direct_block_size < fractalheap->maximum_size)
		length_size = ceilf(log2f(fractalheap->maximum_direct_block_size) / 8);
	else
		length_size = ceilf(log2f(fractalheap->maximum_size) / 8);

	log(" %d %" PRIu64 " %d\n",size,block_offset,offset_size);

	/*
	 * 00003e00  00 46 48 44 42 00 40 02  00 00 00 00 00 00 00 00  |.FHDB.@.........|
	 00003e10  00 00 00 83 8d ac f6 >03  00 0c 00 08 00 04 00 00  |................|
	 00003e20  43 6f 6e 76 65 6e 74 69  6f 6e 73 00 13 00 00 00  |Conventions.....|
	 00003e30  04 00 00 00 02 00 00 00  53 4f 46 41< 03 00 08 00  |........SOFA....|
	 00003e40  08 00 04 00 00 56 65 72  73 69 6f 6e 00 13 00 00  |.....Version....|
	 00003e50  00 03 00 00 00 02 00 00  00 30 2e 36 03 00 10 00  |.........0.6....|
	 00003e60  08 00 04 00 00 53 4f 46  41 43 6f 6e 76 65 6e 74  |.....SOFAConvent|
	 00003e70  69 6f 6e 73 00 13 00 00  00 13 00 00 00 02 00 00  |ions............|
	 00003e80  00 53 69 6d 70 6c 65 46  72 65 65 46 69 65 6c 64  |.SimpleFreeField|
	 00003e90  48 52 49 52 03 00 17 00  08 00 04 00 00 53 4f 46  |HRIR.........SOF|
	 00003ea0  41 43 6f 6e 76 65 6e 74  69 6f 6e 73 56 65 72 73  |AConventionsVers|
	 00003eb0  69 6f 6e 00 13 00 00 00  03 00 00 00 02 00 00 00  |ion.............|
	 *
	 */
	do {
		typeandversion = (uint8_t) fgetc(reader->fhd);
		offset = readValue(reader, offset_size);
		length = readValue(reader, length_size);
		if (offset > 0x10000000 || length > 0x10000000)
			return MYSOFA_UNSUPPORTED_FORMAT;

		log(" %d %4" PRIX64 " %" PRIX64 " %08lX\n",typeandversion,offset,length,ftell(reader->fhd));

		/* TODO: for the following part, the specification is incomplete */
		if (typeandversion == 3) {
			/*
			 * this seems to be a name and value pair
			 */

			if (readValue(reader, 5) != 0x0000040008) {
				log("FHDB type 3 unsupported values");
				return MYSOFA_UNSUPPORTED_FORMAT;
			}

			if (!(name = malloc(length+1)))
				return MYSOFA_NO_MEMORY;
			if (fread(name, 1, length, reader->fhd) != length) {
				free(name);
				return MYSOFA_READ_ERROR;
			}
			name[length]=0;	

			if (readValue(reader, 4) != 0x00000013) {
				log("FHDB type 3 unsupported values");
				free(name);
				return MYSOFA_UNSUPPORTED_FORMAT;
			}

			len = (int) readValue(reader, 2);
			if (len > 0x1000 || len < 0) {
				free(name);
				return MYSOFA_UNSUPPORTED_FORMAT;
			}

			/* TODO: Get definition of this field */
			unknown = readValue(reader, 6);
			if (unknown == 0x000000020200)
				value = NULL;
			else if (unknown == 0x000000020000) {
				if (!(value = malloc(len + 1))) {
					free(name);
					return MYSOFA_NO_MEMORY;
				}
				if (fread(value, 1, len, reader->fhd) != len) {
					free(value);
					free(name);
					return MYSOFA_READ_ERROR;
				}
				value[len] = 0;
			} else if (unknown == 0x20000020000) {
				if (!(value = malloc(5))) {
					free(name);
					return MYSOFA_NO_MEMORY;
				}
				strcpy(value, "");
			} else {
				log("FHDB type 3 unsupported values: %12" PRIX64 "\n",unknown);
				free(name);
				/* TODO:			return MYSOFA_UNSUPPORTED_FORMAT; */
				return MYSOFA_OK;
			} 
			log(" %s = %s\n", name, value);

			attr = malloc(sizeof(struct MYSOFA_ATTRIBUTE));
			attr->name = name;
			attr->value = value;
			attr->next = dataobject->attributes;
			dataobject->attributes = attr;

		} else if (typeandversion == 1) {
			/*
			 * pointer to another data object
			 */
			unknown = readValue(reader, 6);
			if (unknown) {
				log("FHDB type 1 unsupported values\n");
				return MYSOFA_UNSUPPORTED_FORMAT;
			}

			len = fgetc(reader->fhd);
			if (len < 0)
				return MYSOFA_READ_ERROR;
			assert(len < 0x100);

			if (!(name = malloc(len + 1)))
				return MYSOFA_NO_MEMORY;
			if (fread(name, 1, len, reader->fhd) != len) {
				free(name);
				return MYSOFA_READ_ERROR;
			}
			name[len] = 0;

			heap_header_address = readValue(reader,
					reader->superblock.size_of_offsets);

			log("\nfractal head type 1 length %4" PRIX64 " name %s address %" PRIX64 "\n", length, name, heap_header_address);

			dir = malloc(sizeof(struct DIR));
			if (!dir) {
				free(name);
				return MYSOFA_NO_MEMORY;
			}
			memset(dir, 0, sizeof(*dir));

			dir->next = dataobject->directory;
			dataobject->directory = dir;

			store = ftell(reader->fhd);
			if (fseek(reader->fhd, heap_header_address, SEEK_SET)) {
				free(name);
				return errno;
			}

			err = dataobjectRead(reader, &dir->dataobject, name);
			if (err) {
				return err;
			}

			if (store < 0) {
				return errno;
			}
			if (fseek(reader->fhd, store, SEEK_SET) < 0)
				return errno;

		} else if (typeandversion != 0) {
			/* TODO is must be avoided somehow */
			log("fractal head unknown type %d\n", typeandversion);
			/*			return MYSOFA_UNSUPPORTED_FORMAT; */
			return MYSOFA_OK;
		}

	} while (typeandversion != 0);

	return MYSOFA_OK;
}