static int mode_sense_page(SCSIDiskState *s, int page, uint8_t **p_outbuf,
                           int page_control)
{
    static const int mode_sense_valid[0x3f] = {
        [MODE_PAGE_HD_GEOMETRY]            = (1 << TYPE_DISK),
        [MODE_PAGE_FLEXIBLE_DISK_GEOMETRY] = (1 << TYPE_DISK),
        [MODE_PAGE_CACHING]                = (1 << TYPE_DISK) | (1 << TYPE_ROM),
        [MODE_PAGE_R_W_ERROR]              = (1 << TYPE_DISK) | (1 << TYPE_ROM),
        [MODE_PAGE_AUDIO_CTL]              = (1 << TYPE_ROM),
        [MODE_PAGE_CAPABILITIES]           = (1 << TYPE_ROM),
    };

    uint8_t *p = *p_outbuf + 2;
    int length;

    if ((mode_sense_valid[page] & (1 << s->qdev.type)) == 0) {
        return -1;
    }

    /*
     * If Changeable Values are requested, a mask denoting those mode parameters
     * that are changeable shall be returned. As we currently don't support
     * parameter changes via MODE_SELECT all bits are returned set to zero.
     * The buffer was already menset to zero by the caller of this function.
     *
     * The offsets here are off by two compared to the descriptions in the
     * SCSI specs, because those include a 2-byte header.  This is unfortunate,
     * but it is done so that offsets are consistent within our implementation
     * of MODE SENSE and MODE SELECT.  MODE SELECT has to deal with both
     * 2-byte and 4-byte headers.
     */
    switch (page) {
    case MODE_PAGE_HD_GEOMETRY:
        length = 0x16;
        if (page_control == 1) { /* Changeable Values */
            break;
        }
        /* if a geometry hint is available, use it */
        p[0] = (s->qdev.conf.cyls >> 16) & 0xff;
        p[1] = (s->qdev.conf.cyls >> 8) & 0xff;
        p[2] = s->qdev.conf.cyls & 0xff;
        p[3] = s->qdev.conf.heads & 0xff;
        /* Write precomp start cylinder, disabled */
        p[4] = (s->qdev.conf.cyls >> 16) & 0xff;
        p[5] = (s->qdev.conf.cyls >> 8) & 0xff;
        p[6] = s->qdev.conf.cyls & 0xff;
        /* Reduced current start cylinder, disabled */
        p[7] = (s->qdev.conf.cyls >> 16) & 0xff;
        p[8] = (s->qdev.conf.cyls >> 8) & 0xff;
        p[9] = s->qdev.conf.cyls & 0xff;
        /* Device step rate [ns], 200ns */
        p[10] = 0;
        p[11] = 200;
        /* Landing zone cylinder */
        p[12] = 0xff;
        p[13] =  0xff;
        p[14] = 0xff;
        /* Medium rotation rate [rpm], 5400 rpm */
        p[18] = (5400 >> 8) & 0xff;
        p[19] = 5400 & 0xff;
        break;

    case MODE_PAGE_FLEXIBLE_DISK_GEOMETRY:
        length = 0x1e;
        if (page_control == 1) { /* Changeable Values */
            break;
        }
        /* Transfer rate [kbit/s], 5Mbit/s */
        p[0] = 5000 >> 8;
        p[1] = 5000 & 0xff;
        /* if a geometry hint is available, use it */
        p[2] = s->qdev.conf.heads & 0xff;
        p[3] = s->qdev.conf.secs & 0xff;
        p[4] = s->qdev.blocksize >> 8;
        p[6] = (s->qdev.conf.cyls >> 8) & 0xff;
        p[7] = s->qdev.conf.cyls & 0xff;
        /* Write precomp start cylinder, disabled */
        p[8] = (s->qdev.conf.cyls >> 8) & 0xff;
        p[9] = s->qdev.conf.cyls & 0xff;
        /* Reduced current start cylinder, disabled */
        p[10] = (s->qdev.conf.cyls >> 8) & 0xff;
        p[11] = s->qdev.conf.cyls & 0xff;
        /* Device step rate [100us], 100us */
        p[12] = 0;
        p[13] = 1;
        /* Device step pulse width [us], 1us */
        p[14] = 1;
        /* Device head settle delay [100us], 100us */
        p[15] = 0;
        p[16] = 1;
        /* Motor on delay [0.1s], 0.1s */
        p[17] = 1;
        /* Motor off delay [0.1s], 0.1s */
        p[18] = 1;
        /* Medium rotation rate [rpm], 5400 rpm */
        p[26] = (5400 >> 8) & 0xff;
        p[27] = 5400 & 0xff;
        break;

    case MODE_PAGE_CACHING:
        length = 0x12;
        if (page_control == 1 || /* Changeable Values */
            blk_enable_write_cache(s->qdev.conf.blk)) {
            p[0] = 4; /* WCE */
        }
        break;

    case MODE_PAGE_R_W_ERROR:
        length = 10;
        if (page_control == 1) { /* Changeable Values */
            break;
        }
        p[0] = 0x80; /* Automatic Write Reallocation Enabled */
        if (s->qdev.type == TYPE_ROM) {
            p[1] = 0x20; /* Read Retry Count */
        }
        break;

    case MODE_PAGE_AUDIO_CTL:
        length = 14;
        break;

    case MODE_PAGE_CAPABILITIES:
        length = 0x14;
        if (page_control == 1) { /* Changeable Values */
            break;
        }

        p[0] = 0x3b; /* CD-R & CD-RW read */
        p[1] = 0; /* Writing not supported */
        p[2] = 0x7f; /* Audio, composite, digital out,
                        mode 2 form 1&2, multi session */
        p[3] = 0xff; /* CD DA, DA accurate, RW supported,
                        RW corrected, C2 errors, ISRC,
                        UPC, Bar code */
        p[4] = 0x2d | (s->tray_locked ? 2 : 0);
        /* Locking supported, jumper present, eject, tray */
        p[5] = 0; /* no volume & mute control, no
                     changer */
        p[6] = (50 * 176) >> 8; /* 50x read speed */
        p[7] = (50 * 176) & 0xff;
        p[8] = 2 >> 8; /* Two volume levels */
        p[9] = 2 & 0xff;
        p[10] = 2048 >> 8; /* 2M buffer */
        p[11] = 2048 & 0xff;
        p[12] = (16 * 176) >> 8; /* 16x read speed current */
        p[13] = (16 * 176) & 0xff;
        p[16] = (16 * 176) >> 8; /* 16x write speed */
        p[17] = (16 * 176) & 0xff;
        p[18] = (16 * 176) >> 8; /* 16x write speed current */
        p[19] = (16 * 176) & 0xff;
        break;

    default:
        return -1;
    }

    assert(length < 256);
    (*p_outbuf)[0] = page;
    (*p_outbuf)[1] = length;
    *p_outbuf += length + 2;
    return length + 2;
}