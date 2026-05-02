static void credential_write_item(FILE *fp, const char *key, const char *value)
{
	if (!value)
		return;
	if (strchr(value, '\n'))
		die("credential value for %s contains newline", key);
	fprintf(fp, "%s=%s\n", key, value);
}