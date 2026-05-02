void color_esycc_to_rgb(opj_image_t *image)
{
	int y, cb, cr, sign1, sign2, val;
	unsigned int w, h, max, i;
	int flip_value = (1 << (image->comps[0].prec-1));
	int max_value = (1 << image->comps[0].prec) - 1;
	
	if (
		    (image->numcomps < 3)
		 || (image->comps[0].dx != image->comps[1].dx) || (image->comps[0].dx != image->comps[2].dx)
		 || (image->comps[0].dy != image->comps[1].dy) || (image->comps[0].dy != image->comps[2].dy)
	   ) {
		fprintf(stderr,"%s:%d:color_esycc_to_rgb\n\tCAN NOT CONVERT\n", __FILE__,__LINE__);
		return;
	}
	
	w = image->comps[0].w;
	h = image->comps[0].h;
	
	sign1 = (int)image->comps[1].sgnd;
	sign2 = (int)image->comps[2].sgnd;
	
	max = w * h;
	
	for(i = 0; i < max; ++i)
	{
		
		y = image->comps[0].data[i]; cb = image->comps[1].data[i]; cr = image->comps[2].data[i];
		
		if( !sign1) cb -= flip_value;
		if( !sign2) cr -= flip_value;
		
		val = (int)
		((float)y - (float)0.0000368 * (float)cb
		 + (float)1.40199 * (float)cr + (float)0.5);
		
		if(val > max_value) val = max_value; else if(val < 0) val = 0;
		image->comps[0].data[i] = val;
		
		val = (int)
		((float)1.0003 * (float)y - (float)0.344125 * (float)cb
		 - (float)0.7141128 * (float)cr + (float)0.5);
		
		if(val > max_value) val = max_value; else if(val < 0) val = 0;
		image->comps[1].data[i] = val;
		
		val = (int)
		((float)0.999823 * (float)y + (float)1.77204 * (float)cb
		 - (float)0.000008 *(float)cr + (float)0.5);
		
		if(val > max_value) val = max_value; else if(val < 0) val = 0;
		image->comps[2].data[i] = val;
	}
	image->color_space = OPJ_CLRSPC_SRGB;

}/* color_esycc_to_rgb() */
