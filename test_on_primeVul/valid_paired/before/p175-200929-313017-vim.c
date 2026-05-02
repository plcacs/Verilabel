unserialize_uep(bufinfo_T *bi, int *error, char_u *file_name)
{
    int		i;
    u_entry_T	*uep;
    char_u	**array;
    char_u	*line;
    int		line_len;

    uep = (u_entry_T *)U_ALLOC_LINE(sizeof(u_entry_T));
    if (uep == NULL)
	return NULL;
    vim_memset(uep, 0, sizeof(u_entry_T));
#ifdef U_DEBUG
    uep->ue_magic = UE_MAGIC;
#endif
    uep->ue_top = undo_read_4c(bi);
    uep->ue_bot = undo_read_4c(bi);
    uep->ue_lcount = undo_read_4c(bi);
    uep->ue_size = undo_read_4c(bi);
    if (uep->ue_size > 0)
    {
	array = (char_u **)U_ALLOC_LINE(sizeof(char_u *) * uep->ue_size);
	if (array == NULL)
	{
	    *error = TRUE;
	    return uep;
	}
	vim_memset(array, 0, sizeof(char_u *) * uep->ue_size);
    }
    else
	array = NULL;
    uep->ue_array = array;

    for (i = 0; i < uep->ue_size; ++i)
    {
	line_len = undo_read_4c(bi);
	if (line_len >= 0)
	    line = read_string_decrypt(bi, line_len);
	else
	{
	    line = NULL;
	    corruption_error("line length", file_name);
	}
	if (line == NULL)
	{
	    *error = TRUE;
	    return uep;
	}
	array[i] = line;
    }
    return uep;
}