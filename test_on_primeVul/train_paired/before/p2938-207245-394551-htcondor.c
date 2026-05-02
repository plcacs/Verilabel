extract_VOMS_info( globus_gsi_cred_handle_t cred_handle, int verify_type, char **voname, char **firstfqan, char **quoted_DN_and_FQAN)
{

#if !defined(HAVE_EXT_VOMS)
	return 1;
#else

	int ret;
	struct vomsdata *voms_data = NULL;
	struct voms *voms_cert  = NULL;
	char *subject_name = NULL;
	char **fqan = NULL;
	int voms_err;
	int fqan_len = 0;
	char *retfqan = NULL;
	char *tmp_scan_ptr = NULL;

	STACK_OF(X509) *chain = NULL;
	X509 *cert = NULL;

	char* x509_fqan_delimiter = NULL;

	if ( activate_globus_gsi() != 0 ) {
		return 1;
	}

	// calling this function on something that doesn't have VOMS attributes
	// should return error 1.  when the config knob disables VOMS, behave the
	// same way.
	if (!param_boolean_int("USE_VOMS_ATTRIBUTES", 1)) {
		return 1;
	}

	ret = (*globus_gsi_cred_get_cert_chain_ptr)(cred_handle, &chain);
	if(ret != GLOBUS_SUCCESS) {
		ret = 10;
		goto end;
	}

	ret = (*globus_gsi_cred_get_cert_ptr)(cred_handle, &cert);
	if(ret != GLOBUS_SUCCESS) {
		ret = 11;
		goto end;
	}

	if ((*globus_gsi_cred_get_identity_name_ptr)(cred_handle, &subject_name)) {
		set_error_string( "unable to extract subject name" );
		ret = 12;
		goto end;
	}

	voms_data = (*VOMS_Init_ptr)(NULL, NULL);
	if (voms_data == NULL) {
		ret = 13;
		goto end;
	}

	if (verify_type == 0) {
		ret = (*VOMS_SetVerificationType_ptr)( VERIFY_NONE, voms_data, &voms_err );
		if (ret == 0) {
			(*VOMS_ErrorMessage_ptr)(voms_data, voms_err, NULL, 0);
			ret = voms_err;
			goto end;
		}
	}

	ret = (*VOMS_Retrieve_ptr)(cert, chain, RECURSE_CHAIN,
						voms_data, &voms_err);
	if (ret == 0) {
		if (voms_err == VERR_NOEXT) {
			// No VOMS extensions present
			ret = 1;
			goto end;
		} else {
			(*VOMS_ErrorMessage_ptr)(voms_data, voms_err, NULL, 0);
			ret = voms_err;
			goto end;
		}
	}

	// we only support one cert for now.  serializing and encoding all the
	// attributes is bad enough, i don't want to deal with doing this to
	// multiple certs.
	voms_cert = voms_data->data[0];

	// fill in the unquoted versions of things
	if(voname) {
		*voname = strdup(voms_cert->voname);
	}

	if(firstfqan) {
		*firstfqan = strdup(voms_cert->fqan[0]);
	}

	// only construct the quoted_DN_and_FQAN if needed
	if (quoted_DN_and_FQAN) {
		// get our delimiter and trim it
		if (!(x509_fqan_delimiter = param("X509_FQAN_DELIMITER"))) {
			x509_fqan_delimiter = strdup(",");
		}
		tmp_scan_ptr = trim_quotes(x509_fqan_delimiter);
		free(x509_fqan_delimiter);
		x509_fqan_delimiter = tmp_scan_ptr;

		// calculate the length
		fqan_len = 0;

		// start with the length of the quoted DN
		tmp_scan_ptr = quote_x509_string( subject_name );
		fqan_len += strlen( tmp_scan_ptr );
		free(tmp_scan_ptr);

		// add the length of delimiter plus each voms attribute
		for (fqan = voms_cert->fqan; fqan && *fqan; fqan++) {
			// delimiter
			fqan_len += strlen(x509_fqan_delimiter);

			tmp_scan_ptr = quote_x509_string( *fqan );
			fqan_len += strlen( tmp_scan_ptr );
			free(tmp_scan_ptr);
		}

		// now malloc enough room for the quoted DN, quoted attrs, delimiters, and
		// NULL terminator
		retfqan = (char*) malloc (fqan_len+1);
		*retfqan = 0;  // set null terminiator

		// reset length counter -- we use this for efficient appending.
		fqan_len = 0;

		// start with the quoted DN
		tmp_scan_ptr = quote_x509_string( subject_name );
		strcat(retfqan, tmp_scan_ptr);
		fqan_len += strlen( tmp_scan_ptr );
		free(tmp_scan_ptr);

		// add the delimiter plus each voms attribute
		for (fqan = voms_cert->fqan; fqan && *fqan; fqan++) {
			// delimiter
			strcat(&(retfqan[fqan_len]), x509_fqan_delimiter);
			fqan_len += strlen(x509_fqan_delimiter);

			tmp_scan_ptr = quote_x509_string( *fqan );
			strcat(&(retfqan[fqan_len]), tmp_scan_ptr);
			fqan_len += strlen( tmp_scan_ptr );
			free(tmp_scan_ptr);
		}

		*quoted_DN_and_FQAN = retfqan;
	}

	ret = 0;

end:
	free(subject_name);
	free(x509_fqan_delimiter);
	if (voms_data)
		(*VOMS_Destroy_ptr)(voms_data);
	if (cert)
		X509_free(cert);
	if(chain)
		sk_X509_pop_free(chain, X509_free);

	return ret;
#endif

}