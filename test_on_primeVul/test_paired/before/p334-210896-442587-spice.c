void *memslot_get_virt(RedMemSlotInfo *info, QXLPHYSICAL addr, uint32_t add_size,
                       int group_id)
{
    int slot_id;
    int generation;
    unsigned long h_virt;

    MemSlot *slot;

    if (group_id > info->num_memslots_groups) {
        spice_critical("group_id too big");
        return NULL;
    }

    slot_id = memslot_get_id(info, addr);
    if (slot_id > info->num_memslots) {
        print_memslots(info);
        spice_critical("slot_id %d too big, addr=%" PRIx64, slot_id, addr);
        return NULL;
    }

    slot = &info->mem_slots[group_id][slot_id];

    generation = memslot_get_generation(info, addr);
    if (generation != slot->generation) {
        print_memslots(info);
        spice_critical("address generation is not valid, group_id %d, slot_id %d, "
                       "gen %d, slot_gen %d",
                       group_id, slot_id,
                       generation, slot->generation);
        return NULL;
    }

    h_virt = __get_clean_virt(info, addr);
    h_virt += slot->address_delta;

    if (!memslot_validate_virt(info, h_virt, slot_id, add_size, group_id)) {
        return NULL;
    }

    return (void*)(uintptr_t)h_virt;
}