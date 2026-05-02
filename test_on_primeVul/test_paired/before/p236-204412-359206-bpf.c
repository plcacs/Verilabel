static void *__bpf_ringbuf_reserve(struct bpf_ringbuf *rb, u64 size)
{
	unsigned long cons_pos, prod_pos, new_prod_pos, flags;
	u32 len, pg_off;
	struct bpf_ringbuf_hdr *hdr;

	if (unlikely(size > RINGBUF_MAX_RECORD_SZ))
		return NULL;

	len = round_up(size + BPF_RINGBUF_HDR_SZ, 8);
	cons_pos = smp_load_acquire(&rb->consumer_pos);

	if (in_nmi()) {
		if (!spin_trylock_irqsave(&rb->spinlock, flags))
			return NULL;
	} else {
		spin_lock_irqsave(&rb->spinlock, flags);
	}

	prod_pos = rb->producer_pos;
	new_prod_pos = prod_pos + len;

	/* check for out of ringbuf space by ensuring producer position
	 * doesn't advance more than (ringbuf_size - 1) ahead
	 */
	if (new_prod_pos - cons_pos > rb->mask) {
		spin_unlock_irqrestore(&rb->spinlock, flags);
		return NULL;
	}

	hdr = (void *)rb->data + (prod_pos & rb->mask);
	pg_off = bpf_ringbuf_rec_pg_off(rb, hdr);
	hdr->len = size | BPF_RINGBUF_BUSY_BIT;
	hdr->pg_off = pg_off;

	/* pairs with consumer's smp_load_acquire() */
	smp_store_release(&rb->producer_pos, new_prod_pos);

	spin_unlock_irqrestore(&rb->spinlock, flags);

	return (void *)hdr + BPF_RINGBUF_HDR_SZ;
}