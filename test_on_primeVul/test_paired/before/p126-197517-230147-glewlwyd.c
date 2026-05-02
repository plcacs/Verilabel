static json_t * check_attestation_fido_u2f(json_t * j_params, unsigned char * credential_id, size_t credential_id_len, unsigned char * cert_x, size_t cert_x_len, unsigned char * cert_y, size_t cert_y_len, cbor_item_t * att_stmt, unsigned char * rpid_hash, size_t rpid_hash_len, const unsigned char * client_data) {
  json_t * j_error = json_array(), * j_return;
  cbor_item_t * key = NULL, * x5c = NULL, * sig = NULL, * att_cert = NULL;
  int i, ret;
  char * message = NULL;
  gnutls_pubkey_t pubkey = NULL;
  gnutls_x509_crt_t cert = NULL;
  gnutls_datum_t cert_dat, data, signature, cert_issued_by;
  unsigned char data_signed[200], client_data_hash[32], cert_export[32], cert_export_b64[64];
  size_t data_signed_offset = 0, client_data_hash_len = 32, cert_export_len = 32, cert_export_b64_len = 0;
  
  if (j_error != NULL) {
    do {
      if (gnutls_x509_crt_init(&cert)) {
        json_array_append_new(j_error, json_string("check_attestation_fido_u2f - Error gnutls_x509_crt_init"));
        break;
      }
      if (gnutls_pubkey_init(&pubkey)) {
        json_array_append_new(j_error, json_string("check_attestation_fido_u2f - Error gnutls_pubkey_init"));
        break;
      }
      
      // Step 1
      if (att_stmt == NULL || !cbor_isa_map(att_stmt) || cbor_map_size(att_stmt) != 2) {
        json_array_append_new(j_error, json_string("CBOR map value 'attStmt' invalid format"));
        break;
      }
      for (i=0; i<2; i++) {
        key = cbor_map_handle(att_stmt)[i].key;
        if (cbor_isa_string(key)) {
          if (0 == o_strncmp((const char *)cbor_string_handle(key), "x5c", MIN(o_strlen("x5c"), cbor_string_length(key)))) {
            x5c = cbor_map_handle(att_stmt)[i].value;
          } else if (0 == o_strncmp((const char *)cbor_string_handle(key), "sig", MIN(o_strlen("sig"), cbor_string_length(key)))) {
            sig = cbor_map_handle(att_stmt)[i].value;
          } else {
            message = msprintf("attStmt map element %d key is not valid: '%.*s'", i, cbor_string_length(key), cbor_string_handle(key));
            json_array_append_new(j_error, json_string(message));
            o_free(message);
            break;
          }
        } else {
          message = msprintf("attStmt map element %d key is not a string", i);
          json_array_append_new(j_error, json_string(message));
          o_free(message);
          break;
        }
      }
      if (x5c == NULL || !cbor_isa_array(x5c) || cbor_array_size(x5c) != 1) {
        json_array_append_new(j_error, json_string("CBOR map value 'x5c' invalid format"));
        break;
      }
      att_cert = cbor_array_get(x5c, 0);
      cert_dat.data = cbor_bytestring_handle(att_cert);
      cert_dat.size = cbor_bytestring_length(att_cert);
      if ((ret = gnutls_x509_crt_import(cert, &cert_dat, GNUTLS_X509_FMT_DER)) < 0) {
        json_array_append_new(j_error, json_string("Error importing x509 certificate"));
        y_log_message(Y_LOG_LEVEL_DEBUG, "check_attestation_fido_u2f - Error gnutls_pcert_import_x509_raw: %d", ret);
        break;
      }
      if (json_object_get(j_params, "root-ca-list") != json_null() && validate_certificate_from_root(j_params, cert, x5c) != G_OK) {
        json_array_append_new(j_error, json_string("Unrecognized certificate authority"));
        if (gnutls_x509_crt_get_issuer_dn2(cert, &cert_issued_by) >= 0) {
          message = msprintf("Unrecognized certificate autohority: %.*s", cert_issued_by.size, cert_issued_by.data);
          y_log_message(Y_LOG_LEVEL_DEBUG, "check_attestation_fido_u2f - %s", message);
          o_free(message);
          gnutls_free(cert_issued_by.data);
        } else {
          y_log_message(Y_LOG_LEVEL_DEBUG, "check_attestation_fido_u2f - Unrecognized certificate autohority (unable to get issuer dn)");
        }
        break;
      }
      if ((ret = gnutls_pubkey_import_x509(pubkey, cert, 0)) < 0) {
        json_array_append_new(j_error, json_string("Error importing x509 certificate"));
        y_log_message(Y_LOG_LEVEL_DEBUG, "check_attestation_fido_u2f - Error gnutls_pubkey_import_x509: %d", ret);
        break;
      }
      if ((ret = gnutls_x509_crt_get_key_id(cert, GNUTLS_KEYID_USE_SHA256, cert_export, &cert_export_len)) < 0) {
        json_array_append_new(j_error, json_string("Error exporting x509 certificate"));
        y_log_message(Y_LOG_LEVEL_DEBUG, "check_attestation_fido_u2f - Error gnutls_x509_crt_get_key_id: %d", ret);
        break;
      }
      if (!o_base64_encode(cert_export, cert_export_len, cert_export_b64, &cert_export_b64_len)) {
        json_array_append_new(j_error, json_string("Internal error"));
        y_log_message(Y_LOG_LEVEL_DEBUG, "check_attestation_fido_u2f - Error o_base64_encode cert_export");
        break;
      }
      if (!generate_digest_raw(digest_SHA256, client_data, o_strlen((char *)client_data), client_data_hash, &client_data_hash_len)) {
        json_array_append_new(j_error, json_string("Internal error"));
        y_log_message(Y_LOG_LEVEL_ERROR, "check_attestation_fido_u2f - Error generate_digest_raw client_data");
        break;
      }

      if (sig == NULL || !cbor_isa_bytestring(sig)) {
        json_array_append_new(j_error, json_string("Error sig is not a bytestring"));
        break;
      }
      
      // Build bytestring to verify signature
      data_signed[0] = 0x0;
      data_signed_offset = 1;
      
      memcpy(data_signed+data_signed_offset, rpid_hash, rpid_hash_len);
      data_signed_offset += rpid_hash_len;
      
      memcpy(data_signed+data_signed_offset, client_data_hash, client_data_hash_len);
      data_signed_offset+=client_data_hash_len;
      
      memcpy(data_signed+data_signed_offset, credential_id, credential_id_len);
      data_signed_offset+=credential_id_len;
      
      data_signed[data_signed_offset] = 0x04;
      data_signed_offset++;
      
      memcpy(data_signed+data_signed_offset, cert_x, cert_x_len);
      data_signed_offset+=cert_x_len;
      
      memcpy(data_signed+data_signed_offset, cert_y, cert_y_len);
      data_signed_offset+=cert_y_len;
        
      // Let's verify sig over data_signed
      data.data = data_signed;
      data.size = data_signed_offset;
      
      signature.data = cbor_bytestring_handle(sig);
      signature.size = cbor_bytestring_length(sig);
      
      if (gnutls_pubkey_verify_data2(pubkey, GNUTLS_SIGN_ECDSA_SHA256, 0, &data, &signature)) {
        json_array_append_new(j_error, json_string("Invalid signature"));
      }
      
    } while (0);
    
    if (json_array_size(j_error)) {
      j_return = json_pack("{sisO}", "result", G_ERROR_PARAM, "error", j_error);
    } else {
      j_return = json_pack("{sis{ss%}}", "result", G_OK, "data", "certificate", cert_export_b64, cert_export_b64_len);
    }
    json_decref(j_error);
    gnutls_pubkey_deinit(pubkey);
    gnutls_x509_crt_deinit(cert);
    if (att_cert != NULL) {
      cbor_decref(&att_cert);
    }
    
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "check_attestation_fido_u2f - Error allocating resources for j_error");
    j_return = json_pack("{si}", "result", G_ERROR);
  }
  return j_return;
}