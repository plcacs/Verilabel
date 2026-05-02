ut32 armass_assemble(const char *str, ut64 off, int thumb) {
	int i, j;
	char buf[128];
	ArmOpcode aop = {.off = off};
	for (i = j = 0; i < sizeof (buf) - 1 && str[i]; i++, j++) {
		if (str[j] == '#') {
			i--; continue;
		}
		buf[i] = tolower ((const ut8)str[j]);
	}
	buf[i] = 0;
	arm_opcode_parse (&aop, buf);
	aop.off = off;
	if (thumb < 0 || thumb > 1) {
		return -1;
	}
	if (!assemble[thumb] (&aop, off, buf)) {
		//eprintf ("armass: Unknown opcode (%s)\n", buf);
		return -1;
	}
	return aop.o;
}