yank_copy_line(struct block_def *bd, long y_idx, int exclude_trailing_space)
{
    char_u	*pnew;

    if (exclude_trailing_space)
	bd->endspaces = 0;
    if ((pnew = alloc(bd->startspaces + bd->endspaces + bd->textlen + 1))
								      == NULL)
	return FAIL;
    y_current->y_array[y_idx] = pnew;
    vim_memset(pnew, ' ', (size_t)bd->startspaces);
    pnew += bd->startspaces;
    mch_memmove(pnew, bd->textstart, (size_t)bd->textlen);
    pnew += bd->textlen;
    vim_memset(pnew, ' ', (size_t)bd->endspaces);
    pnew += bd->endspaces;
    if (exclude_trailing_space)
    {
	int s = bd->textlen + bd->endspaces;

	while (VIM_ISWHITE(*(bd->textstart + s - 1)) && s > 0)
	{
	    s = s - (*mb_head_off)(bd->textstart, bd->textstart + s - 1) - 1;
	    pnew--;
	}
    }
    *pnew = NUL;
    return OK;
}