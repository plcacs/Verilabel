im_vips2dz( IMAGE *in, const char *filename )
{
	char *p, *q;
	char name[FILENAME_MAX];
	char mode[FILENAME_MAX];
	char buf[FILENAME_MAX];

	int i;
	VipsForeignDzLayout layout = VIPS_FOREIGN_DZ_LAYOUT_DZ; 
	char *suffix = ".jpeg";
	int overlap = 0;
	int tile_size = 256;
	VipsForeignDzDepth depth = VIPS_FOREIGN_DZ_DEPTH_ONEPIXEL; 
	gboolean centre = FALSE;
	VipsAngle angle = VIPS_ANGLE_D0; 

	/* We can't use im_filename_split() --- it assumes that we have a
	 * filename with an extension before the ':', and filename here is
	 * actually a dirname.
	 *
	 * Just split on the first ':'.
	 */
	im_strncpy( name, filename, FILENAME_MAX ); 
	if( (p = strchr( name, ':' )) ) {
		*p = '\0';
		im_strncpy( mode, p + 1, FILENAME_MAX ); 
	}

	strcpy( buf, mode ); 
	p = &buf[0];

	if( (q = im_getnextoption( &p )) ) {
		if( (i = vips_enum_from_nick( "im_vips2dz", 
			VIPS_TYPE_FOREIGN_DZ_LAYOUT, q )) < 0 ) 
			return( -1 );
		layout = i;
	}

	if( (q = im_getnextoption( &p )) ) 
		suffix = g_strdup( q );
	if( (q = im_getnextoption( &p )) ) 
		overlap = atoi( q ); 
	if( (q = im_getnextoption( &p )) ) 
		tile_size = atoi( q ); 

	if( (q = im_getnextoption( &p )) ) {
		if( (i = vips_enum_from_nick( "im_vips2dz", 
			VIPS_TYPE_FOREIGN_DZ_DEPTH, q )) < 0 )
			return( -1 );
		depth = i;
	}

	if( (q = im_getnextoption( &p )) ) {
		if( im_isprefix( "cen", q ) ) 
			centre = TRUE;
	}

	if( (q = im_getnextoption( &p )) ) {
		if( (i = vips_enum_from_nick( "im_vips2dz", 
			VIPS_TYPE_ANGLE, q )) < 0 )
			return( -1 );
		angle = i;
	}

	if( vips_dzsave( in, name,
		"layout", layout,
		"suffix", suffix,
		"overlap", overlap,
		"tile_size", tile_size,
		"depth", depth,
		"centre", centre,
		"angle", angle,
		NULL ) )
		return( -1 );

	return( 0 );
}