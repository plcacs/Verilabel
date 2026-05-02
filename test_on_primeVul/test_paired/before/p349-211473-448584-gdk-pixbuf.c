read_bitmap_file_data (FILE    *fstream,
		       guint   *width, 
		       guint   *height,
		       guchar **data,
		       int     *x_hot, 
		       int     *y_hot)
{
	guchar *bits = NULL;		/* working variable */
	char line[MAX_SIZE];		/* input line from file */
	int size;			/* number of bytes of data */
	char name_and_type[MAX_SIZE];	/* an input line */
	char *type;			/* for parsing */
	int value;			/* from an input line */
	int version10p;			/* boolean, old format */
	int padding;			/* to handle alignment */
	int bytes_per_line;		/* per scanline of data */
	guint ww = 0;			/* width */
	guint hh = 0;			/* height */
	int hx = -1;			/* x hotspot */
	int hy = -1;			/* y hotspot */

	/* first time initialization */
	if (!initialized) {
		init_hex_table ();
	}

	/* error cleanup and return macro */
#define	RETURN(code) { g_free (bits); return code; }

	while (fgets (line, MAX_SIZE, fstream)) {
		if (strlen (line) == MAX_SIZE-1)
			RETURN (FALSE);
		if (sscanf (line,"#define %s %d",name_and_type,&value) == 2) {
			if (!(type = strrchr (name_and_type, '_')))
				type = name_and_type;
			else {
				type++;
			}

			if (!strcmp ("width", type))
				ww = (unsigned int) value;
			if (!strcmp ("height", type))
				hh = (unsigned int) value;
			if (!strcmp ("hot", type)) {
				if (type-- == name_and_type
				    || type-- == name_and_type)
					continue;
				if (!strcmp ("x_hot", type))
					hx = value;
				if (!strcmp ("y_hot", type))
					hy = value;
			}
			continue;
		}
    
		if (sscanf (line, "static short %s = {", name_and_type) == 1)
			version10p = 1;
		else if (sscanf (line,"static const unsigned char %s = {",name_and_type) == 1)
			version10p = 0;
		else if (sscanf (line,"static unsigned char %s = {",name_and_type) == 1)
			version10p = 0;
		else if (sscanf (line, "static const char %s = {", name_and_type) == 1)
			version10p = 0;
		else if (sscanf (line, "static char %s = {", name_and_type) == 1)
			version10p = 0;
		else
			continue;

		if (!(type = strrchr (name_and_type, '_')))
			type = name_and_type;
		else
			type++;

		if (strcmp ("bits[]", type))
			continue;
    
		if (!ww || !hh)
			RETURN (FALSE);

		if ((ww % 16) && ((ww % 16) < 9) && version10p)
			padding = 1;
		else
			padding = 0;

		bytes_per_line = (ww+7)/8 + padding;

		size = bytes_per_line * hh;
		bits = g_malloc (size);

		if (version10p) {
			unsigned char *ptr;
			int bytes;

			for (bytes = 0, ptr = bits; bytes < size; (bytes += 2)) {
				if ((value = next_int (fstream)) < 0)
					RETURN (FALSE);
				*(ptr++) = value;
				if (!padding || ((bytes+2) % bytes_per_line))
					*(ptr++) = value >> 8;
			}
		} else {
			unsigned char *ptr;
			int bytes;

			for (bytes = 0, ptr = bits; bytes < size; bytes++, ptr++) {
				if ((value = next_int (fstream)) < 0) 
					RETURN (FALSE);
				*ptr=value;
			}
		}
		break;
	}

	if (!bits)
		RETURN (FALSE);

	*data = bits;
	*width = ww;
	*height = hh;
	if (x_hot)
		*x_hot = hx;
	if (y_hot)
		*y_hot = hy;

	return TRUE;
}