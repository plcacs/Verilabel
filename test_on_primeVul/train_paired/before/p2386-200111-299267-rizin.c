static pyc_object *get_long_object(RzBuffer *buffer) {
	pyc_object *ret = NULL;
	bool error = false;
	bool neg = false;
	ut32 tmp = 0;
	size_t size;
	size_t i, j = 0, left = 0;
	ut32 n;
	char *hexstr;
	char digist2hex[] = "0123456789abcdef";

	st32 ndigits = get_st32(buffer, &error);
	if (error) {
		return NULL;
	}
	ret = RZ_NEW0(pyc_object);
	if (!ret) {
		return NULL;
	}
	ret->type = TYPE_LONG;
	if (ndigits < 0) {
		ndigits = -ndigits;
		neg = true;
	}
	if (ndigits == 0) {
		ret->data = strdup("0x0");
	} else {
		size = ndigits * 15;
		size = (size - 1) / 4 + 1;
		size += 4 + (neg ? 1 : 0);
		hexstr = malloc(size);
		if (!hexstr) {
			free(ret);
			return NULL;
		}
		memset(hexstr, 0x20, size);
		j = size - 1;
		hexstr[j] = 0;
		for (i = 0; i < ndigits; i++) {
			n = get_ut16(buffer, &error);
			tmp |= n << left;
			left += 15;

			while (left >= 4) {
				hexstr[--j] = digist2hex[tmp & 0xf];
				tmp >>= 4;
				left -= 4;
			}
		}

		if (tmp) {
			hexstr[--j] = digist2hex[tmp & 0xf];
		}

		hexstr[--j] = 'x';
		hexstr[--j] = '0';
		if (neg) {
			hexstr[--j] = '-';
		}

		rz_str_trim(hexstr);
		ret->data = hexstr;
	}
	return ret;
}