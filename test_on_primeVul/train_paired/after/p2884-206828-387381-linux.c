static void k_ascii(struct vc_data *vc, unsigned char value, char up_flag)
{
	unsigned int base;

	if (up_flag)
		return;

	if (value < 10) {
		/* decimal input of code, while Alt depressed */
		base = 10;
	} else {
		/* hexadecimal input of code, while AltGr depressed */
		value -= 10;
		base = 16;
	}

	if (!npadch_active) {
		npadch_value = 0;
		npadch_active = true;
	}

	npadch_value = npadch_value * base + value;
}