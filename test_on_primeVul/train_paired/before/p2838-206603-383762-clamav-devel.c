
int cli_scanpe(cli_ctx *ctx)
{
	uint16_t e_magic; /* DOS signature ("MZ") */
	uint16_t nsections;
	uint32_t e_lfanew; /* address of new exe header */
	uint32_t ep, vep; /* entry point (raw, virtual) */
	uint8_t polipos = 0;
	time_t timestamp;
	struct pe_image_file_hdr file_hdr;
	union {
	    struct pe_image_optional_hdr64 opt64;
	    struct pe_image_optional_hdr32 opt32;
	} pe_opt;
	struct pe_image_section_hdr *section_hdr;
	char sname[9], epbuff[4096], *tempfile;
	uint32_t epsize;
	ssize_t bytes, at;
	unsigned int i, found, upx_success = 0, min = 0, max = 0, err, overlays = 0;
	unsigned int ssize = 0, dsize = 0, dll = 0, pe_plus = 0, corrupted_cur;
	int (*upxfn)(const char *, uint32_t, char *, uint32_t *, uint32_t, uint32_t, uint32_t) = NULL;
	const char *src = NULL;
	char *dest = NULL;
	int ndesc, ret = CL_CLEAN, upack = 0, native=0;
	size_t fsize;
	uint32_t valign, falign, hdr_size, j;
	struct cli_exe_section *exe_sections;
	char timestr[32];
	struct pe_image_data_dir *dirs;
	struct cli_bc_ctx *bc_ctx;
	fmap_t *map;
	struct cli_pe_hook_data pedata;
#ifdef HAVE__INTERNAL__SHA_COLLECT
	int sha_collect = ctx->sha_collect;
#endif
    const char *archtype=NULL, *subsystem=NULL;
	uint32_t viruses_found = 0;
#if HAVE_JSON
        int toval = 0;
        struct json_object *pe_json=NULL;
        char jsonbuf[128];
#endif

    if(!ctx) {
	cli_errmsg("cli_scanpe: ctx == NULL\n");
	return CL_ENULLARG;
    }

#if HAVE_JSON
    if (cli_json_timeout_cycle_check(ctx, &toval) != CL_SUCCESS) {
        return CL_ETIMEOUT;
    }

    if (ctx->options & CL_SCAN_FILE_PROPERTIES) {
        pe_json = get_pe_property(ctx);
    }
#endif
    map = *ctx->fmap;
    if(fmap_readn(map, &e_magic, 0, sizeof(e_magic)) != sizeof(e_magic)) {
	cli_dbgmsg("Can't read DOS signature\n");
	return CL_CLEAN;
    }

    if(EC16(e_magic) != PE_IMAGE_DOS_SIGNATURE && EC16(e_magic) != PE_IMAGE_DOS_SIGNATURE_OLD) {
	cli_dbgmsg("Invalid DOS signature\n");
	return CL_CLEAN;
    }

    if(fmap_readn(map, &e_lfanew, 58 + sizeof(e_magic), sizeof(e_lfanew)) != sizeof(e_lfanew)) {
	cli_dbgmsg("Can't read new header address\n");
	/* truncated header? */
	if(DETECT_BROKEN_PE) {
	    cli_append_virus(ctx,"Heuristics.Broken.Executable");
	    return CL_VIRUS;
	}
	return CL_CLEAN;
    }

    e_lfanew = EC32(e_lfanew);
    cli_dbgmsg("e_lfanew == %d\n", e_lfanew);
    if(!e_lfanew) {
	cli_dbgmsg("Not a PE file\n");
	return CL_CLEAN;
    }

    if(fmap_readn(map, &file_hdr, e_lfanew, sizeof(struct pe_image_file_hdr)) != sizeof(struct pe_image_file_hdr)) {
	/* bad information in e_lfanew - probably not a PE file */
	cli_dbgmsg("Can't read file header\n");
	return CL_CLEAN;
    }

    if(EC32(file_hdr.Magic) != PE_IMAGE_NT_SIGNATURE) {
	cli_dbgmsg("Invalid PE signature (probably NE file)\n");
	return CL_CLEAN;
    }

    if(EC16(file_hdr.Characteristics) & 0x2000) {
#if HAVE_JSON
        if ((pe_json))
            cli_jsonstr(pe_json, "Type", "DLL");
#endif
	cli_dbgmsg("File type: DLL\n");
	dll = 1;
    } else if(EC16(file_hdr.Characteristics) & 0x01) {
#if HAVE_JSON
        if ((pe_json))
            cli_jsonstr(pe_json, "Type", "EXE");
#endif
	cli_dbgmsg("File type: Executable\n");
    }

    switch(EC16(file_hdr.Machine)) {
	case 0x0:
        archtype = "Unknown";
	    break;
	case 0x14c:
        archtype = "80386";
	    break;
	case 0x14d:
        archtype = "80486";
	    break;
	case 0x14e:
        archtype = "80586";
	    break;
	case 0x160:
        archtype = "R30000 (big-endian)";
	    break;
	case 0x162:
        archtype = "R3000";
	    break;
	case 0x166:
        archtype = "R4000";
	    break;
	case 0x168:
        archtype = "R10000";
	    break;
	case 0x184:
        archtype = "DEC Alpha AXP";
	    break;
	case 0x284:
        archtype = "DEC Alpha AXP 64bit";
	    break;
	case 0x1f0:
        archtype = "PowerPC";
	    break;
	case 0x200:
        archtype = "IA64";
	    break;
	case 0x268:
        archtype = "M68k";
	    break;
	case 0x266:
        archtype = "MIPS16";
	    break;
	case 0x366:
        archtype = "MIPS+FPU";
	    break;
	case 0x466:
        archtype = "MIPS16+FPU";
	    break;
	case 0x1a2:
        archtype = "Hitachi SH3";
	    break;
	case 0x1a3:
        archtype = "Hitachi SH3-DSP";
	    break;
	case 0x1a4:
        archtype = "Hitachi SH3-E";
	    break;
	case 0x1a6:
        archtype = "Hitachi SH4";
	    break;
	case 0x1a8:
        archtype = "Hitachi SH5";
	    break;
	case 0x1c0:
        archtype = "ARM";
	    break;
	case 0x1c2:
        archtype = "THUMB";
	    break;
	case 0x1d3:
        archtype = "AM33";
	    break;
	case 0x520:
        archtype = "Infineon TriCore";
	    break;
	case 0xcef:
        archtype = "CEF";
	    break;
	case 0xebc:
        archtype = "EFI Byte Code";
	    break;
	case 0x9041:
        archtype = "M32R";
	    break;
	case 0xc0ee:
        archtype = "CEEE";
	    break;
	case 0x8664:
        archtype = "AMD64";
	    break;
	default:
        archtype = "Unknown";
    }

    if ((archtype)) {
        cli_dbgmsg("Machine type: %s\n", archtype);
#if HAVE_JSON
        cli_jsonstr(pe_json, "ArchType", archtype);
#endif
    }

    nsections = EC16(file_hdr.NumberOfSections);
    if(nsections < 1 || nsections > 96) {
#if HAVE_JSON
        pe_add_heuristic_property(ctx, "BadNumberOfSections");
#endif
	if(DETECT_BROKEN_PE) {
	    cli_append_virus(ctx,"Heuristics.Broken.Executable");
	    return CL_VIRUS;
	}
	if(!ctx->corrupted_input) {
	    if(nsections)
		cli_warnmsg("PE file contains %d sections\n", nsections);
	    else
		cli_warnmsg("PE file contains no sections\n");
	}
	return CL_CLEAN;
    }
    cli_dbgmsg("NumberOfSections: %d\n", nsections);

    timestamp = (time_t) EC32(file_hdr.TimeDateStamp);
    cli_dbgmsg("TimeDateStamp: %s", cli_ctime(&timestamp, timestr, sizeof(timestr)));

#if HAVE_JSON
    cli_jsonstr(pe_json, "TimeDateStamp", cli_ctime(&timestamp, timestr, sizeof(timestr)));
#endif

    cli_dbgmsg("SizeOfOptionalHeader: %x\n", EC16(file_hdr.SizeOfOptionalHeader));

#if HAVE_JSON
    cli_jsonint(pe_json, "SizeOfOptionalHeader", EC16(file_hdr.SizeOfOptionalHeader));
#endif

    if (EC16(file_hdr.SizeOfOptionalHeader) < sizeof(struct pe_image_optional_hdr32)) {
#if HAVE_JSON
        pe_add_heuristic_property(ctx, "BadOptionalHeaderSize");
#endif
        cli_dbgmsg("SizeOfOptionalHeader too small\n");
	if(DETECT_BROKEN_PE) {
	    cli_append_virus(ctx,"Heuristics.Broken.Executable");
	    return CL_VIRUS;
	}
	return CL_CLEAN;
    }

    at = e_lfanew + sizeof(struct pe_image_file_hdr);
    if(fmap_readn(map, &optional_hdr32, at, sizeof(struct pe_image_optional_hdr32)) != sizeof(struct pe_image_optional_hdr32)) {
        cli_dbgmsg("Can't read optional file header\n");
	if(DETECT_BROKEN_PE) {
	    cli_append_virus(ctx,"Heuristics.Broken.Executable");
	    return CL_VIRUS;
	}
	return CL_CLEAN;
    }
    at += sizeof(struct pe_image_optional_hdr32);

    /* This will be a chicken and egg problem until we drop 9x */
    if(EC16(optional_hdr64.Magic)==PE32P_SIGNATURE) {
#if HAVE_JSON
        pe_add_heuristic_property(ctx, "BadOptionalHeaderSizePE32Plus");
#endif
        if(EC16(file_hdr.SizeOfOptionalHeader)!=sizeof(struct pe_image_optional_hdr64)) {
	    /* FIXME: need to play around a bit more with xp64 */
	    cli_dbgmsg("Incorrect SizeOfOptionalHeader for PE32+\n");
	    if(DETECT_BROKEN_PE) {
		cli_append_virus(ctx,"Heuristics.Broken.Executable");
		return CL_VIRUS;
	    }
	    return CL_CLEAN;
	}
	pe_plus = 1;
    }

    if(!pe_plus) { /* PE */
	if (EC16(file_hdr.SizeOfOptionalHeader)!=sizeof(struct pe_image_optional_hdr32)) {
	    /* Seek to the end of the long header */
	    at += EC16(file_hdr.SizeOfOptionalHeader)-sizeof(struct pe_image_optional_hdr32);
	}

	if(DCONF & PE_CONF_UPACK)
	    upack = (EC16(file_hdr.SizeOfOptionalHeader)==0x148);

	vep = EC32(optional_hdr32.AddressOfEntryPoint);
	hdr_size = EC32(optional_hdr32.SizeOfHeaders);
	cli_dbgmsg("File format: PE\n");

	cli_dbgmsg("MajorLinkerVersion: %d\n", optional_hdr32.MajorLinkerVersion);
	cli_dbgmsg("MinorLinkerVersion: %d\n", optional_hdr32.MinorLinkerVersion);
	cli_dbgmsg("SizeOfCode: 0x%x\n", EC32(optional_hdr32.SizeOfCode));
	cli_dbgmsg("SizeOfInitializedData: 0x%x\n", EC32(optional_hdr32.SizeOfInitializedData));
	cli_dbgmsg("SizeOfUninitializedData: 0x%x\n", EC32(optional_hdr32.SizeOfUninitializedData));
	cli_dbgmsg("AddressOfEntryPoint: 0x%x\n", vep);
	cli_dbgmsg("BaseOfCode: 0x%x\n", EC32(optional_hdr32.BaseOfCode));
	cli_dbgmsg("SectionAlignment: 0x%x\n", EC32(optional_hdr32.SectionAlignment));
	cli_dbgmsg("FileAlignment: 0x%x\n", EC32(optional_hdr32.FileAlignment));
	cli_dbgmsg("MajorSubsystemVersion: %d\n", EC16(optional_hdr32.MajorSubsystemVersion));
	cli_dbgmsg("MinorSubsystemVersion: %d\n", EC16(optional_hdr32.MinorSubsystemVersion));
	cli_dbgmsg("SizeOfImage: 0x%x\n", EC32(optional_hdr32.SizeOfImage));
	cli_dbgmsg("SizeOfHeaders: 0x%x\n", hdr_size);
	cli_dbgmsg("NumberOfRvaAndSizes: %d\n", EC32(optional_hdr32.NumberOfRvaAndSizes));
	dirs = optional_hdr32.DataDirectory;
#if HAVE_JSON
    cli_jsonint(pe_json, "MajorLinkerVersion", optional_hdr32.MajorLinkerVersion);
    cli_jsonint(pe_json, "MinorLinkerVersion", optional_hdr32.MinorLinkerVersion);
    cli_jsonint(pe_json, "SizeOfCode", EC32(optional_hdr32.SizeOfCode));
    cli_jsonint(pe_json, "SizeOfInitializedData", EC32(optional_hdr32.SizeOfInitializedData));
    cli_jsonint(pe_json, "SizeOfUninitializedData", EC32(optional_hdr32.SizeOfUninitializedData));
    cli_jsonint(pe_json, "NumberOfRvaAndSizes", EC32(optional_hdr32.NumberOfRvaAndSizes));
    cli_jsonint(pe_json, "MajorSubsystemVersion", EC16(optional_hdr32.MajorSubsystemVersion));
    cli_jsonint(pe_json, "MinorSubsystemVersion", EC16(optional_hdr32.MinorSubsystemVersion));

    snprintf(jsonbuf, sizeof(jsonbuf), "0x%x", EC32(optional_hdr32.BaseOfCode));
    cli_jsonstr(pe_json, "BaseOfCode", jsonbuf);

    snprintf(jsonbuf, sizeof(jsonbuf), "0x%x", EC32(optional_hdr32.SectionAlignment));
    cli_jsonstr(pe_json, "SectionAlignment", jsonbuf);

    snprintf(jsonbuf, sizeof(jsonbuf), "0x%x", EC32(optional_hdr32.FileAlignment));
    cli_jsonstr(pe_json, "FileAlignment", jsonbuf);

    snprintf(jsonbuf, sizeof(jsonbuf), "0x%x", EC32(optional_hdr32.SizeOfImage));
    cli_jsonstr(pe_json, "SizeOfImage", jsonbuf);

    snprintf(jsonbuf, sizeof(jsonbuf), "0x%x", hdr_size);
    cli_jsonstr(pe_json, "SizeOfHeaders", jsonbuf);
#endif

    } else { /* PE+ */
        /* read the remaining part of the header */
        if(fmap_readn(map, &optional_hdr32 + 1, at, sizeof(struct pe_image_optional_hdr64) - sizeof(struct pe_image_optional_hdr32)) != sizeof(struct pe_image_optional_hdr64) - sizeof(struct pe_image_optional_hdr32)) {
	    cli_dbgmsg("Can't read optional file header\n");
	    if(DETECT_BROKEN_PE) {
		cli_append_virus(ctx,"Heuristics.Broken.Executable");
		return CL_VIRUS;
	    }
	    return CL_CLEAN;
	}
	at += sizeof(struct pe_image_optional_hdr64) - sizeof(struct pe_image_optional_hdr32);
	vep = EC32(optional_hdr64.AddressOfEntryPoint);
	hdr_size = EC32(optional_hdr64.SizeOfHeaders);
	cli_dbgmsg("File format: PE32+\n");

	cli_dbgmsg("MajorLinkerVersion: %d\n", optional_hdr64.MajorLinkerVersion);
	cli_dbgmsg("MinorLinkerVersion: %d\n", optional_hdr64.MinorLinkerVersion);
	cli_dbgmsg("SizeOfCode: 0x%x\n", EC32(optional_hdr64.SizeOfCode));
	cli_dbgmsg("SizeOfInitializedData: 0x%x\n", EC32(optional_hdr64.SizeOfInitializedData));
	cli_dbgmsg("SizeOfUninitializedData: 0x%x\n", EC32(optional_hdr64.SizeOfUninitializedData));
	cli_dbgmsg("AddressOfEntryPoint: 0x%x\n", vep);
	cli_dbgmsg("BaseOfCode: 0x%x\n", EC32(optional_hdr64.BaseOfCode));
	cli_dbgmsg("SectionAlignment: 0x%x\n", EC32(optional_hdr64.SectionAlignment));
	cli_dbgmsg("FileAlignment: 0x%x\n", EC32(optional_hdr64.FileAlignment));
	cli_dbgmsg("MajorSubsystemVersion: %d\n", EC16(optional_hdr64.MajorSubsystemVersion));
	cli_dbgmsg("MinorSubsystemVersion: %d\n", EC16(optional_hdr64.MinorSubsystemVersion));
	cli_dbgmsg("SizeOfImage: 0x%x\n", EC32(optional_hdr64.SizeOfImage));
	cli_dbgmsg("SizeOfHeaders: 0x%x\n", hdr_size);
	cli_dbgmsg("NumberOfRvaAndSizes: %d\n", EC32(optional_hdr64.NumberOfRvaAndSizes));
	dirs = optional_hdr64.DataDirectory;
#if HAVE_JSON
    cli_jsonint(pe_json, "MajorLinkerVersion", optional_hdr64.MajorLinkerVersion);
    cli_jsonint(pe_json, "MinorLinkerVersion", optional_hdr64.MinorLinkerVersion);
    cli_jsonint(pe_json, "SizeOfCode", EC32(optional_hdr64.SizeOfCode));
    cli_jsonint(pe_json, "SizeOfInitializedData", EC32(optional_hdr64.SizeOfInitializedData));
    cli_jsonint(pe_json, "SizeOfUninitializedData", EC32(optional_hdr64.SizeOfUninitializedData));
    cli_jsonint(pe_json, "NumberOfRvaAndSizes", EC32(optional_hdr64.NumberOfRvaAndSizes));
    cli_jsonint(pe_json, "MajorSubsystemVersion", EC16(optional_hdr64.MajorSubsystemVersion));
    cli_jsonint(pe_json, "MinorSubsystemVersion", EC16(optional_hdr64.MinorSubsystemVersion));

    snprintf(jsonbuf, sizeof(jsonbuf), "0x%x", EC32(optional_hdr64.BaseOfCode));
    cli_jsonstr(pe_json, "BaseOfCode", jsonbuf);

    snprintf(jsonbuf, sizeof(jsonbuf), "0x%x", EC32(optional_hdr64.SectionAlignment));
    cli_jsonstr(pe_json, "SectionAlignment", jsonbuf);

    snprintf(jsonbuf, sizeof(jsonbuf), "0x%x", EC32(optional_hdr64.FileAlignment));
    cli_jsonstr(pe_json, "FileAlignment", jsonbuf);

    snprintf(jsonbuf, sizeof(jsonbuf), "0x%x", EC32(optional_hdr64.SizeOfImage));
    cli_jsonstr(pe_json, "SizeOfImage", jsonbuf);

    snprintf(jsonbuf, sizeof(jsonbuf), "0x%x", hdr_size);
    cli_jsonstr(pe_json, "SizeOfHeaders", jsonbuf);
#endif
    }

#if HAVE_JSON
    if (ctx->options & CL_SCAN_FILE_PROPERTIES) {
        snprintf(jsonbuf, sizeof(jsonbuf), "0x%x", vep);
        cli_jsonstr(pe_json, "EntryPoint", jsonbuf);
    }
#endif


    switch(pe_plus ? EC16(optional_hdr64.Subsystem) : EC16(optional_hdr32.Subsystem)) {
	case 0:
        subsystem = "Unknown";
	    break;
	case 1:
        subsystem = "Native (svc)";
	    native = 1;
	    break;
	case 2:
        subsystem = "Win32 GUI";
	    break;
	case 3:
        subsystem = "Win32 console";
	    break;
	case 5:
        subsystem = "OS/2 console";
	    break;
	case 7:
        subsystem = "POSIX console";
	    break;
	case 8:
        subsystem = "Native Win9x driver";
	    break;
	case 9:
        subsystem = "WinCE GUI";
	    break;
	case 10:
        subsystem = "EFI application";
	    break;
	case 11:
        subsystem = "EFI driver";
	    break;
	case 12:
        subsystem = "EFI runtime driver";
	    break;
	case 13:
        subsystem = "EFI ROM image";
	    break;
	case 14:
        subsystem = "Xbox";
	    break;
	case 16:
        subsystem = "Boot application";
	    break;
	default:
        subsystem = "Unknown";
    }

    cli_dbgmsg("Subsystem: %s\n", subsystem);

#if HAVE_JSON
    cli_jsonstr(pe_json, "Subsystem", subsystem);
#endif

    cli_dbgmsg("------------------------------------\n");

    if (DETECT_BROKEN_PE && !native && (!(pe_plus?EC32(optional_hdr64.SectionAlignment):EC32(optional_hdr32.SectionAlignment)) || (pe_plus?EC32(optional_hdr64.SectionAlignment):EC32(optional_hdr32.SectionAlignment))%0x1000)) {
        cli_dbgmsg("Bad virtual alignemnt\n");
	cli_append_virus(ctx,"Heuristics.Broken.Executable");
	return CL_VIRUS;
    }

    if (DETECT_BROKEN_PE && !native && (!(pe_plus?EC32(optional_hdr64.FileAlignment):EC32(optional_hdr32.FileAlignment)) || (pe_plus?EC32(optional_hdr64.FileAlignment):EC32(optional_hdr32.FileAlignment))%0x200)) {
        cli_dbgmsg("Bad file alignemnt\n");
	cli_append_virus(ctx, "Heuristics.Broken.Executable");
	return CL_VIRUS;
    }

    fsize = map->len;

    section_hdr = (struct pe_image_section_hdr *) cli_calloc(nsections, sizeof(struct pe_image_section_hdr));

    if(!section_hdr) {
	cli_dbgmsg("Can't allocate memory for section headers\n");
	return CL_EMEM;
    }

    exe_sections = (struct cli_exe_section *) cli_calloc(nsections, sizeof(struct cli_exe_section));
    
    if(!exe_sections) {
	cli_dbgmsg("Can't allocate memory for section headers\n");
	free(section_hdr);
	return CL_EMEM;
    }

    valign = (pe_plus)?EC32(optional_hdr64.SectionAlignment):EC32(optional_hdr32.SectionAlignment);
    falign = (pe_plus)?EC32(optional_hdr64.FileAlignment):EC32(optional_hdr32.FileAlignment);

    if(fmap_readn(map, section_hdr, at, sizeof(struct pe_image_section_hdr)*nsections) != (int)(nsections*sizeof(struct pe_image_section_hdr))) {
        cli_dbgmsg("Can't read section header\n");
	cli_dbgmsg("Possibly broken PE file\n");
	free(section_hdr);
	free(exe_sections);
	if(DETECT_BROKEN_PE) {
	    cli_append_virus(ctx,"Heuristics.Broken.Executable");
	    return CL_VIRUS;
	}
	return CL_CLEAN;
    }
    at += sizeof(struct pe_image_section_hdr)*nsections;

    for(i = 0; falign!=0x200 && i<nsections; i++) {
	/* file alignment fallback mode - blah */
	if (falign && section_hdr[i].SizeOfRawData && EC32(section_hdr[i].PointerToRawData)%falign && !(EC32(section_hdr[i].PointerToRawData)%0x200)) {
	    cli_dbgmsg("Found misaligned section, using 0x200\n");
	    falign = 0x200;
	}
    }

    hdr_size = PESALIGN(hdr_size, valign); /* Aligned headers virtual size */

#if HAVE_JSON
    cli_jsonint(pe_json, "NumberOfSections", nsections);
#endif

    for(i = 0; i < nsections; i++) {
	strncpy(sname, (char *) section_hdr[i].Name, 8);
	sname[8] = 0;
	exe_sections[i].rva = PEALIGN(EC32(section_hdr[i].VirtualAddress), valign);
	exe_sections[i].vsz = PESALIGN(EC32(section_hdr[i].VirtualSize), valign);
	exe_sections[i].raw = PEALIGN(EC32(section_hdr[i].PointerToRawData), falign);
	exe_sections[i].rsz = PESALIGN(EC32(section_hdr[i].SizeOfRawData), falign);
	exe_sections[i].chr = EC32(section_hdr[i].Characteristics);
	exe_sections[i].urva = EC32(section_hdr[i].VirtualAddress); /* Just in case */
	exe_sections[i].uvsz = EC32(section_hdr[i].VirtualSize);
	exe_sections[i].uraw = EC32(section_hdr[i].PointerToRawData);
	exe_sections[i].ursz = EC32(section_hdr[i].SizeOfRawData);

#if HAVE_JSON
        add_section_info(ctx, &exe_sections[i]);

        if (cli_json_timeout_cycle_check(ctx, &toval) != CL_SUCCESS) {
            free(section_hdr);
            free(exe_sections);
            return CL_ETIMEOUT;
        }
#endif

	if (!exe_sections[i].vsz && exe_sections[i].rsz)
	    exe_sections[i].vsz=PESALIGN(exe_sections[i].ursz, valign);

	if (exe_sections[i].rsz && fsize>exe_sections[i].raw && !CLI_ISCONTAINED(0, (uint32_t) fsize, exe_sections[i].raw, exe_sections[i].rsz))
	    exe_sections[i].rsz = fsize - exe_sections[i].raw;
	
	cli_dbgmsg("Section %d\n", i);
	cli_dbgmsg("Section name: %s\n", sname);
	cli_dbgmsg("Section data (from headers - in memory)\n");
	cli_dbgmsg("VirtualSize: 0x%x 0x%x\n", exe_sections[i].uvsz, exe_sections[i].vsz);
	cli_dbgmsg("VirtualAddress: 0x%x 0x%x\n", exe_sections[i].urva, exe_sections[i].rva);
	cli_dbgmsg("SizeOfRawData: 0x%x 0x%x\n", exe_sections[i].ursz, exe_sections[i].rsz);
	cli_dbgmsg("PointerToRawData: 0x%x 0x%x\n", exe_sections[i].uraw, exe_sections[i].raw);

	if(exe_sections[i].chr & 0x20) {
	    cli_dbgmsg("Section contains executable code\n");

	    if(exe_sections[i].vsz < exe_sections[i].rsz) {
		cli_dbgmsg("Section contains free space\n");
		/*
		cli_dbgmsg("Dumping %d bytes\n", section_hdr.SizeOfRawData - section_hdr.VirtualSize);
		ddump(desc, section_hdr.PointerToRawData + section_hdr.VirtualSize, section_hdr.SizeOfRawData - section_hdr.VirtualSize, cli_gentemp(NULL));
		*/

	    }
	}

	if(exe_sections[i].chr & 0x20000000)
	    cli_dbgmsg("Section's memory is executable\n");

	if(exe_sections[i].chr & 0x80000000)
	    cli_dbgmsg("Section's memory is writeable\n");

	if (DETECT_BROKEN_PE && (!valign || (exe_sections[i].urva % valign))) { /* Bad virtual alignment */
	    cli_dbgmsg("VirtualAddress is misaligned\n");
	    cli_dbgmsg("------------------------------------\n");
	    cli_append_virus(ctx, "Heuristics.Broken.Executable");
	    free(section_hdr);
	    free(exe_sections);
	    return CL_VIRUS;
	}

	if (exe_sections[i].rsz) { /* Don't bother with virtual only sections */
	    if (exe_sections[i].raw >= fsize) { /* really broken */
	      cli_dbgmsg("Broken PE file - Section %d starts beyond the end of file (Offset@ %lu, Total filesize %lu)\n", i, (unsigned long)exe_sections[i].raw, (unsigned long)fsize);
	      cli_dbgmsg("------------------------------------\n");
		free(section_hdr);
		free(exe_sections);
		if(DETECT_BROKEN_PE) {
		    cli_append_virus(ctx, "Heuristics.Broken.Executable");
		    return CL_VIRUS;
		}
		return CL_CLEAN; /* no ninjas to see here! move along! */
	    }

	    if(SCAN_ALGO && (DCONF & PE_CONF_POLIPOS) && !*sname && exe_sections[i].vsz > 40000 && exe_sections[i].vsz < 70000 && exe_sections[i].chr == 0xe0000060) polipos = i;

	    /* check hash section sigs */
	    if((DCONF & PE_CONF_MD5SECT) && ctx->engine->hm_mdb) {
	        ret = scan_pe_mdb(ctx, &exe_sections[i]);
	        if (ret != CL_CLEAN) {
	            if (ret != CL_VIRUS)
	                cli_errmsg("scan_pe: scan_pe_mdb failed: %s!\n", cl_strerror(ret));
		    cli_dbgmsg("------------------------------------\n");
	            free(section_hdr);
	            free(exe_sections);
	            return ret;
	        }
	    }
	}
	cli_dbgmsg("------------------------------------\n");

	if (exe_sections[i].urva>>31 || exe_sections[i].uvsz>>31 || (exe_sections[i].rsz && exe_sections[i].uraw>>31) || exe_sections[i].ursz>>31) {
	    cli_dbgmsg("Found PE values with sign bit set\n");
	    free(section_hdr);
	    free(exe_sections);
	    if(DETECT_BROKEN_PE) {
		cli_append_virus(ctx, "Heuristics.Broken.Executable");
		return CL_VIRUS;
	    }
	    return CL_CLEAN;
	}

	if(!i) {
	    if (DETECT_BROKEN_PE && exe_sections[i].urva!=hdr_size) { /* Bad first section RVA */
	        cli_dbgmsg("First section is in the wrong place\n");
		cli_append_virus(ctx, "Heuristics.Broken.Executable");
		free(section_hdr);
		free(exe_sections);
		return CL_VIRUS;
	    }
	    min = exe_sections[i].rva;
	    max = exe_sections[i].rva + exe_sections[i].rsz;
	} else {
	    if (DETECT_BROKEN_PE && exe_sections[i].urva - exe_sections[i-1].urva != exe_sections[i-1].vsz) { /* No holes, no overlapping, no virtual disorder */
	        cli_dbgmsg("Virtually misplaced section (wrong order, overlapping, non contiguous)\n");
		cli_append_virus(ctx, "Heuristics.Broken.Executable");
		free(section_hdr);
		free(exe_sections);
		return CL_VIRUS;
	    }
	    if(exe_sections[i].rva < min)
	        min = exe_sections[i].rva;

	    if(exe_sections[i].rva + exe_sections[i].rsz > max) {
	        max = exe_sections[i].rva + exe_sections[i].rsz;
		overlays = exe_sections[i].raw + exe_sections[i].rsz;
	    }
	}
    }

    free(section_hdr);

    if(!(ep = cli_rawaddr(vep, exe_sections, nsections, &err, fsize, hdr_size)) && err) {
	cli_dbgmsg("EntryPoint out of file\n");
	free(exe_sections);
	if(DETECT_BROKEN_PE) {
	    cli_append_virus(ctx,"Heuristics.Broken.Executable");
	    return CL_VIRUS;
	}
	return CL_CLEAN;
    }

#if HAVE_JSON
    cli_jsonint(pe_json, "EntryPointOffset", ep);

    if (cli_json_timeout_cycle_check(ctx, &toval) != CL_SUCCESS) {
        return CL_ETIMEOUT;
    }
#endif

    cli_dbgmsg("EntryPoint offset: 0x%x (%d)\n", ep, ep);

    if(pe_plus) { /* Do not continue for PE32+ files */
	free(exe_sections);
	return CL_CLEAN;
    }

    epsize = fmap_readn(map, epbuff, ep, 4096);


    /* Disasm scan disabled since it's now handled by the bytecode */

    /* CLI_UNPTEMP("DISASM",(exe_sections,0)); */
    /* if(disasmbuf((unsigned char*)epbuff, epsize, ndesc)) */
    /* 	ret = cli_scandesc(ndesc, ctx, CL_TYPE_PE_DISASM, 1, NULL, AC_SCAN_VIR); */
    /* close(ndesc); */
    /* CLI_TMPUNLK(); */
    /* free(tempfile); */
    /* if(ret == CL_VIRUS) { */
    /* 	free(exe_sections); */
    /* 	return ret; */
    /* } */

    if(overlays) {
	int overlays_sz = fsize - overlays;
	if(overlays_sz > 0) {
	    ret = cli_scanishield(ctx, overlays, overlays_sz);
	    if(ret != CL_CLEAN) {
		free(exe_sections);
		return ret;
	    }
	}
    }

    pedata.nsections = nsections;
    pedata.ep = ep;
    pedata.offset = 0;
    memcpy(&pedata.file_hdr, &file_hdr, sizeof(file_hdr));
    memcpy(&pedata.opt32, &pe_opt.opt32, sizeof(pe_opt.opt32));
    memcpy(&pedata.opt64, &pe_opt.opt64, sizeof(pe_opt.opt64));
    memcpy(&pedata.dirs, dirs, sizeof(pedata.dirs));
    pedata.e_lfanew = e_lfanew;
    pedata.overlays = overlays;
    pedata.overlays_sz = fsize - overlays;
    pedata.hdr_size = hdr_size;

    /* Bytecode BC_PE_ALL hook */
    bc_ctx = cli_bytecode_context_alloc();
    if (!bc_ctx) {
	cli_errmsg("cli_scanpe: can't allocate memory for bc_ctx\n");
	free(exe_sections);
	return CL_EMEM;
    }
    cli_bytecode_context_setpe(bc_ctx, &pedata, exe_sections);
    cli_bytecode_context_setctx(bc_ctx, ctx);
    ret = cli_bytecode_runhook(ctx, ctx->engine, bc_ctx, BC_PE_ALL, map);
    switch (ret) {
        case CL_ENULLARG:
            cli_warnmsg("cli_scanpe: NULL argument supplied\n");
            break;
        case CL_VIRUS:
        case CL_BREAK:
            free(exe_sections);
            cli_bytecode_context_destroy(bc_ctx);
            return ret == CL_VIRUS ? CL_VIRUS : CL_CLEAN;
    }
    cli_bytecode_context_destroy(bc_ctx);
    /* Attempt to detect some popular polymorphic viruses */

    /* W32.Parite.B */
    if(SCAN_ALGO && (DCONF & PE_CONF_PARITE) && !dll && epsize == 4096 && ep == exe_sections[nsections - 1].raw) {
        const char *pt = cli_memstr(epbuff, 4040, "\x47\x65\x74\x50\x72\x6f\x63\x41\x64\x64\x72\x65\x73\x73\x00", 15);
	if(pt) {
	    pt += 15;
	    if((((uint32_t)cli_readint32(pt) ^ (uint32_t)cli_readint32(pt + 4)) == 0x505a4f) && (((uint32_t)cli_readint32(pt + 8) ^ (uint32_t)cli_readint32(pt + 12)) == 0xffffb) && (((uint32_t)cli_readint32(pt + 16) ^ (uint32_t)cli_readint32(pt + 20)) == 0xb8)) {
	        cli_append_virus(ctx,"Heuristics.W32.Parite.B");
		if (!SCAN_ALL) {
		    free(exe_sections);
		    return CL_VIRUS;
		}
		viruses_found++;
	    }
	}
    }

    /* Kriz */
    if(SCAN_ALGO && (DCONF & PE_CONF_KRIZ) && epsize >= 200 && CLI_ISCONTAINED(exe_sections[nsections - 1].raw, exe_sections[nsections - 1].rsz, ep, 0x0fd2) && epbuff[1]=='\x9c' && epbuff[2]=='\x60') {
	enum {KZSTRASH,KZSCDELTA,KZSPDELTA,KZSGETSIZE,KZSXORPRFX,KZSXOR,KZSDDELTA,KZSLOOP,KZSTOP};
	uint8_t kzs[] = {KZSTRASH,KZSCDELTA,KZSPDELTA,KZSGETSIZE,KZSTRASH,KZSXORPRFX,KZSXOR,KZSTRASH,KZSDDELTA,KZSTRASH,KZSLOOP,KZSTOP};
	uint8_t *kzstate = kzs;
	uint8_t *kzcode = (uint8_t *)epbuff + 3;
	uint8_t kzdptr=0xff, kzdsize=0xff;
	int kzlen = 197, kzinitlen=0xffff, kzxorlen=-1;
	cli_dbgmsg("in kriz\n");

	while(*kzstate!=KZSTOP) {
	    uint8_t op;
	    if(kzlen<=6) break;
	    op = *kzcode++;
	    kzlen--;
	    switch (*kzstate) {
	    case KZSTRASH: case KZSGETSIZE: {
		int opsz=0;
		switch(op) {
		case 0x81:
		    kzcode+=5;
		    kzlen-=5;
		    break;
		case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbd: case 0xbe: case 0xbf:
		    if(*kzstate==KZSGETSIZE && cli_readint32(kzcode)==0x0fd2) {
			kzinitlen = kzlen-5;
			kzdsize=op-0xb8;
			kzstate++;
			op=4; /* fake the register to avoid breaking out */
			cli_dbgmsg("kriz: using #%d as size counter\n", kzdsize);
		    }
		    opsz=4;
		case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4d: case 0x4e: case 0x4f:
		    op&=7;
		    if(op!=kzdptr && op!=kzdsize) {
			kzcode+=opsz;
			kzlen-=opsz;
			break;
		    }
		default:
		    kzcode--;
		    kzlen++;
		    kzstate++;
		}
		break;
	    }
	    case KZSCDELTA:
		if(op==0xe8 && (uint32_t)cli_readint32(kzcode) < 0xff) {
		    kzlen-=*kzcode+4;
		    kzcode+=*kzcode+4;
		    kzstate++;
		} else *kzstate=KZSTOP;
		break;
	    case KZSPDELTA:
		if((op&0xf8)==0x58 && (kzdptr=op-0x58)!=4) {
		    kzstate++;
		    cli_dbgmsg("kriz: using #%d as pointer\n", kzdptr);
		} else *kzstate=KZSTOP;
		break;
	    case KZSXORPRFX:
		kzstate++;
		if(op==0x3e) break;
	    case KZSXOR:
		if (op==0x80 && *kzcode==kzdptr+0xb0) {
		    kzxorlen=kzlen;
		    kzcode+=+6;
		    kzlen-=+6;
		    kzstate++;
		} else *kzstate=KZSTOP;
		break;
	    case KZSDDELTA:
		if (op==kzdptr+0x48) kzstate++;
		else *kzstate=KZSTOP;
		break;
	    case KZSLOOP:
		if (op==kzdsize+0x48 && *kzcode==0x75 && kzlen-(int8_t)kzcode[1]-3<=kzinitlen && kzlen-(int8_t)kzcode[1]>=kzxorlen) {
		    cli_append_virus(ctx,"Heuristics.W32.Kriz");
		    if (!SCAN_ALL) {
		        free(exe_sections);
			return CL_VIRUS;
		    }
		    viruses_found++;
		}
		cli_dbgmsg("kriz: loop out of bounds, corrupted sample?\n");
		kzstate++;
	    }
	}
    }

    /* W32.Magistr.A/B */
    if(SCAN_ALGO && (DCONF & PE_CONF_MAGISTR) && !dll && (nsections>1) && (exe_sections[nsections - 1].chr & 0x80000000)) {
        uint32_t rsize, vsize, dam = 0;

	vsize = exe_sections[nsections - 1].uvsz;
	rsize = exe_sections[nsections - 1].rsz;
	if(rsize < exe_sections[nsections - 1].ursz) {
	    rsize = exe_sections[nsections - 1].ursz;
	    dam = 1;
	}

	if(vsize >= 0x612c && rsize >= 0x612c && ((vsize & 0xff) == 0xec)) {
		int bw = rsize < 0x7000 ? rsize : 0x7000;
		const char *tbuff;

	    if((tbuff = fmap_need_off_once(map, exe_sections[nsections - 1].raw + rsize - bw, 4096))) {
		if(cli_memstr(tbuff, 4091, "\xe8\x2c\x61\x00\x00", 5)) {
		    cli_append_virus(ctx, dam ? "Heuristics.W32.Magistr.A.dam" : "Heuristics.W32.Magistr.A");
		    if (!SCAN_ALL) {
		        free(exe_sections);
			return CL_VIRUS;
		    }
		    viruses_found++;
		}
	    }

	} else if(rsize >= 0x7000 && vsize >= 0x7000 && ((vsize & 0xff) == 0xed)) {
		int bw = rsize < 0x8000 ? rsize : 0x8000;
		const char *tbuff;

	    if((tbuff = fmap_need_off_once(map, exe_sections[nsections - 1].raw + rsize - bw, 4096))) {
		if(cli_memstr(tbuff, 4091, "\xe8\x04\x72\x00\x00", 5)) {
		    cli_append_virus(ctx,dam ? "Heuristics.W32.Magistr.B.dam" : "Heuristics.W32.Magistr.B");
		    if (!SCAN_ALL) {
		        free(exe_sections);
			return CL_VIRUS;
		    }
		    viruses_found++;
		} 
	    }
	}
    }

    /* W32.Polipos.A */
    while(polipos && !dll && nsections > 2 && nsections < 13 && e_lfanew <= 0x800 && (EC16(optional_hdr32.Subsystem) == 2 || EC16(optional_hdr32.Subsystem) == 3) && EC16(file_hdr.Machine) == 0x14c && optional_hdr32.SizeOfStackReserve >= 0x80000) {
	uint32_t jump, jold, *jumps = NULL;
	const uint8_t *code;
	unsigned int xsjs = 0;

	if(exe_sections[0].rsz > CLI_MAX_ALLOCATION) break;

	if(!exe_sections[0].rsz) break;
	if(!(code=fmap_need_off_once(map, exe_sections[0].raw, exe_sections[0].rsz))) break;
	for(i=0; i<exe_sections[0].rsz - 5; i++) {
	    if((uint8_t)(code[i]-0xe8) > 1) continue;
	    jump = cli_rawaddr(exe_sections[0].rva+i+5+cli_readint32(&code[i+1]), exe_sections, nsections, &err, fsize, hdr_size);
	    if(err || !CLI_ISCONTAINED(exe_sections[polipos].raw, exe_sections[polipos].rsz, jump, 9)) continue;
	    if(xsjs % 128 == 0) {
		if(xsjs == 1280) break;
		if(!(jumps=(uint32_t *)cli_realloc2(jumps, (xsjs+128)*sizeof(uint32_t)))) {
		    free(exe_sections);
		    return CL_EMEM;
		}
	    }
	    j=0;
	    for(; j<xsjs; j++) {
		if(jumps[j]<jump) continue;
		if(jumps[j]==jump) {
		    xsjs--;
		    break;
		}
		jold=jumps[j];
		jumps[j]=jump;
		jump=jold;
	    }
	    jumps[j]=jump;
	    xsjs++;
	}
	if(!xsjs) break;
	cli_dbgmsg("Polipos: Checking %d xsect jump(s)\n", xsjs);
	for(i=0;i<xsjs;i++) {
	    if(!(code = fmap_need_off_once(map, jumps[i], 9))) continue;
	    if((jump=cli_readint32(code))==0x60ec8b55 || (code[4]==0x0ec && ((jump==0x83ec8b55 && code[6]==0x60) || (jump==0x81ec8b55 && !code[7] && !code[8])))) {
		cli_append_virus(ctx,"Heuristics.W32.Polipos.A");
		if (!SCAN_ALL) {
		    free(jumps);
		    free(exe_sections);
		    return CL_VIRUS;
		}
		viruses_found++;
	    }
	}
	free(jumps);
	break;
    }

    /* Trojan.Swizzor.Gen */
    if (SCAN_ALGO && (DCONF & PE_CONF_SWIZZOR) && nsections > 1 && fsize > 64*1024 && fsize < 4*1024*1024) {
	    if(dirs[2].Size) {
		    struct swizz_stats *stats = cli_calloc(1, sizeof(*stats));
		    unsigned int m = 1000;
		    ret = CL_CLEAN;

		    if (!stats)
			    ret = CL_EMEM;
		    else {
			    cli_parseres_special(EC32(dirs[2].VirtualAddress), EC32(dirs[2].VirtualAddress), map, exe_sections, nsections, fsize, hdr_size, 0, 0, &m, stats);
			    if ((ret = cli_detect_swizz(stats)) == CL_VIRUS) {
				cli_append_virus(ctx,"Heuristics.Trojan.Swizzor.Gen");
			    }
			    free(stats);
		    }
		    if (ret != CL_CLEAN) {
			if (!(ret == CL_VIRUS && SCAN_ALL)) {
			    free(exe_sections);
			    return ret;
			}
			viruses_found++;
		    }
	    }
    }


    /* !!!!!!!!!!!!!!    PACKERS START HERE    !!!!!!!!!!!!!! */
    corrupted_cur = ctx->corrupted_input;
    ctx->corrupted_input = 2; /* caller will reset on return */


    /* UPX, FSG, MEW support */

    /* try to find the first section with physical size == 0 */
    found = 0;
    if(DCONF & (PE_CONF_UPX | PE_CONF_FSG | PE_CONF_MEW)) {
	for(i = 0; i < (unsigned int) nsections - 1; i++) {
	    if(!exe_sections[i].rsz && exe_sections[i].vsz && exe_sections[i + 1].rsz && exe_sections[i + 1].vsz) {
		found = 1;
		cli_dbgmsg("UPX/FSG/MEW: empty section found - assuming compression\n");
#if HAVE_JSON
        cli_jsonbool(pe_json, "HasEmptySection", 1);
#endif
		break;
	    }
	}
    }

    /* MEW support */
    if (found && (DCONF & PE_CONF_MEW) && epsize>=16 && epbuff[0]=='\xe9') {
	uint32_t fileoffset;
	const char *tbuff;

	fileoffset = (vep + cli_readint32(epbuff + 1) + 5);
	while (fileoffset == 0x154 || fileoffset == 0x158) {
	    char *src;
	    uint32_t offdiff, uselzma;

	    cli_dbgmsg ("MEW: found MEW characteristics %08X + %08X + 5 = %08X\n", 
			cli_readint32(epbuff + 1), vep, cli_readint32(epbuff + 1) + vep + 5);

	    if(!(tbuff = fmap_need_off_once(map, fileoffset, 0xb0)))
		break;
	    if (fileoffset == 0x154) cli_dbgmsg("MEW: Win9x compatibility was set!\n");
	    else cli_dbgmsg("MEW: Win9x compatibility was NOT set!\n");

	    if((offdiff = cli_readint32(tbuff+1) - EC32(optional_hdr32.ImageBase)) <= exe_sections[i + 1].rva || offdiff >= exe_sections[i + 1].rva + exe_sections[i + 1].raw - 4) {
	        cli_dbgmsg("MEW: ESI is not in proper section\n");
		break;
	    }
	    offdiff -= exe_sections[i + 1].rva;

	    if(!exe_sections[i + 1].rsz) {
		cli_dbgmsg("MEW: mew section is empty\n");
		break;
	    }
	    ssize = exe_sections[i + 1].vsz;
	    dsize = exe_sections[i].vsz;

	    cli_dbgmsg("MEW: ssize %08x dsize %08x offdiff: %08x\n", ssize, dsize, offdiff);

	    CLI_UNPSIZELIMITS("MEW", MAX(ssize, dsize));
	    CLI_UNPSIZELIMITS("MEW", MAX(ssize + dsize, exe_sections[i + 1].rsz));

	    if (exe_sections[i + 1].rsz < offdiff + 12 || exe_sections[i + 1].rsz > ssize) {
	        cli_dbgmsg("MEW: Size mismatch: %08x\n", exe_sections[i + 1].rsz);
		break;
	    }

	    /* allocate needed buffer */
	    if (!(src = cli_calloc (ssize + dsize, sizeof(char)))) {
	        free(exe_sections);
		return CL_EMEM;
	    }

	    if((bytes = fmap_readn(map, src + dsize, exe_sections[i + 1].raw, exe_sections[i + 1].rsz)) != exe_sections[i + 1].rsz) {
		cli_dbgmsg("MEW: Can't read %d bytes [read: %lu]\n", exe_sections[i + 1].rsz, (unsigned long)bytes);
		free(exe_sections);
		free(src);
		return CL_EREAD;
	    }
	    cli_dbgmsg("MEW: %u (%08x) bytes read\n", (unsigned int)bytes, (unsigned int)bytes);

	    /* count offset to lzma proc, if lzma used, 0xe8 -> call */
	    if (tbuff[0x7b] == '\xe8') {
	        if (!CLI_ISCONTAINED(exe_sections[1].rva, exe_sections[1].vsz, cli_readint32(tbuff + 0x7c) + fileoffset + 0x80, 4)) {
		    cli_dbgmsg("MEW: lzma proc out of bounds!\n");
		    free(src);
		    break; /* to next unpacker in chain */
		}
		uselzma = cli_readint32(tbuff + 0x7c) - (exe_sections[0].rva - fileoffset - 0x80);
	    } else {
	        uselzma = 0;
	    }

#if HAVE_JSON
        cli_jsonstr(pe_json, "Packer", "MEW");
#endif

	    CLI_UNPTEMP("MEW",(src,exe_sections,0));
	    CLI_UNPRESULTS("MEW",(unmew11(src, offdiff, ssize, dsize, EC32(optional_hdr32.ImageBase), exe_sections[0].rva, uselzma, ndesc)),1,(src,0));
	    break;
	}
    }

    if(epsize<168) {
	free(exe_sections);
	return CL_CLEAN;
    }

    if (found || upack) {
	/* Check EP for UPX vs. FSG vs. Upack */

	/* Upack 0.39 produces 2 types of executables
	 * 3 sections:           | 2 sections (one empty, I don't chech found if !upack, since it's in OR above):
	 *   mov esi, value      |   pusha
	 *   lodsd               |   call $+0x9
	 *   push eax            |
	 *
	 * Upack 1.1/1.2 Beta produces [based on 2 samples (sUx) provided by aCaB]:
	 * 2 sections
	 *   mov esi, value
	 *   loads
	 *   mov edi, eax
	 *
	 * Upack unknown [sample 0297729]
	 * 3 sections
	 *   mov esi, value
	 *   push [esi]
	 *   jmp
	 * 
	 */
	/* upack 0.39-3s + sample 0151477*/
 	while(((upack && nsections == 3) && /* 3 sections */
	    ((
	     epbuff[0] == '\xbe' && cli_readint32(epbuff + 1) - EC32(optional_hdr32.ImageBase) > min && /* mov esi */
	     epbuff[5] == '\xad' && epbuff[6] == '\x50' /* lodsd; push eax */
	     )
	    || 
	    /* based on 0297729 sample from aCaB */
	    (epbuff[0] == '\xbe' && cli_readint32(epbuff + 1) - EC32(optional_hdr32.ImageBase) > min && /* mov esi */
	     epbuff[5] == '\xff' && epbuff[6] == '\x36' /* push [esi] */
	     )
	   )) 
	   ||
	   ((!upack && nsections == 2) && /* 2 sections */
	    (( /* upack 0.39-2s */
	     epbuff[0] == '\x60' && epbuff[1] == '\xe8' && cli_readint32(epbuff+2) == 0x9 /* pusha; call+9 */
	     )
	    ||
	    ( /* upack 1.1/1.2, based on 2 samples */
	     epbuff[0] == '\xbe' && cli_readint32(epbuff+1) - EC32(optional_hdr32.ImageBase) < min &&  /* mov esi */
	     cli_readint32(epbuff + 1) - EC32(optional_hdr32.ImageBase) > 0 &&
	     epbuff[5] == '\xad' && epbuff[6] == '\x8b' && epbuff[7] == '\xf8' /* loads;  mov edi, eax */
	     )
	   ))
	   ) { 
	    uint32_t vma, off;
	    int a,b,c;

	    cli_dbgmsg("Upack characteristics found.\n");
	    a = exe_sections[0].vsz;
	    b = exe_sections[1].vsz;
	    if (upack) {
	        cli_dbgmsg("Upack: var set\n");
		c = exe_sections[2].vsz;
		ssize = exe_sections[0].ursz + exe_sections[0].uraw;
		off = exe_sections[0].rva;
		vma = EC32(optional_hdr32.ImageBase) + exe_sections[0].rva;
	    } else {
	        cli_dbgmsg("Upack: var NOT set\n");
		c = exe_sections[1].rva;
		ssize = exe_sections[1].uraw;
		off = 0;
		vma = exe_sections[1].rva - exe_sections[1].uraw;
	    }

	    dsize = a+b+c;

	    CLI_UNPSIZELIMITS("Upack", MAX(MAX(dsize, ssize), exe_sections[1].ursz));

	    if (!CLI_ISCONTAINED(0, dsize, exe_sections[1].rva - off, exe_sections[1].ursz) || (upack && !CLI_ISCONTAINED(0, dsize, exe_sections[2].rva - exe_sections[0].rva, ssize)) || ssize > dsize) {
	        cli_dbgmsg("Upack: probably malformed pe-header, skipping to next unpacker\n");
		break;
	    }
			
	    if((dest = (char *) cli_calloc(dsize, sizeof(char))) == NULL) {
	        free(exe_sections);
		return CL_EMEM;
	    }

	    if((unsigned int)fmap_readn(map, dest, 0, ssize) != ssize) {
	        cli_dbgmsg("Upack: Can't read raw data of section 0\n");
		free(dest);
		break;
	    }

	    if(upack) memmove(dest + exe_sections[2].rva - exe_sections[0].rva, dest, ssize);

	    if((unsigned int)fmap_readn(map, dest + exe_sections[1].rva - off, exe_sections[1].uraw, exe_sections[1].ursz) != exe_sections[1].ursz) {
		cli_dbgmsg("Upack: Can't read raw data of section 1\n");
		free(dest);
		break;
	    }

#if HAVE_JSON
        cli_jsonstr(pe_json, "Packer", "Upack");
#endif

	    CLI_UNPTEMP("Upack",(dest,exe_sections,0));
	    CLI_UNPRESULTS("Upack",(unupack(upack, dest, dsize, epbuff, vma, ep, EC32(optional_hdr32.ImageBase), exe_sections[0].rva, ndesc)),1,(dest,0));
	    break;
	}
    }

    
    while(found  && (DCONF & PE_CONF_FSG) && epbuff[0] == '\x87' && epbuff[1] == '\x25') {
	const char *dst;

	/* FSG v2.0 support - thanks to aCaB ! */

	uint32_t newesi, newedi, newebx, newedx;
	
	ssize = exe_sections[i + 1].rsz;
	dsize = exe_sections[i].vsz;

	CLI_UNPSIZELIMITS("FSG", MAX(dsize, ssize));

	if(ssize <= 0x19 || dsize <= ssize) {
	    cli_dbgmsg("FSG: Size mismatch (ssize: %d, dsize: %d)\n", ssize, dsize);
	    free(exe_sections);
	    return CL_CLEAN;
	}
	
	newedx = cli_readint32(epbuff + 2) - EC32(optional_hdr32.ImageBase);
	if(!CLI_ISCONTAINED(exe_sections[i + 1].rva, exe_sections[i + 1].rsz, newedx, 4)) {
	    cli_dbgmsg("FSG: xchg out of bounds (%x), giving up\n", newedx);
	    break;
	}
	
	if(!exe_sections[i + 1].rsz || !(src = fmap_need_off_once(map, exe_sections[i + 1].raw, ssize))) {
	    cli_dbgmsg("Can't read raw data of section %d\n", i + 1);
	    free(exe_sections);
	    return CL_ESEEK;
	}

	dst = src + newedx - exe_sections[i + 1].rva;
	if(newedx < exe_sections[i + 1].rva || !CLI_ISCONTAINED(src, ssize, dst, 4)) {
	    cli_dbgmsg("FSG: New ESP out of bounds\n");
	    break;
	}

	newedx = cli_readint32(dst) - EC32(optional_hdr32.ImageBase);
	if(!CLI_ISCONTAINED(exe_sections[i + 1].rva, exe_sections[i + 1].rsz, newedx, 4)) {
	    cli_dbgmsg("FSG: New ESP (%x) is wrong\n", newedx);
	    break;
	}
 
	dst = src + newedx - exe_sections[i + 1].rva;
	if(!CLI_ISCONTAINED(src, ssize, dst, 32)) {
	    cli_dbgmsg("FSG: New stack out of bounds\n");
	    break;
	}

	newedi = cli_readint32(dst) - EC32(optional_hdr32.ImageBase);
	newesi = cli_readint32(dst + 4) - EC32(optional_hdr32.ImageBase);
	newebx = cli_readint32(dst + 16) - EC32(optional_hdr32.ImageBase);
	newedx = cli_readint32(dst + 20);

	if(newedi != exe_sections[i].rva) {
	    cli_dbgmsg("FSG: Bad destination buffer (edi is %x should be %x)\n", newedi, exe_sections[i].rva);
	    break;
	}

	if(newesi < exe_sections[i + 1].rva || newesi - exe_sections[i + 1].rva >= exe_sections[i + 1].rsz) {
	    cli_dbgmsg("FSG: Source buffer out of section bounds\n");
	    break;
	}

	if(!CLI_ISCONTAINED(exe_sections[i + 1].rva, exe_sections[i + 1].rsz, newebx, 16)) {
	    cli_dbgmsg("FSG: Array of functions out of bounds\n");
	    break;
	}

	newedx=cli_readint32(newebx + 12 - exe_sections[i + 1].rva + src) - EC32(optional_hdr32.ImageBase);
	cli_dbgmsg("FSG: found old EP @%x\n",newedx);

	if((dest = (char *) cli_calloc(dsize, sizeof(char))) == NULL) {
	    free(exe_sections);
	    return CL_EMEM;
	}

#if HAVE_JSON
    cli_jsonstr(pe_json, "Packer", "FSG");
#endif

	CLI_UNPTEMP("FSG",(dest,exe_sections,0));
	CLI_UNPRESULTSFSG2("FSG",(unfsg_200(newesi - exe_sections[i + 1].rva + src, dest, ssize + exe_sections[i + 1].rva - newesi, dsize, newedi, EC32(optional_hdr32.ImageBase), newedx, ndesc)),1,(dest,0));
	break;
    }


    while(found && (DCONF & PE_CONF_FSG) && epbuff[0] == '\xbe' && cli_readint32(epbuff + 1) - EC32(optional_hdr32.ImageBase) < min) {

	/* FSG support - v. 1.33 (thx trog for the many samples) */

	int sectcnt = 0;
	const char *support;
	uint32_t newesi, newedi, oldep, gp, t;
	struct cli_exe_section *sections;

	ssize = exe_sections[i + 1].rsz;
	dsize = exe_sections[i].vsz;

	CLI_UNPSIZELIMITS("FSG", MAX(dsize, ssize));

	if(ssize <= 0x19 || dsize <= ssize) {
	    cli_dbgmsg("FSG: Size mismatch (ssize: %d, dsize: %d)\n", ssize, dsize);
	    free(exe_sections);
	    return CL_CLEAN;
	}

	if(!(t = cli_rawaddr(cli_readint32(epbuff + 1) - EC32(optional_hdr32.ImageBase), NULL, 0 , &err, fsize, hdr_size)) && err ) {
	    cli_dbgmsg("FSG: Support data out of padding area\n");
	    break;
	}

	gp = exe_sections[i + 1].raw - t;

	CLI_UNPSIZELIMITS("FSG", gp);

	if(!(support = fmap_need_off_once(map, t, gp))) {
	    cli_dbgmsg("Can't read %d bytes from padding area\n", gp); 
	    free(exe_sections);
	    return CL_EREAD;
	}

	/* newebx = cli_readint32(support) - EC32(optional_hdr32.ImageBase);  Unused */
	newedi = cli_readint32(support + 4) - EC32(optional_hdr32.ImageBase); /* 1st dest */
	newesi = cli_readint32(support + 8) - EC32(optional_hdr32.ImageBase); /* Source */

	if(newesi < exe_sections[i + 1].rva || newesi - exe_sections[i + 1].rva >= exe_sections[i + 1].rsz) {
	    cli_dbgmsg("FSG: Source buffer out of section bounds\n");
	    break;
	}

	if(newedi != exe_sections[i].rva) {
	    cli_dbgmsg("FSG: Bad destination (is %x should be %x)\n", newedi, exe_sections[i].rva);
	    break;
	}

	/* Counting original sections */
	for(t = 12; t < gp - 4; t += 4) {
	    uint32_t rva = cli_readint32(support+t);

	    if(!rva)
		break;

	    rva -= EC32(optional_hdr32.ImageBase)+1;
	    sectcnt++;

	    if(rva % 0x1000) cli_dbgmsg("FSG: Original section %d is misaligned\n", sectcnt);

	    if(rva < exe_sections[i].rva || rva - exe_sections[i].rva >= exe_sections[i].vsz) {
		cli_dbgmsg("FSG: Original section %d is out of bounds\n", sectcnt);
		break;
	    }
	}

	if(t >= gp - 4 || cli_readint32(support + t)) {
	    break;
	}

	if((sections = (struct cli_exe_section *) cli_malloc((sectcnt + 1) * sizeof(struct cli_exe_section))) == NULL) {
        cli_errmsg("FSG: Unable to allocate memory for sections %lu\n", (sectcnt + 1) * sizeof(struct cli_exe_section));
	    free(exe_sections);
	    return CL_EMEM;
	}

	sections[0].rva = newedi;
	for(t = 1; t <= (uint32_t)sectcnt; t++)
	    sections[t].rva = cli_readint32(support + 8 + t * 4) - 1 - EC32(optional_hdr32.ImageBase);

	if(!exe_sections[i + 1].rsz || !(src = fmap_need_off_once(map, exe_sections[i + 1].raw, ssize))) {
	    cli_dbgmsg("Can't read raw data of section %d\n", i);
	    free(exe_sections);
	    free(sections);
	    return CL_EREAD;
	}

	if((dest = (char *) cli_calloc(dsize, sizeof(char))) == NULL) {
	    free(exe_sections);
	    free(sections);
	    return CL_EMEM;
	}

	oldep = vep + 161 + 6 + cli_readint32(epbuff+163);
	cli_dbgmsg("FSG: found old EP @%x\n", oldep);

#if HAVE_JSON
    cli_jsonstr(pe_json, "Packer", "FSG");
#endif

	CLI_UNPTEMP("FSG",(dest,sections,exe_sections,0));
	CLI_UNPRESULTSFSG1("FSG",(unfsg_133(src + newesi - exe_sections[i + 1].rva, dest, ssize + exe_sections[i + 1].rva - newesi, dsize, sections, sectcnt, EC32(optional_hdr32.ImageBase), oldep, ndesc)),1,(dest,sections,0));
	break; /* were done with 1.33 */
    }


    while(found && (DCONF & PE_CONF_FSG) && epbuff[0] == '\xbb' && cli_readint32(epbuff + 1) - EC32(optional_hdr32.ImageBase) < min && epbuff[5] == '\xbf' && epbuff[10] == '\xbe' && vep >= exe_sections[i + 1].rva && vep - exe_sections[i + 1].rva > exe_sections[i + 1].rva - 0xe0 ) {

	/* FSG support - v. 1.31 */

	int sectcnt = 0;
	uint32_t gp, t = cli_rawaddr(cli_readint32(epbuff+1) - EC32(optional_hdr32.ImageBase), NULL, 0 , &err, fsize, hdr_size);
	const char *support;
	uint32_t newesi = cli_readint32(epbuff+11) - EC32(optional_hdr32.ImageBase);
	uint32_t newedi = cli_readint32(epbuff+6) - EC32(optional_hdr32.ImageBase);
	uint32_t oldep = vep - exe_sections[i + 1].rva;
	struct cli_exe_section *sections;

	ssize = exe_sections[i + 1].rsz;
	dsize = exe_sections[i].vsz;

	if(err) {
	    cli_dbgmsg("FSG: Support data out of padding area\n");
	    break;
	}

	if(newesi < exe_sections[i + 1].rva || newesi - exe_sections[i + 1].rva >= exe_sections[i + 1].raw) {
	    cli_dbgmsg("FSG: Source buffer out of section bounds\n");
	    break;
	}

	if(newedi != exe_sections[i].rva) {
	    cli_dbgmsg("FSG: Bad destination (is %x should be %x)\n", newedi, exe_sections[i].rva);
	    break;
	}

	CLI_UNPSIZELIMITS("FSG", MAX(dsize, ssize));

	if(ssize <= 0x19 || dsize <= ssize) {
	    cli_dbgmsg("FSG: Size mismatch (ssize: %d, dsize: %d)\n", ssize, dsize);
	    free(exe_sections);
	    return CL_CLEAN;
	}

	gp = exe_sections[i + 1].raw - t;

	CLI_UNPSIZELIMITS("FSG", gp)

	if(!(support = fmap_need_off_once(map, t, gp))) {
	    cli_dbgmsg("Can't read %d bytes from padding area\n", gp); 
	    free(exe_sections);
	    return CL_EREAD;
	}

	/* Counting original sections */
	for(t = 0; t < gp - 2; t += 2) {
	    uint32_t rva = support[t]|(support[t+1]<<8);

	    if (rva == 2 || rva == 1)
		break;

	    rva = ((rva-2)<<12) - EC32(optional_hdr32.ImageBase);
	    sectcnt++;

	    if(rva < exe_sections[i].rva || rva - exe_sections[i].rva >= exe_sections[i].vsz) {
		cli_dbgmsg("FSG: Original section %d is out of bounds\n", sectcnt);
		break;
	    }
	}

	if(t >= gp-10 || cli_readint32(support + t + 6) != 2) {
	    break;
	}

	if((sections = (struct cli_exe_section *) cli_malloc((sectcnt + 1) * sizeof(struct cli_exe_section))) == NULL) {
        cli_errmsg("FSG: Unable to allocate memory for sections %lu\n", (sectcnt + 1) * sizeof(struct cli_exe_section));
	    free(exe_sections);
	    return CL_EMEM;
	}

	sections[0].rva = newedi;
	for(t = 0; t <= (uint32_t)sectcnt - 1; t++) {
	    sections[t+1].rva = (((support[t*2]|(support[t*2+1]<<8))-2)<<12)-EC32(optional_hdr32.ImageBase);
	}

	if(!exe_sections[i + 1].rsz || !(src = fmap_need_off_once(map, exe_sections[i + 1].raw, ssize))) {
	    cli_dbgmsg("FSG: Can't read raw data of section %d\n", i);
	    free(exe_sections);
	    free(sections);
	    return CL_EREAD;
	}

	if((dest = (char *) cli_calloc(dsize, sizeof(char))) == NULL) {
	    free(exe_sections);
	    free(sections);
	    return CL_EMEM;
	}

	gp = 0xda + 6*(epbuff[16]=='\xe8');
	oldep = vep + gp + 6 + cli_readint32(src+gp+2+oldep);
	cli_dbgmsg("FSG: found old EP @%x\n", oldep);

#if HAVE_JSON
    cli_jsonstr(pe_json, "Packer", "FSG");
#endif

	CLI_UNPTEMP("FSG",(dest,sections,exe_sections,0));
	CLI_UNPRESULTSFSG1("FSG",(unfsg_133(src + newesi - exe_sections[i + 1].rva, dest, ssize + exe_sections[i + 1].rva - newesi, dsize, sections, sectcnt, EC32(optional_hdr32.ImageBase), oldep, ndesc)),1,(dest,sections,0));
	break; /* were done with 1.31 */
    }


    if(found && (DCONF & PE_CONF_UPX)) {

	/* UPX support */

	/* we assume (i + 1) is UPX1 */
	ssize = exe_sections[i + 1].rsz;
	dsize = exe_sections[i].vsz + exe_sections[i + 1].vsz;

        /* cli_dbgmsg("UPX: ssize %u dsize %u\n", ssize, dsize); */

	CLI_UNPSIZELIMITS("UPX", MAX(dsize, ssize));

	if(ssize <= 0x19 || dsize <= ssize || dsize > CLI_MAX_ALLOCATION ) {
	    cli_dbgmsg("UPX: Size mismatch or dsize too big (ssize: %d, dsize: %d)\n", ssize, dsize);
	    free(exe_sections);
	    return CL_CLEAN;
	}

	if(!exe_sections[i + 1].rsz || !(src = fmap_need_off_once(map, exe_sections[i + 1].raw, ssize))) {
	    cli_dbgmsg("UPX: Can't read raw data of section %d\n", i+1);
	    free(exe_sections);
	    return CL_EREAD;
	}

	if((dest = (char *) cli_calloc(dsize + 8192, sizeof(char))) == NULL) {
	    free(exe_sections);
	    return CL_EMEM;
	}

	/* try to detect UPX code */
	if(cli_memstr(UPX_NRV2B, 24, epbuff + 0x69, 13) || cli_memstr(UPX_NRV2B, 24, epbuff + 0x69 + 8, 13)) {
	    cli_dbgmsg("UPX: Looks like a NRV2B decompression routine\n");
	    upxfn = upx_inflate2b;
	} else if(cli_memstr(UPX_NRV2D, 24, epbuff + 0x69, 13) || cli_memstr(UPX_NRV2D, 24, epbuff + 0x69 + 8, 13)) {
	    cli_dbgmsg("UPX: Looks like a NRV2D decompression routine\n");
	    upxfn = upx_inflate2d;
	} else if(cli_memstr(UPX_NRV2E, 24, epbuff + 0x69, 13) || cli_memstr(UPX_NRV2E, 24, epbuff + 0x69 + 8, 13)) {
	    cli_dbgmsg("UPX: Looks like a NRV2E decompression routine\n");
	    upxfn = upx_inflate2e;
	}

	if(upxfn) {
	    int skew = cli_readint32(epbuff + 2) - EC32(optional_hdr32.ImageBase) - exe_sections[i + 1].rva;

	    if(epbuff[1] != '\xbe' || skew <= 0 || skew > 0xfff) { /* FIXME: legit skews?? */
		skew = 0; 
	    } else if ((unsigned int)skew > ssize) {
		/* Ignore suggested skew larger than section size */
		skew = 0;
	    } else {
		cli_dbgmsg("UPX: UPX1 seems skewed by %d bytes\n", skew);
	    }

	    /* Try skewed first (skew may be zero) */
	    if(upxfn(src + skew, ssize - skew, dest, &dsize, exe_sections[i].rva, exe_sections[i + 1].rva, vep-skew) >= 0) {
		upx_success = 1;
	    }
	    /* If skew not successful and non-zero, try no skew */
	    else if(skew && (upxfn(src, ssize, dest, &dsize, exe_sections[i].rva, exe_sections[i + 1].rva, vep) >= 0)) {
		upx_success = 1;
	    }

	    if(upx_success)
		cli_dbgmsg("UPX: Successfully decompressed\n");
	    else
		cli_dbgmsg("UPX: Preferred decompressor failed\n");
	}

	if(!upx_success && upxfn != upx_inflate2b) {
	    if(upx_inflate2b(src, ssize, dest, &dsize, exe_sections[i].rva, exe_sections[i + 1].rva, vep) == -1 && upx_inflate2b(src + 0x15, ssize - 0x15, dest, &dsize, exe_sections[i].rva, exe_sections[i + 1].rva, vep - 0x15) == -1) {

		cli_dbgmsg("UPX: NRV2B decompressor failed\n");
	    } else {
		upx_success = 1;
		cli_dbgmsg("UPX: Successfully decompressed with NRV2B\n");
	    }
	}

	if(!upx_success && upxfn != upx_inflate2d) {
	    if(upx_inflate2d(src, ssize, dest, &dsize, exe_sections[i].rva, exe_sections[i + 1].rva, vep) == -1 && upx_inflate2d(src + 0x15, ssize - 0x15, dest, &dsize, exe_sections[i].rva, exe_sections[i + 1].rva, vep - 0x15) == -1) {

		cli_dbgmsg("UPX: NRV2D decompressor failed\n");
	    } else {
		upx_success = 1;
		cli_dbgmsg("UPX: Successfully decompressed with NRV2D\n");
	    }
	}

	if(!upx_success && upxfn != upx_inflate2e) {
	    if(upx_inflate2e(src, ssize, dest, &dsize, exe_sections[i].rva, exe_sections[i + 1].rva, vep) == -1 && upx_inflate2e(src + 0x15, ssize - 0x15, dest, &dsize, exe_sections[i].rva, exe_sections[i + 1].rva, vep - 0x15) == -1) {
		cli_dbgmsg("UPX: NRV2E decompressor failed\n");
	    } else {
		upx_success = 1;
		cli_dbgmsg("UPX: Successfully decompressed with NRV2E\n");
	    }
	}

	if(cli_memstr(UPX_LZMA2, 20, epbuff + 0x2f, 20)) {
	    uint32_t strictdsize=cli_readint32(epbuff+0x21), skew = 0;
	    if(ssize > 0x15 && epbuff[0] == '\x60' && epbuff[1] == '\xbe') {
		skew = cli_readint32(epbuff+2) - exe_sections[i + 1].rva - optional_hdr32.ImageBase;
		if(skew!=0x15) skew = 0;
	    }
	    if(strictdsize<=dsize)
		upx_success = upx_inflatelzma(src+skew, ssize-skew, dest, &strictdsize, exe_sections[i].rva, exe_sections[i + 1].rva, vep) >=0;
	} else if (cli_memstr(UPX_LZMA1, 20, epbuff + 0x39, 20)) {
	    uint32_t strictdsize=cli_readint32(epbuff+0x2b), skew = 0;
	    if(ssize > 0x15 && epbuff[0] == '\x60' && epbuff[1] == '\xbe') {
		skew = cli_readint32(epbuff+2) - exe_sections[i + 1].rva - optional_hdr32.ImageBase;
		if(skew!=0x15) skew = 0;
	    }
	    if(strictdsize<=dsize)
		upx_success = upx_inflatelzma(src+skew, ssize-skew, dest, &strictdsize, exe_sections[i].rva, exe_sections[i + 1].rva, vep) >=0;
	}

	if(!upx_success) {
	    cli_dbgmsg("UPX: All decompressors failed\n");
	    free(dest);
	}
    }

    if(upx_success) {
	free(exe_sections);

	CLI_UNPTEMP("UPX/FSG",(dest,0));
#if HAVE_JSON
    cli_jsonstr(pe_json, "Packer", "UPX");
#endif

	if((unsigned int) write(ndesc, dest, dsize) != dsize) {
	    cli_dbgmsg("UPX/FSG: Can't write %d bytes\n", dsize);
	    free(tempfile);
	    free(dest);
	    close(ndesc);
	    return CL_EWRITE;
	}

	free(dest);
	if (lseek(ndesc, 0, SEEK_SET) == -1) {
        cli_dbgmsg("UPX/FSG: lseek() failed\n");
        close(ndesc);
        CLI_TMPUNLK();
        free(tempfile);
        SHA_RESET;
        return CL_ESEEK;
    }

	if(ctx->engine->keeptmp)
	    cli_dbgmsg("UPX/FSG: Decompressed data saved in %s\n", tempfile);

	cli_dbgmsg("***** Scanning decompressed file *****\n");
	SHA_OFF;
	if((ret = cli_magic_scandesc(ndesc, ctx)) == CL_VIRUS) {
	    close(ndesc);
	    CLI_TMPUNLK();
	    free(tempfile);
	    SHA_RESET;
	    return CL_VIRUS;
	}

	SHA_RESET;
	close(ndesc);
	CLI_TMPUNLK();
	free(tempfile);
	return ret;
    }


    /* Petite */

    if(epsize<200) {
	free(exe_sections);
	return CL_CLEAN;
    }

    found = 2;

    if(epbuff[0] != '\xb8' || (uint32_t) cli_readint32(epbuff + 1) != exe_sections[nsections - 1].rva + EC32(optional_hdr32.ImageBase)) {
	if(nsections < 2 || epbuff[0] != '\xb8' || (uint32_t) cli_readint32(epbuff + 1) != exe_sections[nsections - 2].rva + EC32(optional_hdr32.ImageBase))
	    found = 0;
	else
	    found = 1;
    }

    if(found && (DCONF & PE_CONF_PETITE)) {
	cli_dbgmsg("Petite: v2.%d compression detected\n", found);

	if(cli_readint32(epbuff + 0x80) == 0x163c988d) {
	    cli_dbgmsg("Petite: level zero compression is not supported yet\n");
	} else {
	    dsize = max - min;

	    CLI_UNPSIZELIMITS("Petite", dsize);

	    if((dest = (char *) cli_calloc(dsize, sizeof(char))) == NULL) {
		cli_dbgmsg("Petite: Can't allocate %d bytes\n", dsize);
		free(exe_sections);
		return CL_EMEM;
	    }

	    for(i = 0 ; i < nsections; i++) {
		if(exe_sections[i].raw) {
		    if(!exe_sections[i].rsz || (unsigned int)fmap_readn(map, dest + exe_sections[i].rva - min, exe_sections[i].raw, exe_sections[i].ursz) != exe_sections[i].ursz) {
			free(exe_sections);
			free(dest);
			return CL_CLEAN;
		    }
		}
	    }

#if HAVE_JSON
        cli_jsonstr(pe_json, "Packer", "Petite");
#endif

	    CLI_UNPTEMP("Petite",(dest,exe_sections,0));
	    CLI_UNPRESULTS("Petite",(petite_inflate2x_1to9(dest, min, max - min, exe_sections, nsections - (found == 1 ? 1 : 0), EC32(optional_hdr32.ImageBase),vep, ndesc, found, EC32(optional_hdr32.DataDirectory[2].VirtualAddress),EC32(optional_hdr32.DataDirectory[2].Size))),0,(dest,0));
	}
    }

    /* PESpin 1.1 */

    if((DCONF & PE_CONF_PESPIN) && nsections > 1 &&
       vep >= exe_sections[nsections - 1].rva &&
       vep < exe_sections[nsections - 1].rva + exe_sections[nsections - 1].rsz - 0x3217 - 4 &&
       memcmp(epbuff+4, "\xe8\x00\x00\x00\x00\x8b\x1c\x24\x83\xc3", 10) == 0)  {

	char *spinned;

	CLI_UNPSIZELIMITS("PEspin", fsize);

	if((spinned = (char *) cli_malloc(fsize)) == NULL) {
        cli_errmsg("PESping: Unable to allocate memory for spinned %lu\n", (unsigned long)fsize);
	    free(exe_sections);
	    return CL_EMEM;
	}

	if((size_t) fmap_readn(map, spinned, 0, fsize) != fsize) {
	    cli_dbgmsg("PESpin: Can't read %lu bytes\n", (unsigned long)fsize);
	    free(spinned);
	    free(exe_sections);
	    return CL_EREAD;
	}

#if HAVE_JSON
    cli_jsonstr(pe_json, "Packer", "PEspin");
#endif

	CLI_UNPTEMP("PESpin",(spinned,exe_sections,0));
	CLI_UNPRESULTS_("PEspin",SPINCASE(),(unspin(spinned, fsize, exe_sections, nsections - 1, vep, ndesc, ctx)),0,(spinned,0));
    }


    /* yC 1.3 & variants */
    if((DCONF & PE_CONF_YC) && nsections > 1 &&
       (EC32(optional_hdr32.AddressOfEntryPoint) == exe_sections[nsections - 1].rva + 0x60)) {

	uint32_t ecx = 0;
	int16_t offset;

	/* yC 1.3 */
	if (!memcmp(epbuff, "\x55\x8B\xEC\x53\x56\x57\x60\xE8\x00\x00\x00\x00\x5D\x81\xED", 15) &&
	    !memcmp(epbuff+0x26, "\x8D\x3A\x8B\xF7\x33\xC0\xEB\x04\x90\xEB\x01\xC2\xAC", 13) &&
	    ((uint8_t)epbuff[0x13] == 0xB9) &&
	    ((uint16_t)(cli_readint16(epbuff+0x18)) == 0xE981) &&
	    !memcmp(epbuff+0x1e,"\x8B\xD5\x81\xC2", 4)) {

	    offset = 0;
	    if (0x6c - cli_readint32(epbuff+0xf) + cli_readint32(epbuff+0x22) == 0xC6)
		ecx = cli_readint32(epbuff+0x14) - cli_readint32(epbuff+0x1a);
	}

	/* yC 1.3 variant */
	if (!ecx && !memcmp(epbuff, "\x55\x8B\xEC\x83\xEC\x40\x53\x56\x57", 9) &&
	    !memcmp(epbuff+0x17, "\xe8\x00\x00\x00\x00\x5d\x81\xed", 8) &&
	    ((uint8_t)epbuff[0x23] == 0xB9)) {

	    offset = 0x10;
	    if (0x6c - cli_readint32(epbuff+0x1f) + cli_readint32(epbuff+0x32) == 0xC6)
		ecx = cli_readint32(epbuff+0x24) - cli_readint32(epbuff+0x2a);
	}

	/* yC 1.x/modified */
	if (!ecx && !memcmp(epbuff, "\x60\xe8\x00\x00\x00\x00\x5d\x81\xed",9) &&
	    ((uint8_t)epbuff[0xd] == 0xb9) &&
	    ((uint16_t)cli_readint16(epbuff + 0x12)== 0xbd8d) &&
	    !memcmp(epbuff+0x18, "\x8b\xf7\xac", 3)) {

	    offset = -0x18;
	    if (0x66 - cli_readint32(epbuff+0x9) + cli_readint32(epbuff+0x14) == 0xae)
		ecx = cli_readint32(epbuff+0xe);
	}

	if (ecx > 0x800 && ecx < 0x2000 &&
	    !memcmp(epbuff+0x63+offset, "\xaa\xe2\xcc", 3) &&
	    (fsize >= exe_sections[nsections-1].raw + 0xC6 + ecx + offset)) {

	    char *spinned;

	    if((spinned = (char *) cli_malloc(fsize)) == NULL) {
            cli_errmsg("yC: Unable to allocate memory for spinned %lu\n", (unsigned long)fsize);
	      free(exe_sections);
	      return CL_EMEM;
	    }

	    if((size_t) fmap_readn(map, spinned, 0, fsize) != fsize) {
	      cli_dbgmsg("yC: Can't read %lu bytes\n", (unsigned long)fsize);
	      free(spinned);
	      free(exe_sections);
	      return CL_EREAD;
	    }

#if HAVE_JSON
        cli_jsonstr(pe_json, "Packer", "yC");
#endif

	    cli_dbgmsg("%d,%d,%d,%d\n", nsections-1, e_lfanew, ecx, offset);
	    CLI_UNPTEMP("yC",(spinned,exe_sections,0));
	    CLI_UNPRESULTS("yC",(yc_decrypt(spinned, fsize, exe_sections, nsections-1, e_lfanew, ndesc, ecx, offset)),0,(spinned,0));
	}
    }

    /* WWPack */

    while ((DCONF & PE_CONF_WWPACK) && nsections > 1 &&
       vep == exe_sections[nsections - 1].rva &&
       memcmp(epbuff, "\x53\x55\x8b\xe8\x33\xdb\xeb", 7) == 0 &&
       memcmp(epbuff+0x68, "\xe8\x00\x00\x00\x00\x58\x2d\x6d\x00\x00\x00\x50\x60\x33\xc9\x50\x58\x50\x50", 19) == 0)  {
	uint32_t head = exe_sections[nsections - 1].raw;
        uint8_t *packer;
	char *src;

	ssize = 0;
	for(i=0 ; ; i++) {
	    if(exe_sections[i].raw<head)
	        head=exe_sections[i].raw;
	    if(i+1==nsections) break;
	    if(ssize<exe_sections[i].rva+exe_sections[i].vsz)
		ssize=exe_sections[i].rva+exe_sections[i].vsz;
	}
	if(!head || !ssize || head>ssize) break;

	CLI_UNPSIZELIMITS("WWPack", ssize);

        if(!(src=(char *)cli_calloc(ssize, sizeof(char)))) {
	    free(exe_sections);
	    return CL_EMEM;
	}
	if((size_t) fmap_readn(map, src, 0, head) != head) {
	    cli_dbgmsg("WWPack: Can't read %d bytes from headers\n", head);
	    free(src);
	    free(exe_sections);
	    return CL_EREAD;
	}
        for(i = 0 ; i < (unsigned int)nsections-1; i++) {
	    if(!exe_sections[i].rsz) continue;
            if(!CLI_ISCONTAINED(src, ssize, src+exe_sections[i].rva, exe_sections[i].rsz)) break;
            if((unsigned int)fmap_readn(map, src+exe_sections[i].rva, exe_sections[i].raw, exe_sections[i].rsz)!=exe_sections[i].rsz) break;
        }
        if(i+1!=nsections) {
            cli_dbgmsg("WWpack: Probably hacked/damaged file.\n");
            free(src);
            break;
        }
	if((packer = (uint8_t *) cli_calloc(exe_sections[nsections - 1].rsz, sizeof(char))) == NULL) {
	    free(src);
	    free(exe_sections);
	    return CL_EMEM;
	}
	if(!exe_sections[nsections - 1].rsz || (size_t) fmap_readn(map, packer, exe_sections[nsections - 1].raw, exe_sections[nsections - 1].rsz) != exe_sections[nsections - 1].rsz) {
	    cli_dbgmsg("WWPack: Can't read %d bytes from wwpack sect\n", exe_sections[nsections - 1].rsz);
	    free(src);
	    free(packer);
	    free(exe_sections);
	    return CL_EREAD;
	}

#if HAVE_JSON
    cli_jsonstr(pe_json, "Packer", "WWPack");
#endif

	CLI_UNPTEMP("WWPack",(src,packer,exe_sections,0));
	CLI_UNPRESULTS("WWPack",(wwunpack((uint8_t *)src, ssize, packer, exe_sections, nsections-1, e_lfanew, ndesc)),0,(src,packer,0));
	break;
    }


    /* ASPACK support */
    while((DCONF & PE_CONF_ASPACK) && ep+58+0x70e < fsize && !memcmp(epbuff,"\x60\xe8\x03\x00\x00\x00\xe9\xeb",8)) {
	char *src;

        if(epsize<0x3bf || memcmp(epbuff+0x3b9, "\x68\x00\x00\x00\x00\xc3",6)) break;
	ssize = 0;
	for(i=0 ; i< nsections ; i++)
	    if(ssize<exe_sections[i].rva+exe_sections[i].vsz)
		ssize=exe_sections[i].rva+exe_sections[i].vsz;
	if(!ssize) break;

	CLI_UNPSIZELIMITS("Aspack", ssize);

        if(!(src=(char *)cli_calloc(ssize, sizeof(char)))) {
	    free(exe_sections);
	    return CL_EMEM;
	}
        for(i = 0 ; i < (unsigned int)nsections; i++) {
	    if(!exe_sections[i].rsz) continue;
            if(!CLI_ISCONTAINED(src, ssize, src+exe_sections[i].rva, exe_sections[i].rsz)) break;
            if((unsigned int)fmap_readn(map, src+exe_sections[i].rva, exe_sections[i].raw, exe_sections[i].rsz)!=exe_sections[i].rsz) break;
        }
        if(i!=nsections) {
            cli_dbgmsg("Aspack: Probably hacked/damaged Aspack file.\n");
            free(src);
            break;
        }

#if HAVE_JSON
        cli_jsonstr(pe_json, "Packer", "Aspack");
#endif

	CLI_UNPTEMP("Aspack",(src,exe_sections,0));
	CLI_UNPRESULTS("Aspack",(unaspack212((uint8_t *)src, ssize, exe_sections, nsections, vep-1, EC32(optional_hdr32.ImageBase), ndesc)),1,(src,0));
	break;
    }

    /* NsPack */

    while (DCONF & PE_CONF_NSPACK) {
	uint32_t eprva = vep;
	uint32_t start_of_stuff, rep = ep;
	unsigned int nowinldr;
	const char *nbuff;

	src=epbuff;
	if (*epbuff=='\xe9') { /* bitched headers */
	    eprva = cli_readint32(epbuff+1)+vep+5;
	    if (!(rep = cli_rawaddr(eprva, exe_sections, nsections, &err, fsize, hdr_size)) && err) break;
	    if (!(nbuff = fmap_need_off_once(map, rep, 24))) break;
	    src = nbuff;
	}

	if (memcmp(src, "\x9c\x60\xe8\x00\x00\x00\x00\x5d\xb8\x07\x00\x00\x00", 13)) break;

	nowinldr = 0x54-cli_readint32(src+17);
	cli_dbgmsg("NsPack: Found *start_of_stuff @delta-%x\n", nowinldr);

	if(!(nbuff = fmap_need_off_once(map, rep-nowinldr, 4))) break;
	start_of_stuff=rep+cli_readint32(nbuff);
	if(!(nbuff = fmap_need_off_once(map, start_of_stuff, 20))) break;
	src = nbuff;
	if (!cli_readint32(nbuff)) {
	    start_of_stuff+=4; /* FIXME: more to do */
	    src+=4;
	}

	ssize = cli_readint32(src+5)|0xff;
	dsize = cli_readint32(src+9);

	CLI_UNPSIZELIMITS("NsPack", MAX(ssize,dsize));

	if (!ssize || !dsize || dsize != exe_sections[0].vsz) break;
	if (!(dest=cli_malloc(dsize))) {
        cli_errmsg("NsPack: Unable to allocate memory for dest %u\n", dsize);
        break;
    }
	/* memset(dest, 0xfc, dsize); */

	if(!(src = fmap_need_off(map, start_of_stuff, ssize))) {
	    free(dest);
	    break;
	}
	/* memset(src, 0x00, ssize); */

	eprva+=0x27a;
	if (!(rep = cli_rawaddr(eprva, exe_sections, nsections, &err, fsize, hdr_size)) && err) {
	  free(dest);
	  break;
	}
	if(!(nbuff = fmap_need_off_once(map, rep, 5))) {
	  free(dest);
	  break;
	}
	fmap_unneed_off(map, start_of_stuff, ssize);
	eprva=eprva+5+cli_readint32(nbuff+1);
	cli_dbgmsg("NsPack: OEP = %08x\n", eprva);

#if HAVE_JSON
    cli_jsonstr(pe_json, "Packer", "NsPack");
#endif

	CLI_UNPTEMP("NsPack",(dest,exe_sections,0));
	CLI_UNPRESULTS("NsPack",(unspack(src, dest, ctx, exe_sections[0].rva, EC32(optional_hdr32.ImageBase), eprva, ndesc)),0,(dest,0));
	break;
    }

    /* to be continued ... */




    /* !!!!!!!!!!!!!!    PACKERS END HERE    !!!!!!!!!!!!!! */
    ctx->corrupted_input = corrupted_cur;

    /* Bytecode BC_PE_UNPACKER hook */
    bc_ctx = cli_bytecode_context_alloc();
    if (!bc_ctx) {
	cli_errmsg("cli_scanpe: can't allocate memory for bc_ctx\n");
	return CL_EMEM;
    }
    cli_bytecode_context_setpe(bc_ctx, &pedata, exe_sections);
    cli_bytecode_context_setctx(bc_ctx, ctx);
    ret = cli_bytecode_runhook(ctx, ctx->engine, bc_ctx, BC_PE_UNPACKER, map);
    switch (ret) {
	case CL_VIRUS:
	    free(exe_sections);
	    cli_bytecode_context_destroy(bc_ctx);
	    return CL_VIRUS;
	case CL_SUCCESS:
	    ndesc = cli_bytecode_context_getresult_file(bc_ctx, &tempfile);
	    cli_bytecode_context_destroy(bc_ctx);
	    if (ndesc != -1 && tempfile) {
		CLI_UNPRESULTS("bytecode PE hook", 1, 1, (0));
	    }
	    break;
	default:
	    cli_bytecode_context_destroy(bc_ctx);
    }

    free(exe_sections);
#if HAVE_JSON
    if (cli_json_timeout_cycle_check(ctx, &toval) != CL_SUCCESS) {
        return CL_ETIMEOUT;
    }
#endif
    if (SCAN_ALL && viruses_found)
	return CL_VIRUS;
    return CL_CLEAN;