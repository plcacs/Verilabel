jio_vsnprintf(char * str, int n, const char * format, va_list args)
{
	int result;

	Trc_SC_vsnprintf_Entry(str, n, format);

#if defined(WIN32) && !defined(WIN32_IBMC)
	result = _vsnprintf( str, n, format, args );
#else 
	result = vsprintf( str, format, args );
#endif

	Trc_SC_vsnprintf_Exit(result);

	return result;
}