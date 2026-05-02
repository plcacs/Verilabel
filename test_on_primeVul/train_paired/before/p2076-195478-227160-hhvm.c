void rfc1867PostHandler(Transport* transport,
                        Array& post,
                        Array& files,
                        size_t content_length,
                        const void*& data, size_t& size,
                        const std::string boundary) {
  char *s=nullptr, *start_arr=nullptr;
  std::string array_index, abuf;
  char *lbuf=nullptr;
  int total_bytes=0, cancel_upload=0, is_arr_upload=0, array_len=0;
  int max_file_size=0, skip_upload=0, anonindex=0, is_anonymous;
  std::set<std::string> &uploaded_files = s_rfc1867_data->rfc1867UploadedFiles;
  multipart_buffer *mbuff;
  int fd=-1;
  void *event_extra_data = nullptr;
  unsigned int llen = 0;
  int upload_count = RuntimeOption::MaxFileUploads;

  /* Initialize the buffer */
  if (!(mbuff = multipart_buffer_new(transport,
                                     (const char *)data, size, boundary))) {
    Logger::Warning("Unable to initialize the input buffer");
    return;
  }

  /* Initialize $_FILES[] */
  s_rfc1867_data->rfc1867ProtectedVariables.clear();

  uploaded_files.clear();

  int (*php_rfc1867_callback)(apc_rfc1867_data *rfc1867ApcData,
                              unsigned int event, void *event_data,
                              void **extra) = s_rfc1867_data->rfc1867Callback;

  if (php_rfc1867_callback != nullptr) {
    multipart_event_start event_start;

    event_start.content_length = content_length;
    if (php_rfc1867_callback(&s_rfc1867_data->rfc1867ApcData,
                             MULTIPART_EVENT_START, &event_start,
                             &event_extra_data) == FAILURE) {
      goto fileupload_done;
    }
  }

  while (!multipart_buffer_eof(mbuff)) {
    std::string temp_filename;
    char buff[FILLUNIT];
    char *cd=nullptr,*param=nullptr,*filename=nullptr, *tmp=nullptr;
    size_t blen=0, wlen=0;
    off_t offset;

    header_list header;
    if (!multipart_buffer_headers(mbuff, header)) {
      goto fileupload_done;
    }

    if ((cd = php_mime_get_hdr_value(header, "Content-Disposition"))) {
      char *pair=nullptr;
      int end=0;

      while (isspace(*cd)) {
        ++cd;
      }

      while (*cd && (pair = php_ap_getword(&cd, ';')))
      {
        char *key=nullptr, *word = pair;

        while (isspace(*cd)) {
          ++cd;
        }

        if (strchr(pair, '=')) {
          key = php_ap_getword(&pair, '=');

          if (!strcasecmp(key, "name")) {
            if (param) {
              free(param);
            }
            param = php_ap_getword_conf(&pair);
          } else if (!strcasecmp(key, "filename")) {
            if (filename) {
              free(filename);
            }
            filename = php_ap_getword_conf(&pair);
          }
        }
        if (key) free(key);
        free(word);
      }

      /* Normal form variable, safe to read all data into memory */
      if (!filename && param) {
        unsigned int value_len;
        char *value = multipart_buffer_read_body(mbuff, &value_len);
        unsigned int new_val_len; /* Dummy variable */

        if (!value) {
          value = strdup("");
        }

        new_val_len = value_len;
        if (php_rfc1867_callback != nullptr) {
          multipart_event_formdata event_formdata;
          size_t newlength = new_val_len;

          event_formdata.post_bytes_processed = mbuff->read_post_bytes;
          event_formdata.name = param;
          event_formdata.value = &value;
          event_formdata.length = new_val_len;
          event_formdata.newlength = &newlength;
          if (php_rfc1867_callback(&s_rfc1867_data->rfc1867ApcData,
                                   MULTIPART_EVENT_FORMDATA, &event_formdata,
                                   &event_extra_data) == FAILURE) {
            free(param);
            free(value);
            continue;
          }
          new_val_len = newlength;
        }

        String val(value, new_val_len, CopyString);
        safe_php_register_variable(param, val, post, 0);

        if (!strcasecmp(param, "MAX_FILE_SIZE")) {
          max_file_size = atol(value);
        }

        free(param);
        free(value);
        continue;
      }

      /* If file_uploads=off, skip the file part */
      if (!RuntimeOption::EnableFileUploads) {
        skip_upload = 1;
      } else if (upload_count <= 0) {
        Logger::Warning(
          "Maximum number of allowable file uploads has been exceeded"
        );
        skip_upload = 1;
      }

      /* Return with an error if the posted data is garbled */
      if (!param && !filename) {
        Logger::Warning("File Upload Mime headers garbled");
        goto fileupload_done;
      }

      if (!param) {
        is_anonymous = 1;
        param = (char*)malloc(MAX_SIZE_ANONNAME);
        snprintf(param, MAX_SIZE_ANONNAME, "%u", anonindex++);
      } else {
        is_anonymous = 0;
      }

      /* New Rule: never repair potential malicious user input */
      if (!skip_upload) {
        tmp = param;
        long c = 0;

        while (*tmp) {
          if (*tmp == '[') {
            c++;
          } else if (*tmp == ']') {
            c--;
            if (tmp[1] && tmp[1] != '[') {
              skip_upload = 1;
              break;
            }
          }
          if (c < 0) {
            skip_upload = 1;
            break;
          }
          tmp++;
        }
      }

      total_bytes = cancel_upload = 0;

      if (!skip_upload) {
        /* Handle file */
        char path[PATH_MAX];

        // open a temporary file
        snprintf(path, sizeof(path), "%s/XXXXXX",
                 RuntimeOption::UploadTmpDir.c_str());
        fd = mkstemp(path);
        upload_count--;
        if (fd == -1) {
          Logger::Warning("Unable to open temporary file");
          Logger::Warning("File upload error - unable to create a "
                          "temporary file");
          cancel_upload = UPLOAD_ERROR_E;
        }
        temp_filename = path;
      }

      if (!skip_upload && php_rfc1867_callback != nullptr) {
        multipart_event_file_start event_file_start;

        event_file_start.post_bytes_processed = mbuff->read_post_bytes;
        event_file_start.name = param;
        event_file_start.filename = &filename;
        if (php_rfc1867_callback(&s_rfc1867_data->rfc1867ApcData,
                                 MULTIPART_EVENT_FILE_START,
                                 &event_file_start,
                                 &event_extra_data) == FAILURE) {
          if (!temp_filename.empty()) {
            if (cancel_upload != UPLOAD_ERROR_E) { /* file creation failed */
              close(fd);
              unlink(temp_filename.c_str());
            }
          }
          temp_filename="";
          free(param);
          free(filename);
          continue;
        }
      }


      if (skip_upload) {
        free(param);
        free(filename);
        continue;
      }

      if (strlen(filename) == 0) {
        Logger::Verbose("No file uploaded");
        cancel_upload = UPLOAD_ERROR_D;
      }

      offset = 0;
      end = 0;
      while (!cancel_upload &&
             (blen = multipart_buffer_read(mbuff, buff, sizeof(buff), &end)))
      {
        if (php_rfc1867_callback != nullptr) {
          multipart_event_file_data event_file_data;

          event_file_data.post_bytes_processed = mbuff->read_post_bytes;
          event_file_data.offset = offset;
          event_file_data.data = buff;
          event_file_data.length = blen;
          event_file_data.newlength = &blen;
          if (php_rfc1867_callback(&s_rfc1867_data->rfc1867ApcData,
                                   MULTIPART_EVENT_FILE_DATA,
                                   &event_file_data,
                                   &event_extra_data) == FAILURE) {
            cancel_upload = UPLOAD_ERROR_X;
            continue;
          }
        }


        if (VirtualHost::GetUploadMaxFileSize() > 0 &&
            total_bytes > VirtualHost::GetUploadMaxFileSize()) {
          Logger::Verbose("upload_max_filesize of %" PRId64 " bytes exceeded "
                          "- file [%s=%s] not saved",
                          VirtualHost::GetUploadMaxFileSize(),
                          param, filename);
          cancel_upload = UPLOAD_ERROR_A;
        } else if (max_file_size && (total_bytes > max_file_size)) {
          Logger::Verbose("MAX_FILE_SIZE of %d bytes exceeded - "
                          "file [%s=%s] not saved",
                          max_file_size, param, filename);
          cancel_upload = UPLOAD_ERROR_B;
        } else if (blen > 0) {

          wlen = folly::writeFull(fd, buff, blen);
          if (wlen < blen) {
            Logger::Verbose("Only %zd bytes were written, expected to "
                            "write %zd", wlen, blen);
            cancel_upload = UPLOAD_ERROR_F;
          } else {
            total_bytes += wlen;
          }

          offset += wlen;
        }
      }
      if (fd!=-1) { /* may not be initialized if file could not be created */
        close(fd);
      }
      if (!cancel_upload && !end) {
        Logger::Verbose("Missing mime boundary at the end of the data for "
                        "file %s", strlen(filename) > 0 ? filename : "");
        cancel_upload = UPLOAD_ERROR_C;
      }
      if (strlen(filename) > 0 && total_bytes == 0 && !cancel_upload) {
        Logger::Verbose("Uploaded file size 0 - file [%s=%s] not saved",
                        param, filename);
        cancel_upload = 5;
      }

      if (php_rfc1867_callback != nullptr) {
        multipart_event_file_end event_file_end;

        event_file_end.post_bytes_processed = mbuff->read_post_bytes;
        event_file_end.temp_filename = temp_filename.c_str();
        event_file_end.cancel_upload = cancel_upload;
        if (php_rfc1867_callback(&s_rfc1867_data->rfc1867ApcData,
                                 MULTIPART_EVENT_FILE_END,
                                 &event_file_end,
                                 &event_extra_data) == FAILURE) {
          cancel_upload = UPLOAD_ERROR_X;
        }
      }

      if (cancel_upload && cancel_upload != UPLOAD_ERROR_C) {
        if (!temp_filename.empty()) {
          if (cancel_upload != UPLOAD_ERROR_E) { /* file creation failed */
            unlink(temp_filename.c_str());
          }
        }
        temp_filename="";
      } else {
        s_rfc1867_data->rfc1867UploadedFiles.insert(temp_filename);
      }

      /* is_arr_upload is true when name of file upload field
       * ends in [.*]
       * start_arr is set to point to 1st [
       */
      is_arr_upload = (start_arr = strchr(param,'[')) &&
                      (param[strlen(param)-1] == ']');

      if (is_arr_upload) {
        array_len = strlen(start_arr);
        array_index = std::string(start_arr+1, array_len-2);
      }

      /* Add $foo_name */
      if (llen < strlen(param) + MAX_SIZE_OF_INDEX + 1) {
        llen = strlen(param);
        lbuf = (char *) realloc(lbuf, llen + MAX_SIZE_OF_INDEX + 1);
        llen += MAX_SIZE_OF_INDEX + 1;
      }

      if (is_arr_upload) {
        abuf = std::string(param, strlen(param)-array_len);
        snprintf(lbuf, llen, "%s_name[%s]",
                 abuf.c_str(), array_index.c_str());
      } else {
        snprintf(lbuf, llen, "%s_name", param);
      }

      /* The \ check should technically be needed for win32 systems only
       * where it is a valid path separator. However, IE in all it's wisdom
       * always sends the full path of the file on the user's filesystem,
       * which means that unless the user does basename() they get a bogus
       * file name. Until IE's user base drops to nill or problem is fixed
       * this code must remain enabled for all systems.
       */
      s = strrchr(filename, '\\');
      if ((tmp = strrchr(filename, '/')) > s) {
        s = tmp;
      }

      Array globals = php_globals_as_array();
      if (!is_anonymous) {
        if (s) {
          String val(s+1, strlen(s+1), CopyString);
          safe_php_register_variable(lbuf, val, globals, 0);
        } else {
          String val(filename, strlen(filename), CopyString);
          safe_php_register_variable(lbuf, val, globals, 0);
        }
      }

      /* Add $foo[name] */
      if (is_arr_upload) {
        snprintf(lbuf, llen, "%s[name][%s]",
                 abuf.c_str(), array_index.c_str());
      } else {
        snprintf(lbuf, llen, "%s[name]", param);
      }
      if (s) {
        String val(s+1, strlen(s+1), CopyString);
        safe_php_register_variable(lbuf, val, files, 0);
      } else {
        String val(filename, strlen(filename), CopyString);
        safe_php_register_variable(lbuf, val, files, 0);
      }
      free(filename);
      s = nullptr;

      /* Possible Content-Type: */
      if ((cancel_upload && cancel_upload != UPLOAD_ERROR_C) ||
          !(cd = php_mime_get_hdr_value(header, "Content-Type"))) {
        cd = "";
      } else {
        /* fix for Opera 6.01 */
        s = strchr(cd, ';');
        if (s != nullptr) {
          *s = '\0';
        }
      }

      /* Add $foo_type */
      if (is_arr_upload) {
        snprintf(lbuf, llen, "%s_type[%s]",
                 abuf.c_str(), array_index.c_str());
      } else {
        snprintf(lbuf, llen, "%s_type", param);
      }
      if (!is_anonymous) {
        String val(cd, strlen(cd), CopyString);
        safe_php_register_variable(lbuf, val, globals, 0);
      }

      /* Add $foo[type] */
      if (is_arr_upload) {
        snprintf(lbuf, llen, "%s[type][%s]",
                 abuf.c_str(), array_index.c_str());
      } else {
        snprintf(lbuf, llen, "%s[type]", param);
      }
      String val(cd, strlen(cd), CopyString);
      safe_php_register_variable(lbuf, val, files, 0);

      /* Restore Content-Type Header */
      if (s != nullptr) {
        *s = ';';
      }
      s = "";

      /* Initialize variables */
      add_protected_variable(param);

      Variant tempFileName(temp_filename);

      /* if param is of form xxx[.*] this will cut it to xxx */
      if (!is_anonymous) {
        safe_php_register_variable(param, tempFileName, globals, 1);
      }

      /* Add $foo[tmp_name] */
      if (is_arr_upload) {
        snprintf(lbuf, llen, "%s[tmp_name][%s]",
                 abuf.c_str(), array_index.c_str());
      } else {
        snprintf(lbuf, llen, "%s[tmp_name]", param);
      }
      add_protected_variable(lbuf);
      safe_php_register_variable(lbuf, tempFileName, files, 1);

      Variant file_size, error_type;

      error_type = cancel_upload;

      /* Add $foo[error] */
      if (cancel_upload) {
        file_size = 0;
      } else {
        file_size = total_bytes;
      }

      if (is_arr_upload) {
        snprintf(lbuf, llen, "%s[error][%s]",
                 abuf.c_str(), array_index.c_str());
      } else {
        snprintf(lbuf, llen, "%s[error]", param);
      }
      safe_php_register_variable(lbuf, error_type, files, 0);

      /* Add $foo_size */
      if (is_arr_upload) {
        snprintf(lbuf, llen, "%s_size[%s]",
                 abuf.c_str(), array_index.c_str());
      } else {
        snprintf(lbuf, llen, "%s_size", param);
      }
      if (!is_anonymous) {
        safe_php_register_variable(lbuf, file_size, globals, 0);
      }

      /* Add $foo[size] */
      if (is_arr_upload) {
        snprintf(lbuf, llen, "%s[size][%s]",
                 abuf.c_str(), array_index.c_str());
      } else {
        snprintf(lbuf, llen, "%s[size]", param);
      }
      safe_php_register_variable(lbuf, file_size, files, 0);
      free(param);
    }
  }
fileupload_done:
  data = mbuff->post_data;
  size = mbuff->post_size;
  if (php_rfc1867_callback != nullptr) {
    multipart_event_end event_end;

    event_end.post_bytes_processed = mbuff->read_post_bytes;
    php_rfc1867_callback(&s_rfc1867_data->rfc1867ApcData,
                         MULTIPART_EVENT_END, &event_end, &event_extra_data);
  }
  if (lbuf) free(lbuf);
  s_rfc1867_data->rfc1867ProtectedVariables.clear();
  if (mbuff->boundary_next) free(mbuff->boundary_next);
  if (mbuff->boundary) free(mbuff->boundary);
  if (mbuff->buffer) free(mbuff->buffer);
  if (mbuff) free(mbuff);
}