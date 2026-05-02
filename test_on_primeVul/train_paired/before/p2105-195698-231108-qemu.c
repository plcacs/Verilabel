process_tx_desc(E1000State *s, struct e1000_tx_desc *dp)
{
    PCIDevice *d = PCI_DEVICE(s);
    uint32_t txd_lower = le32_to_cpu(dp->lower.data);
    uint32_t dtype = txd_lower & (E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D);
    unsigned int split_size = txd_lower & 0xffff, bytes, sz;
    unsigned int msh = 0xfffff;
    uint64_t addr;
    struct e1000_context_desc *xp = (struct e1000_context_desc *)dp;
    struct e1000_tx *tp = &s->tx;

    s->mit_ide |= (txd_lower & E1000_TXD_CMD_IDE);
    if (dtype == E1000_TXD_CMD_DEXT) {    /* context descriptor */
        if (le32_to_cpu(xp->cmd_and_length) & E1000_TXD_CMD_TSE) {
            e1000x_read_tx_ctx_descr(xp, &tp->tso_props);
            s->use_tso_for_migration = 1;
            tp->tso_frames = 0;
        } else {
            e1000x_read_tx_ctx_descr(xp, &tp->props);
            s->use_tso_for_migration = 0;
        }
        return;
    } else if (dtype == (E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D)) {
        // data descriptor
        if (tp->size == 0) {
            tp->sum_needed = le32_to_cpu(dp->upper.data) >> 8;
        }
        tp->cptse = (txd_lower & E1000_TXD_CMD_TSE) ? 1 : 0;
    } else {
        // legacy descriptor
        tp->cptse = 0;
    }

    if (e1000x_vlan_enabled(s->mac_reg) &&
        e1000x_is_vlan_txd(txd_lower) &&
        (tp->cptse || txd_lower & E1000_TXD_CMD_EOP)) {
        tp->vlan_needed = 1;
        stw_be_p(tp->vlan_header,
                      le16_to_cpu(s->mac_reg[VET]));
        stw_be_p(tp->vlan_header + 2,
                      le16_to_cpu(dp->upper.fields.special));
    }

    addr = le64_to_cpu(dp->buffer_addr);
    if (tp->cptse) {
        msh = tp->tso_props.hdr_len + tp->tso_props.mss;
        do {
            bytes = split_size;
            if (tp->size + bytes > msh)
                bytes = msh - tp->size;

            bytes = MIN(sizeof(tp->data) - tp->size, bytes);
            pci_dma_read(d, addr, tp->data + tp->size, bytes);
            sz = tp->size + bytes;
            if (sz >= tp->tso_props.hdr_len
                && tp->size < tp->tso_props.hdr_len) {
                memmove(tp->header, tp->data, tp->tso_props.hdr_len);
            }
            tp->size = sz;
            addr += bytes;
            if (sz == msh) {
                xmit_seg(s);
                memmove(tp->data, tp->header, tp->tso_props.hdr_len);
                tp->size = tp->tso_props.hdr_len;
            }
            split_size -= bytes;
        } while (bytes && split_size);
    } else {
        split_size = MIN(sizeof(tp->data) - tp->size, split_size);
        pci_dma_read(d, addr, tp->data + tp->size, split_size);
        tp->size += split_size;
    }

    if (!(txd_lower & E1000_TXD_CMD_EOP))
        return;
    if (!(tp->cptse && tp->size < tp->tso_props.hdr_len)) {
        xmit_seg(s);
    }
    tp->tso_frames = 0;
    tp->sum_needed = 0;
    tp->vlan_needed = 0;
    tp->size = 0;
    tp->cptse = 0;
}