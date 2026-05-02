delete_buff_tail(buffheader_T *buf, int slen)
{
    int len = (int)STRLEN(buf->bh_curr->b_str);

    if (len >= slen)
    {
	buf->bh_curr->b_str[len - slen] = NUL;
	buf->bh_space += slen;
    }
}