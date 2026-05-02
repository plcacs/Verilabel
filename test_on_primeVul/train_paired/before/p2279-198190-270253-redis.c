void streamGetEdgeID(stream *s, int first, int skip_tombstones, streamID *edge_id)
{
    streamIterator si;
    int64_t numfields;
    streamIteratorStart(&si,s,NULL,NULL,!first);
    si.skip_tombstones = skip_tombstones;
    int found = streamIteratorGetID(&si,edge_id,&numfields);
    if (!found) {
        streamID min_id = {0, 0}, max_id = {UINT64_MAX, UINT64_MAX};
        *edge_id = first ? max_id : min_id;
    }

}