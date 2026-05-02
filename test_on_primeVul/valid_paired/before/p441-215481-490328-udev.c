size_t util_path_encode(char *s, size_t len)
{
	char t[(len * 3)+1];
	size_t i, j;

	for (i = 0, j = 0; s[i] != '\0'; i++) {
		if (s[i] == '/') {
			memcpy(&t[j], "\\x2f", 4);
			j += 4;
		} else if (s[i] == '\\') {
			memcpy(&t[j], "\\x5c", 4);
			j += 4;
		} else {
			t[j] = s[i];
			j++;
		}
	}
	if (len == 0)
		return j;
	i = (j < len - 1) ? j : len - 1;
	memcpy(s, t, i);
	s[i] = '\0';
	return j;
}