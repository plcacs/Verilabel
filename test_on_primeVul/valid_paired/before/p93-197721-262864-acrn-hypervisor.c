void dmar_free_irte(const struct intr_source *intr_src, uint16_t index)
{
	struct dmar_drhd_rt *dmar_unit;
	union dmar_ir_entry *ir_table, *ir_entry;
	union pci_bdf sid;

	if (intr_src->is_msi) {
		dmar_unit = device_to_dmaru((uint8_t)intr_src->src.msi.bits.b, intr_src->src.msi.fields.devfun);
	} else {
		dmar_unit = ioapic_to_dmaru(intr_src->src.ioapic_id, &sid);
	}

	if (is_dmar_unit_valid(dmar_unit, sid)) {
		ir_table = (union dmar_ir_entry *)hpa2hva(dmar_unit->ir_table_addr);
		ir_entry = ir_table + index;
		ir_entry->bits.remap.present = 0x0UL;

		iommu_flush_cache(ir_entry, sizeof(union dmar_ir_entry));
		dmar_invalid_iec(dmar_unit, index, 0U, false);

		if (!is_irte_reserved(dmar_unit, index)) {
			spinlock_obtain(&dmar_unit->lock);
			bitmap_clear_nolock(index & 0x3FU, &dmar_unit->irte_alloc_bitmap[index >> 6U]);
			spinlock_release(&dmar_unit->lock);
		}
	}

}