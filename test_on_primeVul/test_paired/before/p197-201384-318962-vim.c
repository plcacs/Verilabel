ga_concat_shorten_esc(garray_T *gap, char_u *str)
{
    char_u  *p;
    char_u  *s;
    int	    c;
    int	    clen;
    char_u  buf[NUMBUFLEN];
    int	    same_len;

    if (str == NULL)
    {
	ga_concat(gap, (char_u *)"NULL");
	return;
    }

    for (p = str; *p != NUL; ++p)
    {
	same_len = 1;
	s = p;
	c = mb_ptr2char_adv(&s);
	clen = s - p;
	while (*s != NUL && c == mb_ptr2char(s))
	{
	    ++same_len;
	    s += clen;
	}
	if (same_len > 20)
	{
	    ga_concat(gap, (char_u *)"\\[");
	    ga_concat_esc(gap, p, clen);
	    ga_concat(gap, (char_u *)" occurs ");
	    vim_snprintf((char *)buf, NUMBUFLEN, "%d", same_len);
	    ga_concat(gap, buf);
	    ga_concat(gap, (char_u *)" times]");
	    p = s - 1;
	}
	else
	    ga_concat_esc(gap, p, clen);
    }
}