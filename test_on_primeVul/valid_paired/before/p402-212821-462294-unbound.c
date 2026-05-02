writepid (const char* pidfile, pid_t pid)
{
	FILE* f;

	if ((f = fopen(pidfile, "w")) ==  NULL ) {
		log_err("cannot open pidfile %s: %s", 
			pidfile, strerror(errno));
		return;
	}
	if(fprintf(f, "%lu\n", (unsigned long)pid) < 0) {
		log_err("cannot write to pidfile %s: %s", 
			pidfile, strerror(errno));
	}
	fclose(f);
}