static int readOHDRHeaderMessageDataLayout(struct READER *reader,
                                           struct DATAOBJECT *data) {

  int i, err;
  unsigned size;

  uint8_t dimensionality, layout_class;
  uint32_t dataset_element_size;
  uint64_t data_address, store, data_size;

  UNUSED(dataset_element_size);
  UNUSED(data_size);

  if (fgetc(reader->fhd) != 3) {
    // LCOV_EXCL_START
    mylog("object OHDR message data layout message must have version 3\n");
    return MYSOFA_INVALID_FORMAT;
    // LCOV_EXCL_STOP
  }

  layout_class = (uint8_t)fgetc(reader->fhd);
  mylog("data layout %d\n", layout_class);

  switch (layout_class) {
#if 0
	case 0:
	data_size = readValue(reader, 2);
	fseek(reader->fhd, data_size, SEEK_CUR);
	mylog("TODO 0 SIZE %u\n", data_size);
	break;
#endif
  case 1:
    data_address = readValue(reader, reader->superblock.size_of_offsets);
    data_size = readValue(reader, reader->superblock.size_of_lengths);
    mylog("CHUNK Contiguous SIZE %" PRIu64 "\n", data_size);

    if (validAddress(reader, data_address)) {
      store = ftell(reader->fhd);
      if (fseek(reader->fhd, data_address, SEEK_SET) < 0)
        return errno; // LCOV_EXCL_LINE
      if (!data->data) {
        if (data_size > 0x10000000)
          return MYSOFA_INVALID_FORMAT;
        data->data_len = data_size;
        data->data = calloc(1, data_size);
        if (!data->data)
          return MYSOFA_NO_MEMORY; // LCOV_EXCL_LINE
      }
      err = fread(data->data, 1, data_size, reader->fhd);
      if (err != data_size)
        return MYSOFA_READ_ERROR; // LCOV_EXCL_LINE
      if (fseek(reader->fhd, store, SEEK_SET) < 0)
        return errno; // LCOV_EXCL_LINE
    }
    break;

  case 2:
    dimensionality = (uint8_t)fgetc(reader->fhd);
    mylog("dimensionality %d\n", dimensionality);

    if (dimensionality < 1 || dimensionality > DATAOBJECT_MAX_DIMENSIONALITY) {
      mylog("data layout 2: invalid dimensionality %d %lu %lu\n",
            dimensionality, sizeof(data->datalayout_chunk),
            sizeof(data->datalayout_chunk[0]));
      return MYSOFA_INVALID_FORMAT; // LCOV_EXCL_LINE
    }
    data_address = readValue(reader, reader->superblock.size_of_offsets);
    mylog(" CHUNK %" PRIX64 "\n", data_address);
    for (i = 0; i < dimensionality; i++) {
      data->datalayout_chunk[i] = readValue(reader, 4);
      mylog(" %d\n", data->datalayout_chunk[i]);
    }
    /* TODO last entry? error in spec: ?*/

    size = data->datalayout_chunk[dimensionality - 1];
    for (i = 0; i < data->ds.dimensionality; i++)
      size *= data->ds.dimension_size[i];

    if (validAddress(reader, data_address) && dimensionality <= 4) {
      store = ftell(reader->fhd);
      if (fseek(reader->fhd, data_address, SEEK_SET) < 0)
        return errno; // LCOV_EXCL_LINE
      if (!data->data) {
        if (size > 0x10000000)
          return MYSOFA_INVALID_FORMAT; // LCOV_EXCL_LINE
        data->data_len = size;
        data->data = calloc(1, size);
        if (!data->data)
          return MYSOFA_NO_MEMORY; // LCOV_EXCL_LINE
      }
      err = treeRead(reader, data);
      if (err)
        return err; // LCOV_EXCL_LINE
      if (fseek(reader->fhd, store, SEEK_SET) < 0)
        return errno; // LCOV_EXCL_LINE
    }
    break;

  default:
    // LCOV_EXCL_START
    mylog("object OHDR message data layout message has unknown layout class "
          "%d\n",
          layout_class);
    return MYSOFA_INVALID_FORMAT;
    // LCOV_EXCL_STOP
  }

  return MYSOFA_OK;
}