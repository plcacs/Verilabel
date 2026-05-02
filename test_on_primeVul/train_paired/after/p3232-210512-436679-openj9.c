jio_snprintf(char * str, int n, const char * format, ...)
{
	va_list args;
	int result;

	Trc_SC_snprintf_Entry();

	va_start(args, format);
	result = vsnprintf( str, n, format, args );
	va_end(args);

	Trc_SC_snprintf_Exit(result);

	return result;

}