void PackLinuxElf64::unpack(OutputFile *fo)
{
    unsigned const c_phnum = get_te16(&ehdri.e_phnum);
    upx_uint64_t old_data_off = 0;
    upx_uint64_t old_data_len = 0;
    upx_uint64_t old_dtinit = 0;

    unsigned szb_info = sizeof(b_info);
    {
        upx_uint64_t const e_entry = get_te64(&ehdri.e_entry);
        if (e_entry < 0x401180
        &&  get_te16(&ehdri.e_machine)==Elf64_Ehdr::EM_386) { /* old style, 8-byte b_info */
            szb_info = 2*sizeof(unsigned);
        }
    }

    fi->seek(overlay_offset - sizeof(l_info), SEEK_SET);
    fi->readx(&linfo, sizeof(linfo));
    lsize = get_te16(&linfo.l_lsize);
    p_info hbuf;  fi->readx(&hbuf, sizeof(hbuf));
    unsigned orig_file_size = get_te32(&hbuf.p_filesize);
    blocksize = get_te32(&hbuf.p_blocksize);
    if (file_size > (off_t)orig_file_size || blocksize > orig_file_size
        || !mem_size_valid(1, blocksize, OVERHEAD))
        throwCantUnpack("p_info corrupted");

#define MAX_ELF_HDR 1024
    union {
        unsigned char buf[MAX_ELF_HDR];
        //struct { Elf64_Ehdr ehdr; Elf64_Phdr phdr; } e;
    } u;
    Elf64_Ehdr *const ehdr = (Elf64_Ehdr *) u.buf;
    Elf64_Phdr const *phdr = 0;

    ibuf.alloc(blocksize + OVERHEAD);
    b_info bhdr; memset(&bhdr, 0, sizeof(bhdr));
    fi->readx(&bhdr, szb_info);
    ph.u_len = get_te32(&bhdr.sz_unc);
    ph.c_len = get_te32(&bhdr.sz_cpr);
    if (ph.c_len > (unsigned)file_size || ph.c_len == 0 || ph.u_len == 0
    ||  ph.u_len > sizeof(u))
        throwCantUnpack("b_info corrupted");
    ph.filter_cto = bhdr.b_cto8;

    // Uncompress Ehdr and Phdrs.
    if (ibuf.getSize() < ph.c_len  ||  sizeof(u) < ph.u_len)
        throwCompressedDataViolation();
    fi->readx(ibuf, ph.c_len);
    decompress(ibuf, (upx_byte *)ehdr, false);
    if (ehdr->e_type   !=ehdri.e_type
    ||  ehdr->e_machine!=ehdri.e_machine
    ||  ehdr->e_version!=ehdri.e_version
        // less strict for EM_PPC64 to workaround earlier bug
    ||  !( ehdr->e_flags==ehdri.e_flags
        || Elf64_Ehdr::EM_PPC64 == get_te16(&ehdri.e_machine))
    ||  ehdr->e_ehsize !=ehdri.e_ehsize
        // check EI_MAG[0-3], EI_CLASS, EI_DATA, EI_VERSION
    ||  memcmp(ehdr->e_ident, ehdri.e_ident, Elf64_Ehdr::EI_OSABI)) {
        throwCantUnpack("ElfXX_Ehdr corrupted");
    }
    fi->seek(- (off_t) (szb_info + ph.c_len), SEEK_CUR);

    unsigned const u_phnum = get_te16(&ehdr->e_phnum);
    unsigned total_in = 0;
    unsigned total_out = 0;
    unsigned c_adler = upx_adler32(NULL, 0);
    unsigned u_adler = upx_adler32(NULL, 0);

    // Packed ET_EXE has no PT_DYNAMIC.
    // Packed ET_DYN has original PT_DYNAMIC for info needed by rtld.
    bool const is_shlib = !!elf_find_ptype(Elf64_Phdr::PT_DYNAMIC, phdri, c_phnum);
    if (is_shlib) {
        // Unpack and output the Ehdr and Phdrs for real.
        // This depends on position within input file fi.
        unpackExtent(ph.u_len, fo, total_in, total_out,
            c_adler, u_adler, false, szb_info);

        // The first PT_LOAD.  Part is not compressed (for benefit of rtld.)
        // Read enough to position the input for next unpackExtent.
        fi->seek(0, SEEK_SET);
        fi->readx(ibuf, overlay_offset + sizeof(hbuf) + szb_info + ph.c_len);
        overlay_offset -= sizeof(linfo);
        if (fo) {
            fo->write(ibuf + ph.u_len, overlay_offset - ph.u_len);
        }
        // Search the Phdrs of compressed
        int n_ptload = 0;
        phdr = (Elf64_Phdr *) (void *) (1+ (Elf64_Ehdr *)(unsigned char *)ibuf);
        for (unsigned j=0; j < u_phnum; ++phdr, ++j) {
            if (PT_LOAD64==get_te32(&phdr->p_type) && 0!=n_ptload++) {
                old_data_off = get_te64(&phdr->p_offset);
                old_data_len = get_te64(&phdr->p_filesz);
                break;
            }
        }

        total_in  = overlay_offset;
        total_out = overlay_offset;
        ph.u_len = 0;

        // Decompress and unfilter the tail of first PT_LOAD.
        phdr = (Elf64_Phdr *) (void *) (1+ ehdr);
        for (unsigned j=0; j < u_phnum; ++phdr, ++j) {
            if (PT_LOAD64==get_te32(&phdr->p_type)) {
                ph.u_len = get_te64(&phdr->p_filesz) - overlay_offset;
                break;
            }
        }
        unpackExtent(ph.u_len, fo, total_in, total_out,
            c_adler, u_adler, false, szb_info);
    }
    else {  // main executable
        // Decompress each PT_LOAD.
        bool first_PF_X = true;
        phdr = (Elf64_Phdr *) (void *) (1+ ehdr);  // uncompressed
        for (unsigned j=0; j < u_phnum; ++phdr, ++j) {
            if (PT_LOAD64==get_te32(&phdr->p_type)) {
                unsigned const filesz = get_te64(&phdr->p_filesz);
                unsigned const offset = get_te64(&phdr->p_offset);
                if (fo)
                    fo->seek(offset, SEEK_SET);
                if (Elf64_Phdr::PF_X & get_te32(&phdr->p_flags)) {
                    unpackExtent(filesz, fo, total_in, total_out,
                        c_adler, u_adler, first_PF_X, szb_info);
                    first_PF_X = false;
                }
                else {
                    unpackExtent(filesz, fo, total_in, total_out,
                        c_adler, u_adler, false, szb_info);
                }
            }
        }
    }
    phdr = phdri;
    load_va = 0;
    for (unsigned j=0; j < c_phnum; ++j) {
        if (PT_LOAD64==get_te32(&phdr->p_type)) {
            load_va = get_te64(&phdr->p_vaddr);
            break;
        }
    }
    if (is_shlib
    ||  ((unsigned)(get_te64(&ehdri.e_entry) - load_va) + up4(lsize) +
                ph.getPackHeaderSize() + sizeof(overlay_offset))
            < up4(file_size)) {
        // Loader is not at end; skip past it.
        funpad4(fi);  // MATCH01
        unsigned d_info[6]; fi->readx(d_info, sizeof(d_info));
        if (0==old_dtinit) {
            old_dtinit = d_info[2 + (0==d_info[0])];
        }
        fi->seek(lsize - sizeof(d_info), SEEK_CUR);
    }

    // The gaps between PT_LOAD and after last PT_LOAD
    phdr = (Elf64_Phdr *) (u.buf + sizeof(*ehdr));
    upx_uint64_t hi_offset(0);
    for (unsigned j = 0; j < u_phnum; ++j) {
        if (PT_LOAD64==phdr[j].p_type
        &&  hi_offset < phdr[j].p_offset)
            hi_offset = phdr[j].p_offset;
    }
    for (unsigned j = 0; j < u_phnum; ++j) {
        unsigned const size = find_LOAD_gap(phdr, j, u_phnum);
        if (size) {
            unsigned const where = get_te64(&phdr[j].p_offset) +
                                   get_te64(&phdr[j].p_filesz);
            if (fo)
                fo->seek(where, SEEK_SET);
            unpackExtent(size, fo, total_in, total_out,
                c_adler, u_adler, false, szb_info,
                (phdr[j].p_offset != hi_offset));
        }
    }

    // check for end-of-file
    fi->readx(&bhdr, szb_info);
    unsigned const sz_unc = ph.u_len = get_te32(&bhdr.sz_unc);

    if (sz_unc == 0) { // uncompressed size 0 -> EOF
        // note: magic is always stored le32
        unsigned const sz_cpr = get_le32(&bhdr.sz_cpr);
        if (sz_cpr != UPX_MAGIC_LE32)  // sz_cpr must be h->magic
            throwCompressedDataViolation();
    }
    else { // extra bytes after end?
        throwCompressedDataViolation();
    }

    if (is_shlib) {  // the non-first PT_LOAD
        int n_ptload = 0;
        unsigned load_off = 0;
        phdr = (Elf64_Phdr *) (u.buf + sizeof(*ehdr));
        for (unsigned j= 0; j < u_phnum; ++j, ++phdr) {
            if (PT_LOAD64==get_te32(&phdr->p_type) && 0!=n_ptload++) {
                load_off = get_te64(&phdr->p_offset);
                fi->seek(old_data_off, SEEK_SET);
                fi->readx(ibuf, old_data_len);
                total_in  += old_data_len;
                total_out += old_data_len;
                if (fo) {
                    fo->seek(get_te64(&phdr->p_offset), SEEK_SET);
                    fo->rewrite(ibuf, old_data_len);
                }
            }
        }
        // Restore DT_INIT.d_val
        phdr = (Elf64_Phdr *) (u.buf + sizeof(*ehdr));
        for (unsigned j= 0; j < u_phnum; ++j, ++phdr) {
            if (phdr->PT_DYNAMIC==get_te32(&phdr->p_type)) {
                unsigned const dyn_off = get_te64(&phdr->p_offset);
                unsigned const dyn_len = get_te64(&phdr->p_filesz);
                Elf64_Dyn *dyn = (Elf64_Dyn *)((unsigned char *)ibuf +
                    (dyn_off - load_off));
                for (unsigned j2= 0; j2 < dyn_len; ++dyn, j2 += sizeof(*dyn)) {
                    if (dyn->DT_INIT==get_te32(&dyn->d_tag)) {
                        if (fo) {
                            fo->seek(sizeof(upx_uint64_t) + j2 + dyn_off, SEEK_SET);
                            fo->rewrite(&old_dtinit, sizeof(old_dtinit));
                            fo->seek(0, SEEK_END);
                        }
                        break;
                    }
                }
            }
        }
    }

    // update header with totals
    ph.c_len = total_in;
    ph.u_len = total_out;

    // all bytes must be written
    if (total_out != orig_file_size)
        throwEOFException();

    // finally test the checksums
    if (ph.c_adler != c_adler || ph.u_adler != u_adler)
        throwChecksumError();
#undef MAX_ELF_HDR
}