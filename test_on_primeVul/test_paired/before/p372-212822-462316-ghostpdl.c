pcl_status_read(byte * data, uint max_data, pcl_state_t * pcs)
{
    uint count = min(max_data,
                     pcs->status.write_pos - pcs->status.read_pos);

    if (count)
        memcpy(data, pcs->status.buffer + pcs->status.read_pos, count);
    pcs->status.read_pos += count;
    if (pcs->status.read_pos == pcs->status.write_pos) {
        gs_free_object(pcs->memory, pcs->status.buffer, "status buffer");
        pcs->status.write_pos = pcs->status.read_pos = 0;
    }
    return count;
}