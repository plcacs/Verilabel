writepid (const char* pidfile, pid_t pid)
{
	int fd;
	char pidbuf[32];
	size_t count = 0;
	snprintf(pidbuf, sizeof(pidbuf), "%lu\n", (unsigned long)pid);

	if((fd = open(pidfile, O_WRONLY | O_CREAT | O_TRUNC
#ifdef O_NOFOLLOW
		| O_NOFOLLOW
#endif
		, 0644)) == -1) {
		log_err("cannot open pidfile %s: %s", 
			pidfile, strerror(errno));
		return;
	}
	while(count < strlen(pidbuf)) {
		ssize_t r = write(fd, pidbuf+count, strlen(pidbuf)-count);
		if(r == -1) {
			if(errno == EAGAIN || errno == EINTR)
				continue;
			log_err("cannot write to pidfile %s: %s",
				pidfile, strerror(errno));
			break;
		}
		count += r;
	}
	close(fd);
}