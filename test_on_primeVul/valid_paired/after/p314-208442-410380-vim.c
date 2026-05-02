delete_buff_tail(buffheader_T *buf, int slen)
{
    int len;

    if (buf->bh_curr == NULL || buf->bh_curr->b_str == NULL)
	return;  // nothing to delete
    len = (int)STRLEN(buf->bh_curr->b_str);
    if (len >= slen)
    {
	buf->bh_curr->b_str[len - slen] = NUL;
	buf->bh_space += slen;
    }
}