sdhci_write(void *opaque, hwaddr offset, uint64_t val, unsigned size)
{
    SDHCIState *s = (SDHCIState *)opaque;
    unsigned shift =  8 * (offset & 0x3);
    uint32_t mask = ~(((1ULL << (size * 8)) - 1) << shift);
    uint32_t value = val;
    value <<= shift;

    switch (offset & ~0x3) {
    case SDHC_SYSAD:
        s->sdmasysad = (s->sdmasysad & mask) | value;
        MASKED_WRITE(s->sdmasysad, mask, value);
        /* Writing to last byte of sdmasysad might trigger transfer */
        if (!(mask & 0xFF000000) && TRANSFERRING_DATA(s->prnsts) && s->blkcnt &&
                s->blksize && SDHC_DMA_TYPE(s->hostctl1) == SDHC_CTRL_SDMA) {
            if (s->trnmod & SDHC_TRNS_MULTI) {
                sdhci_sdma_transfer_multi_blocks(s);
            } else {
                sdhci_sdma_transfer_single_block(s);
            }
        }
        break;
    case SDHC_BLKSIZE:
        if (!TRANSFERRING_DATA(s->prnsts)) {
            MASKED_WRITE(s->blksize, mask, value);
            MASKED_WRITE(s->blkcnt, mask >> 16, value >> 16);
        }

        /* Limit block size to the maximum buffer size */
        if (extract32(s->blksize, 0, 12) > s->buf_maxsz) {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Size 0x%x is larger than "
                          "the maximum buffer 0x%x\n", __func__, s->blksize,
                          s->buf_maxsz);

            s->blksize = deposit32(s->blksize, 0, 12, s->buf_maxsz);
        }

        break;
    case SDHC_ARGUMENT:
        MASKED_WRITE(s->argument, mask, value);
        break;
    case SDHC_TRNMOD:
        /* DMA can be enabled only if it is supported as indicated by
         * capabilities register */
        if (!(s->capareg & R_SDHC_CAPAB_SDMA_MASK)) {
            value &= ~SDHC_TRNS_DMA;
        }
        MASKED_WRITE(s->trnmod, mask, value & SDHC_TRNMOD_MASK);
        MASKED_WRITE(s->cmdreg, mask >> 16, value >> 16);

        /* Writing to the upper byte of CMDREG triggers SD command generation */
        if ((mask & 0xFF000000) || !sdhci_can_issue_command(s)) {
            break;
        }

        sdhci_send_command(s);
        break;
    case  SDHC_BDATA:
        if (sdhci_buff_access_is_sequential(s, offset - SDHC_BDATA)) {
            sdhci_write_dataport(s, value >> shift, size);
        }
        break;
    case SDHC_HOSTCTL:
        if (!(mask & 0xFF0000)) {
            sdhci_blkgap_write(s, value >> 16);
        }
        MASKED_WRITE(s->hostctl1, mask, value);
        MASKED_WRITE(s->pwrcon, mask >> 8, value >> 8);
        MASKED_WRITE(s->wakcon, mask >> 24, value >> 24);
        if (!(s->prnsts & SDHC_CARD_PRESENT) || ((s->pwrcon >> 1) & 0x7) < 5 ||
                !(s->capareg & (1 << (31 - ((s->pwrcon >> 1) & 0x7))))) {
            s->pwrcon &= ~SDHC_POWER_ON;
        }
        break;
    case SDHC_CLKCON:
        if (!(mask & 0xFF000000)) {
            sdhci_reset_write(s, value >> 24);
        }
        MASKED_WRITE(s->clkcon, mask, value);
        MASKED_WRITE(s->timeoutcon, mask >> 16, value >> 16);
        if (s->clkcon & SDHC_CLOCK_INT_EN) {
            s->clkcon |= SDHC_CLOCK_INT_STABLE;
        } else {
            s->clkcon &= ~SDHC_CLOCK_INT_STABLE;
        }
        break;
    case SDHC_NORINTSTS:
        if (s->norintstsen & SDHC_NISEN_CARDINT) {
            value &= ~SDHC_NIS_CARDINT;
        }
        s->norintsts &= mask | ~value;
        s->errintsts &= (mask >> 16) | ~(value >> 16);
        if (s->errintsts) {
            s->norintsts |= SDHC_NIS_ERR;
        } else {
            s->norintsts &= ~SDHC_NIS_ERR;
        }
        sdhci_update_irq(s);
        break;
    case SDHC_NORINTSTSEN:
        MASKED_WRITE(s->norintstsen, mask, value);
        MASKED_WRITE(s->errintstsen, mask >> 16, value >> 16);
        s->norintsts &= s->norintstsen;
        s->errintsts &= s->errintstsen;
        if (s->errintsts) {
            s->norintsts |= SDHC_NIS_ERR;
        } else {
            s->norintsts &= ~SDHC_NIS_ERR;
        }
        /* Quirk for Raspberry Pi: pending card insert interrupt
         * appears when first enabled after power on */
        if ((s->norintstsen & SDHC_NISEN_INSERT) && s->pending_insert_state) {
            assert(s->pending_insert_quirk);
            s->norintsts |= SDHC_NIS_INSERT;
            s->pending_insert_state = false;
        }
        sdhci_update_irq(s);
        break;
    case SDHC_NORINTSIGEN:
        MASKED_WRITE(s->norintsigen, mask, value);
        MASKED_WRITE(s->errintsigen, mask >> 16, value >> 16);
        sdhci_update_irq(s);
        break;
    case SDHC_ADMAERR:
        MASKED_WRITE(s->admaerr, mask, value);
        break;
    case SDHC_ADMASYSADDR:
        s->admasysaddr = (s->admasysaddr & (0xFFFFFFFF00000000ULL |
                (uint64_t)mask)) | (uint64_t)value;
        break;
    case SDHC_ADMASYSADDR + 4:
        s->admasysaddr = (s->admasysaddr & (0x00000000FFFFFFFFULL |
                ((uint64_t)mask << 32))) | ((uint64_t)value << 32);
        break;
    case SDHC_FEAER:
        s->acmd12errsts |= value;
        s->errintsts |= (value >> 16) & s->errintstsen;
        if (s->acmd12errsts) {
            s->errintsts |= SDHC_EIS_CMD12ERR;
        }
        if (s->errintsts) {
            s->norintsts |= SDHC_NIS_ERR;
        }
        sdhci_update_irq(s);
        break;
    case SDHC_ACMD12ERRSTS:
        MASKED_WRITE(s->acmd12errsts, mask, value & UINT16_MAX);
        if (s->uhs_mode >= UHS_I) {
            MASKED_WRITE(s->hostctl2, mask >> 16, value >> 16);

            if (FIELD_EX32(s->hostctl2, SDHC_HOSTCTL2, V18_ENA)) {
                sdbus_set_voltage(&s->sdbus, SD_VOLTAGE_1_8V);
            } else {
                sdbus_set_voltage(&s->sdbus, SD_VOLTAGE_3_3V);
            }
        }
        break;

    case SDHC_CAPAB:
    case SDHC_CAPAB + 4:
    case SDHC_MAXCURR:
    case SDHC_MAXCURR + 4:
        qemu_log_mask(LOG_GUEST_ERROR, "SDHC wr_%ub @0x%02" HWADDR_PRIx
                      " <- 0x%08x read-only\n", size, offset, value >> shift);
        break;

    default:
        qemu_log_mask(LOG_UNIMP, "SDHC wr_%ub @0x%02" HWADDR_PRIx " <- 0x%08x "
                      "not implemented\n", size, offset, value >> shift);
        break;
    }
    trace_sdhci_access("wr", size << 3, offset, "<-",
                       value >> shift, value >> shift);
}