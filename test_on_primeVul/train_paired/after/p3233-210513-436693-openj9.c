jio_vsnprintf(char * str, int n, const char * format, va_list args)
{
	int result;

	Trc_SC_vsnprintf_Entry(str, n, format);

	result = vsnprintf( str, n, format, args );

	Trc_SC_vsnprintf_Exit(result);

	return result;
}