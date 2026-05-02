static pyc_object *get_complex_object(RzBinPycObj *pyc, RzBuffer *buffer) {
	pyc_object *ret = NULL;
	bool error = false;
	ut32 size = 0;
	ut32 n1 = 0;
	ut32 n2 = 0;

	ret = RZ_NEW0(pyc_object);
	if (!ret) {
		return NULL;
	}

	if ((pyc->magic_int & 0xffff) <= 62061) {
		n1 = get_ut8(buffer, &error);
	} else {
		n1 = get_st32(buffer, &error);
	}
	if (error) {
		free(ret);
		return NULL;
	}
	ut8 *s1 = malloc(n1 + 1);
	if (!s1) {
		return NULL;
	}
	/* object contain string representation of the number */
	size = rz_buf_read(buffer, s1, n1);
	if (size != n1) {
		RZ_FREE(s1);
		RZ_FREE(ret);
		return NULL;
	}
	s1[n1] = '\0';

	if ((pyc->magic_int & 0xffff) <= 62061) {
		n2 = get_ut8(buffer, &error);
	} else
		n2 = get_st32(buffer, &error);
	if (error) {
		return NULL;
	}
	ut8 *s2 = malloc(n2 + 1);
	if (!s2) {
		return NULL;
	}
	/* object contain string representation of the number */
	size = rz_buf_read(buffer, s2, n2);
	if (size != n2) {
		RZ_FREE(s1);
		RZ_FREE(s2);
		RZ_FREE(ret);
		return NULL;
	}
	s2[n2] = '\0';

	ret->type = TYPE_COMPLEX;
	ret->data = rz_str_newf("%s+%sj", s1, s2);
	RZ_FREE(s1);
	RZ_FREE(s2);
	if (!ret->data) {
		RZ_FREE(ret);
		return NULL;
	}
	return ret;
}