static int parse_line(char *str)
{
	uint8_t array[256];
	uint16_t value, pskey, length = 0;
	char *off, *end;

	pskey = strtol(str + 1, NULL, 16);
	off = strstr(str, "=");
	if (!off)
		return -EIO;

	off++;

	while (1) {
		value = strtol(off, &end, 16);
		if (value == 0 && off == end)
			break;

		array[length++] = value & 0xff;
		array[length++] = value >> 8;

		if (*end == '\0')
			break;

		off = end + 1;
	}

	return psr_put(pskey, array, length);
}