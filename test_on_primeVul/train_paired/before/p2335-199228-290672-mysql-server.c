static bool add_line(String &buffer,char *line,char *in_string,
                     bool *ml_comment, bool truncated)
{
  uchar inchar;
  char buff[80], *pos, *out;
  COMMANDS *com;
  bool need_space= 0;
  bool ss_comment= 0;
  DBUG_ENTER("add_line");

  if (!line[0] && buffer.is_empty())
    DBUG_RETURN(0);
#ifdef HAVE_READLINE
  if (status.add_to_history && line[0] && not_in_history(line))
    add_history(line);
#endif
  char *end_of_line=line+(uint) strlen(line);

  for (pos=out=line ; (inchar= (uchar) *pos) ; pos++)
  {
    if (!preserve_comments)
    {
      // Skip spaces at the beginning of a statement
      if (my_isspace(charset_info,inchar) && (out == line) &&
          buffer.is_empty())
        continue;
    }
        
#ifdef USE_MB
    // Accept multi-byte characters as-is
    int length;
    if (use_mb(charset_info) &&
        (length= my_ismbchar(charset_info, pos, end_of_line)))
    {
      if (!*ml_comment || preserve_comments)
      {
        while (length--)
          *out++ = *pos++;
        pos--;
      }
      else
        pos+= length - 1;
      continue;
    }
#endif
    if (!*ml_comment && inchar == '\\' &&
        !(*in_string && 
          (mysql.server_status & SERVER_STATUS_NO_BACKSLASH_ESCAPES)))
    {
      // Found possbile one character command like \c

      if (!(inchar = (uchar) *++pos))
	break;				// readline adds one '\'
      if (*in_string || inchar == 'N')	// \N is short for NULL
      {					// Don't allow commands in string
	*out++='\\';
	*out++= (char) inchar;
	continue;
      }
      if ((com=find_command(NullS,(char) inchar)))
      {
        // Flush previously accepted characters
        if (out != line)
        {
          buffer.append(line, (uint) (out-line));
          out= line;
        }
        
        if ((*com->func)(&buffer,pos-1) > 0)
          DBUG_RETURN(1);                       // Quit
        if (com->takes_params)
        {
          if (ss_comment)
          {
            /*
              If a client-side macro appears inside a server-side comment,
              discard all characters in the comment after the macro (that is,
              until the end of the comment rather than the next delimiter)
            */
            for (pos++; *pos && (*pos != '*' || *(pos + 1) != '/'); pos++)
              ;
            pos--;
          }
          else
          {
            for (pos++ ;
                 *pos && (*pos != *delimiter ||
                          !is_prefix(pos + 1, delimiter + 1)) ; pos++)
              ;	// Remove parameters
            if (!*pos)
              pos--;
            else 
              pos+= delimiter_length - 1; // Point at last delim char
          }
        }
      }
      else
      {
	sprintf(buff,"Unknown command '\\%c'.",inchar);
	if (put_info(buff,INFO_ERROR) > 0)
	  DBUG_RETURN(1);
	*out++='\\';
	*out++=(char) inchar;
	continue;
      }
    }
    else if (!*ml_comment && !*in_string && is_prefix(pos, delimiter))
    {
      // Found a statement. Continue parsing after the delimiter
      pos+= delimiter_length;

      if (preserve_comments)
      {
        while (my_isspace(charset_info, *pos))
          *out++= *pos++;
      }
      // Flush previously accepted characters
      if (out != line)
      {
        buffer.append(line, (uint32) (out-line));
        out= line;
      }

      if (preserve_comments && ((*pos == '#') ||
                                ((*pos == '-') &&
                                 (pos[1] == '-') &&
                                 my_isspace(charset_info, pos[2]))))
      {
        // Add trailing single line comments to this statement
        buffer.append(pos);
        pos+= strlen(pos);
      }

      pos--;

      if ((com= find_command(buffer.c_ptr(), 0)))
      {
          
        if ((*com->func)(&buffer, buffer.c_ptr()) > 0)
          DBUG_RETURN(1);                       // Quit 
      }
      else
      {
        if (com_go(&buffer, 0) > 0)             // < 0 is not fatal
          DBUG_RETURN(1);
      }
      buffer.length(0);
    }
    else if (!*ml_comment && (!*in_string && (inchar == '#' ||
                                              (inchar == '-' && pos[1] == '-' &&
                              /*
                                The third byte is either whitespace or is the
                                end of the line -- which would occur only
                                because of the user sending newline -- which is
                                itself whitespace and should also match.
                              */
			      (my_isspace(charset_info,pos[2]) ||
                               !pos[2])))))
    {
      // Flush previously accepted characters
      if (out != line)
      {
        buffer.append(line, (uint32) (out - line));
        out= line;
      }

      // comment to end of line
      if (preserve_comments)
      {
        bool started_with_nothing= !buffer.length();

        buffer.append(pos);

        /*
          A single-line comment by itself gets sent immediately so that
          client commands (delimiter, status, etc) will be interpreted on
          the next line.
        */
        if (started_with_nothing)
        {
          if (com_go(&buffer, 0) > 0)             // < 0 is not fatal
            DBUG_RETURN(1);
          buffer.length(0);
        }
      }

      break;
    }
    else if (!*in_string && inchar == '/' && *(pos+1) == '*' &&
	     *(pos+2) != '!')
    {
      if (preserve_comments)
      {
        *out++= *pos++;                       // copy '/'
        *out++= *pos;                         // copy '*'
      }
      else
        pos++;
      *ml_comment= 1;
      if (out != line)
      {
        buffer.append(line,(uint) (out-line));
        out=line;
      }
    }
    else if (*ml_comment && !ss_comment && inchar == '*' && *(pos + 1) == '/')
    {
      if (preserve_comments)
      {
        *out++= *pos++;                       // copy '*'
        *out++= *pos;                         // copy '/'
      }
      else
        pos++;
      *ml_comment= 0;
      if (out != line)
      {
        buffer.append(line, (uint32) (out - line));
        out= line;
      }
      // Consumed a 2 chars or more, and will add 1 at most,
      // so using the 'line' buffer to edit data in place is ok.
      need_space= 1;
    }      
    else
    {						// Add found char to buffer
      if (!*in_string && inchar == '/' && *(pos + 1) == '*' &&
          *(pos + 2) == '!')
        ss_comment= 1;
      else if (!*in_string && ss_comment && inchar == '*' && *(pos + 1) == '/')
        ss_comment= 0;
      if (inchar == *in_string)
	*in_string= 0;
      else if (!*ml_comment && !*in_string &&
	       (inchar == '\'' || inchar == '"' || inchar == '`'))
	*in_string= (char) inchar;
      if (!*ml_comment || preserve_comments)
      {
        if (need_space && !my_isspace(charset_info, (char)inchar))
          *out++= ' ';
        need_space= 0;
        *out++= (char) inchar;
      }
    }
  }
  if (out != line || !buffer.is_empty())
  {
    uint length=(uint) (out-line);

    if (!truncated && (length < 9 ||
                       my_strnncoll (charset_info, (uchar *)line, 9,
                                     (const uchar *) "delimiter", 9) ||
                       (*in_string || *ml_comment)))
    {
      /* 
        Don't add a new line in case there's a DELIMITER command to be 
        added to the glob buffer (e.g. on processing a line like 
        "<command>;DELIMITER <non-eof>") : similar to how a new line is 
        not added in the case when the DELIMITER is the first command 
        entered with an empty glob buffer. However, if the delimiter is
        part of a string or a comment, the new line should be added. (e.g.
        SELECT '\ndelimiter\n';\n)
      */
      *out++='\n';
      length++;
    }
    if (buffer.length() + length >= buffer.alloced_length())
      buffer.realloc(buffer.length()+length+IO_SIZE);
    if ((!*ml_comment || preserve_comments) && buffer.append(line, length))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}