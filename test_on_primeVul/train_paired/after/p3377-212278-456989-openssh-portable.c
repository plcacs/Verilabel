verify_host_key(char *host, struct sockaddr *hostaddr, Key *host_key)
{
	int flags = 0;
	char *fp;
	Key *plain = NULL;

	fp = key_fingerprint(host_key, SSH_FP_MD5, SSH_FP_HEX);
	debug("Server host key: %s %s", key_type(host_key), fp);
	free(fp);

	if (options.verify_host_key_dns) {
		/*
		 * XXX certs are not yet supported for DNS, so downgrade
		 * them and try the plain key.
		 */
		plain = key_from_private(host_key);
		if (key_is_cert(plain))
			key_drop_cert(plain);
		if (verify_host_key_dns(host, hostaddr, plain, &flags) == 0) {
			if (flags & DNS_VERIFY_FOUND) {
				if (options.verify_host_key_dns == 1 &&
				    flags & DNS_VERIFY_MATCH &&
				    flags & DNS_VERIFY_SECURE) {
					key_free(plain);
					return 0;
				}
				if (flags & DNS_VERIFY_MATCH) {
					matching_host_key_dns = 1;
				} else {
					warn_changed_key(plain);
					error("Update the SSHFP RR in DNS "
					    "with the new host key to get rid "
					    "of this message.");
				}
			}
		}
		key_free(plain);
	}

	return check_host_key(host, hostaddr, options.port, host_key, RDRW,
	    options.user_hostfiles, options.num_user_hostfiles,
	    options.system_hostfiles, options.num_system_hostfiles);
}