void send_file_direct(char *file_name) {
   int fh, i, length, delta;
   char str[MAX_PATH_LENGTH], dir[MAX_PATH_LENGTH], charset[80];

   getcwd(dir, sizeof(dir));
   fh = open(file_name, O_RDONLY | O_BINARY);
   if (fh > 0) {
      lseek(fh, 0, SEEK_END);
      length = TELL(fh);
      lseek(fh, 0, SEEK_SET);

      rsprintf("HTTP/1.1 200 Document follows\r\n");
      rsprintf("Server: ELOG HTTP %s-%s\r\n", VERSION, git_revision());
      rsprintf("Accept-Ranges: bytes\r\n");

      /* set expiration time to one day if no thumbnail */
      if (isparam("thumb")) {
         rsprintf("Pragma: no-cache\r\n");
         rsprintf("Cache-control: private, max-age=0, no-cache, no-store\r\n");
      } else {
         rsprintf("Cache-control: public, max-age=86400\r\n");
      }

      if (keep_alive) {
         rsprintf("Connection: Keep-Alive\r\n");
         rsprintf("Keep-Alive: timeout=60, max=10\r\n");
      }

      /* return proper header for file type */
      for (i = 0; i < (int) strlen(file_name); i++)
         str[i] = toupper(file_name[i]);
      str[i] = 0;

      for (i = 0; filetype[i].ext[0]; i++)
         if (chkext(str, filetype[i].ext))
            break;

      if (!getcfg("global", "charset", charset, sizeof(charset)))
         strcpy(charset, DEFAULT_HTTP_CHARSET);

      if (filetype[i].ext[0]) {
         if (strncmp(filetype[i].type, "text", 4) == 0)
            rsprintf("Content-Type: %s;charset=%s\r\n", filetype[i].type, charset);
         else if (strcmp(filetype[i].ext, ".SVG") == 0) {
            rsprintf("Content-Type: %s\r\n", filetype[i].type);
            if (strrchr(file_name, '/'))
               strlcpy(str, strrchr(file_name, '/')+1, sizeof(str));
            else
               strlcpy(str, file_name, sizeof(str));
            if (str[6] == '_' && str[13] == '_')
               rsprintf("Content-Disposition: attachment; filename=\"%s\"\r\n", str+14);
            else
               rsprintf("Content-Disposition: attachment; filename=\"%s\"\r\n", str);
         } else
            rsprintf("Content-Type: %s\r\n", filetype[i].type);
      } else if (is_ascii(file_name))
         rsprintf("Content-Type: text/plain;charset=%s\r\n", charset);
      else
         rsprintf("Content-Type: application/octet-stream;charset=%s\r\n", charset);

      rsprintf("Content-Length: %d\r\n\r\n", length);

      /* increase return buffer size if file too big */
      if (length > return_buffer_size - (int) strlen(return_buffer)) {
         delta = length - (return_buffer_size - strlen(return_buffer)) + 1000;

         return_buffer = xrealloc(return_buffer, return_buffer_size + delta);
         memset(return_buffer + return_buffer_size, 0, delta);
         return_buffer_size += delta;
      }

      return_length = strlen(return_buffer) + length;
      read(fh, return_buffer + strlen(return_buffer), length);

      close(fh);
   } else {
      char encodedname[256];
      show_html_header(NULL, FALSE, "404 Not Found", TRUE, FALSE, NULL, FALSE, 0);

      rsprintf("<body><h1>Not Found</h1>\r\n");
      rsprintf("The requested file <b>");
      strencode2(encodedname, file_name, sizeof(encodedname));
      if (strchr(file_name, DIR_SEPARATOR))
         rsprintf("%s", encodedname);
      else
         rsprintf("%s%c%s", dir, DIR_SEPARATOR, encodedname);
      rsprintf("</b> was not found on this server<p>\r\n");
      rsprintf("<hr><address>ELOG version %s</address></body></html>\r\n\r\n", VERSION);
      return_length = strlen_retbuf;
      keep_alive = FALSE;
   }
}