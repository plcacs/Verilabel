jio_snprintf(char * str, int n, const char * format, ...)
{
	va_list args;
	int result;

	Trc_SC_snprintf_Entry();

	va_start(args, format);
#if defined(WIN32) && !defined(WIN32_IBMC)
	result = _vsnprintf( str, n, format, args );
#else
	result = vsprintf( str, format, args );
#endif
	va_end(args);

	Trc_SC_snprintf_Exit(result);

	return result;

}