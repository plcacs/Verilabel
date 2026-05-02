static pj_status_t parse_query(pj_dns_parsed_query *q, pj_pool_t *pool,
			       const pj_uint8_t *pkt, const pj_uint8_t *start,
			       const pj_uint8_t *max, int *parsed_len)
{
    const pj_uint8_t *p = start;
    int name_len, name_part_len;
    pj_status_t status;

    /* Get the length of the name */
    status = get_name_len(0, pkt, start, max, &name_part_len, &name_len);
    if (status != PJ_SUCCESS)
	return status;

    /* Allocate memory for the name */
    q->name.ptr = (char*) pj_pool_alloc(pool, name_len+4);
    q->name.slen = 0;

    /* Get the name */
    status = get_name(0, pkt, start, max, &q->name);
    if (status != PJ_SUCCESS)
	return status;

    p = (start + name_part_len);

    /* Get the type */
    pj_memcpy(&q->type, p, 2);
    q->type = pj_ntohs(q->type);
    p += 2;

    /* Get the class */
    pj_memcpy(&q->dnsclass, p, 2);
    q->dnsclass = pj_ntohs(q->dnsclass);
    p += 2;

    *parsed_len = (int)(p - start);

    return PJ_SUCCESS;
}