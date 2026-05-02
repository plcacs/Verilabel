static void virtio_blk_handle_read(VirtIOBlockReq *req)
{
    BlockDriverAIOCB *acb;
    uint64_t sector;

    sector = ldq_p(&req->out->sector);

    if (sector & req->dev->sector_mask) {
        virtio_blk_rw_complete(req, -EIO);
        return;
    }

    acb = bdrv_aio_readv(req->dev->bs, sector, &req->qiov,
                         req->qiov.size / BDRV_SECTOR_SIZE,
                         virtio_blk_rw_complete, req);
    if (!acb) {
        virtio_blk_rw_complete(req, -EIO);
    }
}