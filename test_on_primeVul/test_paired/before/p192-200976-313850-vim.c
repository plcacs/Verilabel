get_visual_text(
    cmdarg_T	*cap,
    char_u	**pp,	    // return: start of selected text
    int		*lenp)	    // return: length of selected text
{
    if (VIsual_mode != 'V')
	unadjust_for_sel();
    if (VIsual.lnum != curwin->w_cursor.lnum)
    {
	if (cap != NULL)
	    clearopbeep(cap->oap);
	return FAIL;
    }
    if (VIsual_mode == 'V')
    {
	*pp = ml_get_curline();
	*lenp = (int)STRLEN(*pp);
    }
    else
    {
	if (LT_POS(curwin->w_cursor, VIsual))
	{
	    *pp = ml_get_pos(&curwin->w_cursor);
	    *lenp = VIsual.col - curwin->w_cursor.col + 1;
	}
	else
	{
	    *pp = ml_get_pos(&VIsual);
	    *lenp = curwin->w_cursor.col - VIsual.col + 1;
	}
	if (**pp == NUL)
	    *lenp = 0;
	if (has_mbyte && *lenp > 0)
	    // Correct the length to include all bytes of the last character.
	    *lenp += (*mb_ptr2len)(*pp + (*lenp - 1)) - 1;
    }
    reset_VIsual_and_resel();
    return OK;
}