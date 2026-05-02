static void coroutine_fn mirror_wait_on_conflicts(MirrorOp *self,
                                                  MirrorBlockJob *s,
                                                  uint64_t offset,
                                                  uint64_t bytes)
{
    uint64_t self_start_chunk = offset / s->granularity;
    uint64_t self_end_chunk = DIV_ROUND_UP(offset + bytes, s->granularity);
    uint64_t self_nb_chunks = self_end_chunk - self_start_chunk;

    while (find_next_bit(s->in_flight_bitmap, self_end_chunk,
                         self_start_chunk) < self_end_chunk &&
           s->ret >= 0)
    {
        MirrorOp *op;

        QTAILQ_FOREACH(op, &s->ops_in_flight, next) {
            uint64_t op_start_chunk = op->offset / s->granularity;
            uint64_t op_nb_chunks = DIV_ROUND_UP(op->offset + op->bytes,
                                                 s->granularity) -
                                    op_start_chunk;

            if (op == self) {
                continue;
            }

            if (ranges_overlap(self_start_chunk, self_nb_chunks,
                               op_start_chunk, op_nb_chunks))
            {
                /*
                 * If the operation is already (indirectly) waiting for us, or
                 * will wait for us as soon as it wakes up, then just go on
                 * (instead of producing a deadlock in the former case).
                 */
                if (op->waiting_for_op) {
                    continue;
                }

                self->waiting_for_op = op;
                qemu_co_queue_wait(&op->waiting_requests, NULL);
                self->waiting_for_op = NULL;
                break;
            }
        }
    }
}