ins_comp_get_next_word_or_line(
	buf_T	*ins_buf,		// buffer being scanned
	pos_T	*cur_match_pos,		// current match position
	int	*match_len,
	int	*cont_s_ipos)		// next ^X<> will set initial_pos
{
    char_u	*ptr;
    int		len;

    *match_len = 0;
    ptr = ml_get_buf(ins_buf, cur_match_pos->lnum, FALSE) +
	cur_match_pos->col;
    if (ctrl_x_mode_line_or_eval())
    {
	if (compl_status_adding())
	{
	    if (cur_match_pos->lnum >= ins_buf->b_ml.ml_line_count)
		return NULL;
	    ptr = ml_get_buf(ins_buf, cur_match_pos->lnum + 1, FALSE);
	    if (!p_paste)
		ptr = skipwhite(ptr);
	}
	len = (int)STRLEN(ptr);
    }
    else
    {
	char_u	*tmp_ptr = ptr;

	if (compl_status_adding())
	{
	    tmp_ptr += compl_length;
	    // Skip if already inside a word.
	    if (vim_iswordp(tmp_ptr))
		return NULL;
	    // Find start of next word.
	    tmp_ptr = find_word_start(tmp_ptr);
	}
	// Find end of this word.
	tmp_ptr = find_word_end(tmp_ptr);
	len = (int)(tmp_ptr - ptr);

	if (compl_status_adding() && len == compl_length)
	{
	    if (cur_match_pos->lnum < ins_buf->b_ml.ml_line_count)
	    {
		// Try next line, if any. the new word will be
		// "join" as if the normal command "J" was used.
		// IOSIZE is always greater than
		// compl_length, so the next STRNCPY always
		// works -- Acevedo
		STRNCPY(IObuff, ptr, len);
		ptr = ml_get_buf(ins_buf, cur_match_pos->lnum + 1, FALSE);
		tmp_ptr = ptr = skipwhite(ptr);
		// Find start of next word.
		tmp_ptr = find_word_start(tmp_ptr);
		// Find end of next word.
		tmp_ptr = find_word_end(tmp_ptr);
		if (tmp_ptr > ptr)
		{
		    if (*ptr != ')' && IObuff[len - 1] != TAB)
		    {
			if (IObuff[len - 1] != ' ')
			    IObuff[len++] = ' ';
			// IObuf =~ "\k.* ", thus len >= 2
			if (p_js
				&& (IObuff[len - 2] == '.'
				    || (vim_strchr(p_cpo, CPO_JOINSP)
					== NULL
					&& (IObuff[len - 2] == '?'
					    || IObuff[len - 2] == '!'))))
			    IObuff[len++] = ' ';
		    }
		    // copy as much as possible of the new word
		    if (tmp_ptr - ptr >= IOSIZE - len)
			tmp_ptr = ptr + IOSIZE - len - 1;
		    STRNCPY(IObuff + len, ptr, tmp_ptr - ptr);
		    len += (int)(tmp_ptr - ptr);
		    *cont_s_ipos = TRUE;
		}
		IObuff[len] = NUL;
		ptr = IObuff;
	    }
	    if (len == compl_length)
		return NULL;
	}
    }

    *match_len = len;
    return ptr;
}