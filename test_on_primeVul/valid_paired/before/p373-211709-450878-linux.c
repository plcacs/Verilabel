static int __init lp_setup (char *str)
{
	static int parport_ptr;
	int x;

	if (get_option(&str, &x)) {
		if (x == 0) {
			/* disable driver on "lp=" or "lp=0" */
			parport_nr[0] = LP_PARPORT_OFF;
		} else {
			printk(KERN_WARNING "warning: 'lp=0x%x' is deprecated, ignored\n", x);
			return 0;
		}
	} else if (!strncmp(str, "parport", 7)) {
		int n = simple_strtoul(str+7, NULL, 10);
		if (parport_ptr < LP_NO)
			parport_nr[parport_ptr++] = n;
		else
			printk(KERN_INFO "lp: too many ports, %s ignored.\n",
			       str);
	} else if (!strcmp(str, "auto")) {
		parport_nr[0] = LP_PARPORT_AUTO;
	} else if (!strcmp(str, "none")) {
		parport_nr[parport_ptr++] = LP_PARPORT_NONE;
	} else if (!strcmp(str, "reset")) {
		reset = 1;
	}
	return 1;
}