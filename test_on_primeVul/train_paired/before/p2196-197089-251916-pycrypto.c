ALGnew(PyObject *self, PyObject *args, PyObject *kwdict)
{
	unsigned char *key, *IV;
	ALGobject * new=NULL;
	int keylen, IVlen=0, mode=MODE_ECB, segment_size=0;
	PyObject *counter = NULL;
	int counter_shortcut = 0;
#ifdef PCT_ARC2_MODULE
        int effective_keylen = 1024;    /* this is a weird default, but it's compatible with old versions of PyCrypto */
#endif
	/* Set default values */
	if (!PyArg_ParseTupleAndKeywords(args, kwdict, "s#|is#Oi"
#ifdef PCT_ARC2_MODULE
					 "i"
#endif
					 , kwlist,
					 &key, &keylen, &mode, &IV, &IVlen,
					 &counter, &segment_size
#ifdef PCT_ARC2_MODULE
					 , &effective_keylen
#endif
		)) 
	{
		return NULL;
	}

	if (mode<MODE_ECB || mode>MODE_CTR) 
	{
		PyErr_Format(PyExc_ValueError, 
			     "Unknown cipher feedback mode %i",
			     mode);
		return NULL;
	}
	if (mode == MODE_PGP) {
		PyErr_Format(PyExc_ValueError, 
			     "MODE_PGP is not supported anymore");
		return NULL;
	}
	if (KEY_SIZE!=0 && keylen!=KEY_SIZE)
	{
		PyErr_Format(PyExc_ValueError,
			     "Key must be %i bytes long, not %i",
			     KEY_SIZE, keylen);
		return NULL;
	}
	if (KEY_SIZE==0 && keylen==0)
	{
		PyErr_SetString(PyExc_ValueError,
				"Key cannot be the null string");
		return NULL;
	}
	if (IVlen != BLOCK_SIZE && mode != MODE_ECB && mode != MODE_CTR)
	{
		PyErr_Format(PyExc_ValueError,
			     "IV must be %i bytes long", BLOCK_SIZE);
		return NULL;
	}

	/* Mode-specific checks */
	if (mode == MODE_CFB) {
		if (segment_size == 0) segment_size = 8;
		if (segment_size < 1 || segment_size > BLOCK_SIZE*8 || ((segment_size & 7) != 0)) {
			PyErr_Format(PyExc_ValueError, 
				     "segment_size must be multiple of 8 (bits) "
				     "between 1 and %i", BLOCK_SIZE*8);
			return NULL;
		}
	}
	if (mode == MODE_CTR) {
		if (counter == NULL) {
			PyErr_SetString(PyExc_TypeError,
					"'counter' keyword parameter is required with CTR mode");
			return NULL;
		} else if (Py_TYPE(counter) == PCT_CounterBEType || Py_TYPE(counter) == PCT_CounterLEType) {
			counter_shortcut = 1;
		} else if (!PyCallable_Check(counter)) {
			PyErr_SetString(PyExc_ValueError, 
					"'counter' parameter must be a callable object");
			return NULL;
		}
	} else {
		if (counter != NULL) {
			PyErr_SetString(PyExc_ValueError, 
					"'counter' parameter only useful with CTR mode");
			return NULL;
		}
	}

	/* Cipher-specific checks */
#ifdef PCT_ARC2_MODULE
        if (effective_keylen<0 || effective_keylen>1024) {
		PyErr_Format(PyExc_ValueError,
			     "RC2: effective_keylen must be between 0 and 1024, not %i",
			     effective_keylen);
		return NULL;
        }
#endif

	/* Copy parameters into object */
	new = newALGobject();
	new->segment_size = segment_size;
	new->counter = counter;
	Py_XINCREF(counter);
	new->counter_shortcut = counter_shortcut;
#ifdef PCT_ARC2_MODULE
        new->st.effective_keylen = effective_keylen;
#endif

	block_init(&(new->st), key, keylen);
	if (PyErr_Occurred())
	{
		Py_DECREF(new);
		return NULL;
	}
	memset(new->IV, 0, BLOCK_SIZE);
	memset(new->oldCipher, 0, BLOCK_SIZE);
	memcpy(new->IV, IV, IVlen);
	new->mode = mode;
	new->count=BLOCK_SIZE;   /* stores how many bytes in new->oldCipher have been used */
	return new;
}