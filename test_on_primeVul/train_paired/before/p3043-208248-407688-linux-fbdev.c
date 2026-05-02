static int i740fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	switch (var->bits_per_pixel) {
	case 8:
		var->red.offset	= var->green.offset = var->blue.offset = 0;
		var->red.length	= var->green.length = var->blue.length = 8;
		break;
	case 16:
		switch (var->green.length) {
		default:
		case 5:
			var->red.offset = 10;
			var->green.offset = 5;
			var->blue.offset = 0;
			var->red.length	= 5;
			var->green.length = 5;
			var->blue.length = 5;
			break;
		case 6:
			var->red.offset = 11;
			var->green.offset = 5;
			var->blue.offset = 0;
			var->red.length = var->blue.length = 5;
			break;
		}
		break;
	case 24:
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->red.length	= var->green.length = var->blue.length = 8;
		break;
	case 32:
		var->transp.offset = 24;
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->transp.length = 8;
		var->red.length = var->green.length = var->blue.length = 8;
		break;
	default:
		return -EINVAL;
	}

	if (var->xres > var->xres_virtual)
		var->xres_virtual = var->xres;

	if (var->yres > var->yres_virtual)
		var->yres_virtual = var->yres;

	if (info->monspecs.hfmax && info->monspecs.vfmax &&
	    info->monspecs.dclkmax && fb_validate_mode(var, info) < 0)
		return -EINVAL;

	return 0;
}