drill_parse_T_code(gerb_file_t *fd, drill_state_t *state,
			gerbv_image_t *image, ssize_t file_line)
{
    int tool_num;
    gboolean done = FALSE;
    int temp;
    double size;
    gerbv_drill_stats_t *stats = image->drill_stats;
    gerbv_aperture_t *apert;
    gchar *tmps;
    gchar *string;

    dprintf("---> entering %s()...\n", __FUNCTION__);

    /* Sneak a peek at what's hiding after the 'T'. Ugly fix for
       broken headers from Orcad, which is crap */
    temp = gerb_fgetc(fd);
    dprintf("  Found a char '%s' (0x%02x) after the T\n",
	    gerbv_escape_char(temp), temp);
    
    /* might be a tool tool change stop switch on/off*/
    if((temp == 'C') && ((fd->ptr + 2) < fd->datalen)){
    	if(gerb_fgetc(fd) == 'S'){
    	    if (gerb_fgetc(fd) == 'T' ){
    	  	fd->ptr -= 4;
    	  	tmps = get_line(fd++);
    	  	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_NOTE, -1,
			_("Tool change stop switch found \"%s\" "
			    "at line %ld in file \"%s\""),
			tmps, file_line, fd->filename);
	  	g_free (tmps);

	  	return -1;
	    }
	    gerb_ungetc(fd);
	}
	gerb_ungetc(fd);
    }

    if( !(isdigit(temp) != 0 || temp == '+' || temp =='-') ) {
	if(temp != EOF) {
	    gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		   _("OrCAD bug: Junk text found in place of tool definition"));
	    tmps = get_line(fd);
	    gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_WARNING, -1,
		    _("Junk text \"%s\" "
			"at line %ld in file \"%s\""),
		    tmps, file_line, fd->filename);
	    g_free (tmps);
	    gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_WARNING, -1,
				  _("Ignoring junk text"));
	}
	return -1;
    }
    gerb_ungetc(fd);

    tool_num = (int) gerb_fgetint(fd, NULL);
    dprintf ("  Handling tool T%d at line %ld\n", tool_num, file_line);

    if (tool_num == 0) 
	return tool_num; /* T00 is a command to unload the drill */

    if (tool_num < TOOL_MIN || tool_num >= TOOL_MAX) {
	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		_("Out of bounds drill number %d "
		    "at line %ld in file \"%s\""),
		tool_num, file_line, fd->filename);
    }

    /* Set the current tool to the correct one */
    state->current_tool = tool_num;
    apert = image->aperture[tool_num];

    /* Check for a size definition */
    temp = gerb_fgetc(fd);

    /* This bit of code looks for a tool definition by scanning for strings
     * of form TxxC, TxxF, TxxS.  */
    while (!done) {
	switch((char)temp) {
	case 'C':
	    size = read_double(fd, state->header_number_format, GERBV_OMIT_ZEROS_TRAILING, state->decimals);
	    dprintf ("  Read a size of %g\n", size);

	    if (state->unit == GERBV_UNIT_MM) {
		size /= 25.4;
	    } else if(size >= 4.0) {
		/* If the drill size is >= 4 inches, assume that this
		   must be wrong and that the units are mils.
		   The limit being 4 inches is because the smallest drill
		   I've ever seen used is 0,3mm(about 12mil). Half of that
		   seemed a bit too small a margin, so a third it is */

		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
			_("Read a drill of diameter %g inches "
			    "at line %ld in file \"%s\""),
			    size, file_line, fd->filename);
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_WARNING, -1,
			_("Assuming units are mils"));
		size /= 1000.0;
	    }

	    if (size <= 0. || size >= 10000.) {
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
			_("Unreasonable drill size %g found for drill %d "
			    "at line %ld in file \"%s\""),
			    size, tool_num, file_line, fd->filename);
	    } else {
		if (apert != NULL) {
		    /* allow a redefine of a tool only if the new definition is exactly the same.
		     * This avoid lots of spurious complaints with the output of some cad
		     * tools while keeping complaints if there is a true problem
		     */
		    if (apert->parameter[0] != size
		    ||  apert->type != GERBV_APTYPE_CIRCLE
		    ||  apert->nuf_parameters != 1
		    ||  apert->unit != GERBV_UNIT_INCH) {

			gerbv_stats_printf(stats->error_list,
				GERBV_MESSAGE_ERROR, -1,
				_("Found redefinition of drill %d "
				"at line %ld in file \"%s\""),
				tool_num, file_line, fd->filename);
		    }
		} else {
		    apert = image->aperture[tool_num] =
						g_new0(gerbv_aperture_t, 1);
		    if (apert == NULL)
			GERB_FATAL_ERROR("malloc tool failed in %s()",
					__FUNCTION__);

		    /* There's really no way of knowing what unit the tools
		       are defined in without sneaking a peek in the rest of
		       the file first. That's done in drill_guess_format() */
		    apert->parameter[0] = size;
		    apert->type = GERBV_APTYPE_CIRCLE;
		    apert->nuf_parameters = 1;
		    apert->unit = GERBV_UNIT_INCH;
		}
	    }
	    
	    /* Add the tool whose definition we just found into the list
	     * of tools for this layer used to generate statistics. */
	    stats = image->drill_stats;
	    string = g_strdup_printf("%s", (state->unit == GERBV_UNIT_MM ? _("mm") : _("inch")));
	    drill_stats_add_to_drill_list(stats->drill_list, 
					  tool_num, 
					  state->unit == GERBV_UNIT_MM ? size*25.4 : size, 
					  string);
	    g_free(string);
	    break;

	case 'F':
	case 'S' :
	    /* Silently ignored. They're not important. */
	    gerb_fgetint(fd, NULL);
	    break;

	default:
	    /* Stop when finding anything but what's expected
	       (and put it back) */
	    gerb_ungetc(fd);
	    done = TRUE;
	    break;
	}  /* switch((char)temp) */

	temp = gerb_fgetc(fd);
	if (EOF == temp) {
	    gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Unexpected EOF encountered in header of "
			"drill file \"%s\""), fd->filename);

	/* Restore new line character for processing */
	if ('\n' == temp || '\r' == temp)
	    gerb_ungetc(fd);
	}
    }   /* while(!done) */  /* Done looking at tool definitions */

    /* Catch the tools that aren't defined.
       This isn't strictly a good thing, but at least something is shown */
    if (apert == NULL) {
        double dia;

	apert = image->aperture[tool_num] = g_new0(gerbv_aperture_t, 1);
	if (apert == NULL)
	    GERB_FATAL_ERROR("malloc tool failed in %s()", __FUNCTION__);

        /* See if we have the tool table */
        dia = gerbv_get_tool_diameter(tool_num);
        if (dia <= 0) {
            /*
             * There is no tool. So go out and make some.
             * This size calculation is, of course, totally bogus.
             */
            dia = (double)(16 + 8 * tool_num) / 1000;
            /*
             * Oooh, this is sooo ugly. But some CAD systems seem to always
             * use T00 at the end of the file while others that don't have
             * tool definitions inside the file never seem to use T00 at all.
             */
            if (tool_num != 0) {
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
			_("Tool %02d used without being defined "
			    "at line %ld in file \"%s\""),
			tool_num, file_line, fd->filename);
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_WARNING, -1,
			_("Setting a default size of %g\""), dia);
            }
	}

	apert->type = GERBV_APTYPE_CIRCLE;
	apert->nuf_parameters = 1;
	apert->parameter[0] = dia;

	/* Add the tool whose definition we just found into the list
	 * of tools for this layer used to generate statistics. */
	if (tool_num != 0) {  /* Only add non-zero tool nums.  
			       * Zero = unload command. */
	    stats = image->drill_stats;
	    string = g_strdup_printf("%s", 
				     (state->unit == GERBV_UNIT_MM ? _("mm") : _("inch")));
	    drill_stats_add_to_drill_list(stats->drill_list, 
					  tool_num, 
					  state->unit == GERBV_UNIT_MM ? dia*25.4 : dia,
					  string);
	    g_free(string);
	}
    } /* if(image->aperture[tool_num] == NULL) */	
    
    dprintf("<----  ...leaving %s()\n", __FUNCTION__);

    return tool_num;
} /* drill_parse_T_code() */