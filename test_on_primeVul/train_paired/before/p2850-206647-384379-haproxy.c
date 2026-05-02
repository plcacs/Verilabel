int hpack_dht_insert(struct hpack_dht *dht, struct ist name, struct ist value)
{
	unsigned int used;
	unsigned int head;
	unsigned int prev;
	unsigned int wrap;
	unsigned int tail;
	uint32_t headroom, tailroom;

	if (!hpack_dht_make_room(dht, name.len + value.len))
		return 0;

	/* Now there is enough room in the table, that's guaranteed by the
	 * protocol, but not necessarily where we need it.
	 */

	used = dht->used;
	if (!used) {
		/* easy, the table was empty */
		dht->front = dht->head = 0;
		dht->wrap  = dht->used = 1;
		dht->total = 0;
		head = 0;
		dht->dte[head].addr = dht->size - (name.len + value.len);
		goto copy;
	}

	/* compute the new head, used and wrap position */
	prev = head = dht->head;
	wrap = dht->wrap;
	tail = hpack_dht_get_tail(dht);

	used++;
	head++;

	if (head >= wrap) {
		/* head is leading the entries, we either need to push the
		 * table further or to loop back to released entries. We could
		 * force to loop back when at least half of the allocatable
		 * entries are free but in practice it never happens.
		 */
		if ((sizeof(*dht) + (wrap + 1) * sizeof(dht->dte[0]) <= dht->dte[dht->front].addr))
			wrap++;
		else if (head >= used) /* there's a hole at the beginning */
			head = 0;
		else {
			/* no more room, head hits tail and the index cannot be
			 * extended, we have to realign the whole table.
			 */
			if (!hpack_dht_defrag(dht))
				return -1;

			wrap = dht->wrap + 1;
			head = dht->head + 1;
			prev = head - 1;
			tail = 0;
		}
	}
	else if (used >= wrap) {
		/* we've hit the tail, we need to reorganize the index so that
		 * the head is at the end (but not necessarily move the data).
		 */
		if (!hpack_dht_defrag(dht))
			return -1;

		wrap = dht->wrap + 1;
		head = dht->head + 1;
		prev = head - 1;
		tail = 0;
	}

	/* Now we have updated head, used and wrap, we know that there is some
	 * available room at least from the protocol's perspective. This space
	 * is split in two areas :
	 *
	 *   1: if the previous head was the front cell, the space between the
	 *      end of the index table and the front cell's address.
	 *   2: if the previous head was the front cell, the space between the
	 *      end of the tail and the end of the table ; or if the previous
	 *      head was not the front cell, the space between the end of the
	 *      tail and the head's address.
	 */
	if (prev == dht->front) {
		/* the area was contiguous */
		headroom = dht->dte[dht->front].addr - (sizeof(*dht) + wrap * sizeof(dht->dte[0]));
		tailroom = dht->size - dht->dte[tail].addr - dht->dte[tail].nlen - dht->dte[tail].vlen;
	}
	else {
		/* it's already wrapped so we can't store anything in the headroom */
		headroom = 0;
		tailroom = dht->dte[prev].addr - dht->dte[tail].addr - dht->dte[tail].nlen - dht->dte[tail].vlen;
	}

	/* We can decide to stop filling the headroom as soon as there's enough
	 * room left in the tail to suit the protocol, but tests show that in
	 * practice it almost never happens in other situations so the extra
	 * test is useless and we simply fill the headroom as long as it's
	 * available.
	 */
	if (headroom >= name.len + value.len) {
		/* install upfront and update ->front */
		dht->dte[head].addr = dht->dte[dht->front].addr - (name.len + value.len);
		dht->front = head;
	}
	else if (tailroom >= name.len + value.len) {
		dht->dte[head].addr = dht->dte[tail].addr + dht->dte[tail].nlen + dht->dte[tail].vlen + tailroom - (name.len + value.len);
	}
	else {
		/* need to defragment the table before inserting upfront */
		dht = hpack_dht_defrag(dht);
		wrap = dht->wrap + 1;
		head = dht->head + 1;
		dht->dte[head].addr = dht->dte[dht->front].addr - (name.len + value.len);
		dht->front = head;
	}

	dht->wrap = wrap;
	dht->head = head;
	dht->used = used;

 copy:
	dht->total         += name.len + value.len;
	dht->dte[head].nlen = name.len;
	dht->dte[head].vlen = value.len;

	memcpy((void *)dht + dht->dte[head].addr, name.ptr, name.len);
	memcpy((void *)dht + dht->dte[head].addr + name.len, value.ptr, value.len);
	return 0;
}