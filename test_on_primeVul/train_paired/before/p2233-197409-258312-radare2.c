static int r_cmd_java_call(void *user, const char *input) {
	RCore *core = (RCore *) user;
	int res = false;
	ut32 i = 0;
	if (strncmp (input, "java", 4)) {
		return false;
	}
	if (input[4] != ' ') {
		return r_cmd_java_handle_help (core, input);
	}
	for (; i < END_CMDS; i++) {
		//IFDBG r_cons_printf ("Checking cmd: %s %d %s\n", JAVA_CMDS[i].name, JAVA_CMDS[i].name_len, p);
		IFDBG r_cons_printf ("Checking cmd: %s %d\n", JAVA_CMDS[i].name,
			strncmp (input+5, JAVA_CMDS[i].name, JAVA_CMDS[i].name_len));
		if (!strncmp (input + 5, JAVA_CMDS[i].name, JAVA_CMDS[i].name_len)) {
			const char *cmd = input + 5 + JAVA_CMDS[i].name_len;
			if (*cmd && *cmd == ' ') {
				cmd++;
			}
			//IFDBG r_cons_printf ("Executing cmd: %s (%s)\n", JAVA_CMDS[i].name, cmd+5+JAVA_CMDS[i].name_len );
			res =  JAVA_CMDS[i].handler (core, cmd);
			break;
		}
	}
	if (!res) {
		return r_cmd_java_handle_help (core, input);
	}
	return true;
}