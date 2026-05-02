static void __init mp_override_legacy_irq(u8 bus_irq, u8 polarity, u8 trigger,
					  u32 gsi)
{
	int ioapic;
	int pin;
	struct mpc_intsrc mp_irq;

	/*
	 * Convert 'gsi' to 'ioapic.pin'.
	 */
	ioapic = mp_find_ioapic(gsi);
	if (ioapic < 0)
		return;
	pin = mp_find_ioapic_pin(ioapic, gsi);

	/*
	 * TBD: This check is for faulty timer entries, where the override
	 *      erroneously sets the trigger to level, resulting in a HUGE
	 *      increase of timer interrupts!
	 */
	if ((bus_irq == 0) && (trigger == 3))
		trigger = 1;

	mp_irq.type = MP_INTSRC;
	mp_irq.irqtype = mp_INT;
	mp_irq.irqflag = (trigger << 2) | polarity;
	mp_irq.srcbus = MP_ISA_BUS;
	mp_irq.srcbusirq = bus_irq;	/* IRQ */
	mp_irq.dstapic = mpc_ioapic_id(ioapic); /* APIC ID */
	mp_irq.dstirq = pin;	/* INTIN# */

	mp_save_irq(&mp_irq);

	/*
	 * Reset default identity mapping if gsi is also an legacy IRQ,
	 * otherwise there will be more than one entry with the same GSI
	 * and acpi_isa_irq_to_gsi() may give wrong result.
	 */
	if (gsi < nr_legacy_irqs() && isa_irq_to_gsi[gsi] == gsi)
		isa_irq_to_gsi[gsi] = ACPI_INVALID_GSI;
	isa_irq_to_gsi[bus_irq] = gsi;
}