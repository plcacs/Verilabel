int esp_output_head(struct xfrm_state *x, struct sk_buff *skb, struct esp_info *esp)
{
	u8 *tail;
	int nfrags;
	int esph_offset;
	struct page *page;
	struct sk_buff *trailer;
	int tailen = esp->tailen;

	/* this is non-NULL only with TCP/UDP Encapsulation */
	if (x->encap) {
		int err = esp_output_encap(x, skb, esp);

		if (err < 0)
			return err;
	}

	if (!skb_cloned(skb)) {
		if (tailen <= skb_tailroom(skb)) {
			nfrags = 1;
			trailer = skb;
			tail = skb_tail_pointer(trailer);

			goto skip_cow;
		} else if ((skb_shinfo(skb)->nr_frags < MAX_SKB_FRAGS)
			   && !skb_has_frag_list(skb)) {
			int allocsize;
			struct sock *sk = skb->sk;
			struct page_frag *pfrag = &x->xfrag;

			esp->inplace = false;

			allocsize = ALIGN(tailen, L1_CACHE_BYTES);

			spin_lock_bh(&x->lock);

			if (unlikely(!skb_page_frag_refill(allocsize, pfrag, GFP_ATOMIC))) {
				spin_unlock_bh(&x->lock);
				goto cow;
			}

			page = pfrag->page;
			get_page(page);

			tail = page_address(page) + pfrag->offset;

			esp_output_fill_trailer(tail, esp->tfclen, esp->plen, esp->proto);

			nfrags = skb_shinfo(skb)->nr_frags;

			__skb_fill_page_desc(skb, nfrags, page, pfrag->offset,
					     tailen);
			skb_shinfo(skb)->nr_frags = ++nfrags;

			pfrag->offset = pfrag->offset + allocsize;

			spin_unlock_bh(&x->lock);

			nfrags++;

			skb->len += tailen;
			skb->data_len += tailen;
			skb->truesize += tailen;
			if (sk && sk_fullsock(sk))
				refcount_add(tailen, &sk->sk_wmem_alloc);

			goto out;
		}
	}

cow:
	esph_offset = (unsigned char *)esp->esph - skb_transport_header(skb);

	nfrags = skb_cow_data(skb, tailen, &trailer);
	if (nfrags < 0)
		goto out;
	tail = skb_tail_pointer(trailer);
	esp->esph = (struct ip_esp_hdr *)(skb_transport_header(skb) + esph_offset);

skip_cow:
	esp_output_fill_trailer(tail, esp->tfclen, esp->plen, esp->proto);
	pskb_put(skb, trailer, tailen);

out:
	return nfrags;
}