xml_element* xml_elem_parse_buf(const char* in_buf, int len, XML_ELEM_INPUT_OPTIONS options, XML_ELEM_ERROR error)
{
   xml_element* xReturn = NULL;
   char buf[100] = "";
   static STRUCT_XML_ELEM_INPUT_OPTIONS default_opts = {encoding_utf_8};

   if(!options) {
      options = &default_opts;
   }

   if(in_buf) {
      XML_Parser parser;
      xml_elem_data mydata = {0};

      parser = XML_ParserCreate(NULL);

      mydata.root = xml_elem_new();
      mydata.current = mydata.root;
      mydata.input_options = options;
      mydata.needs_enc_conversion = options->encoding && strcmp(options->encoding, encoding_utf_8);

      XML_SetElementHandler(parser, (XML_StartElementHandler)_xmlrpc_startElement, (XML_EndElementHandler)_xmlrpc_endElement);
      XML_SetCharacterDataHandler(parser, (XML_CharacterDataHandler)_xmlrpc_charHandler);

      /* pass the xml_elem_data struct along */
      XML_SetUserData(parser, (void*)&mydata);

      if(!len) {
         len = strlen(in_buf);
      }

      /* parse the XML */
      if(XML_Parse(parser, in_buf, len, 1) == 0) {
         enum XML_Error err_code = XML_GetErrorCode(parser);
         int line_num = XML_GetCurrentLineNumber(parser);
         int col_num = XML_GetCurrentColumnNumber(parser);
         long byte_idx = XML_GetCurrentByteIndex(parser);
         int byte_total = XML_GetCurrentByteCount(parser);
         const char * error_str = XML_ErrorString(err_code);
         if(byte_idx >= 0) {
             snprintf(buf, 
                      sizeof(buf),
                      "\n\tdata beginning %ld before byte index: %s\n",
                      byte_idx > 10  ? 10 : byte_idx,
                      in_buf + (byte_idx > 10 ? byte_idx - 10 : byte_idx));
         }

         fprintf(stderr, "expat reports error code %i\n"
                "\tdescription: %s\n"
                "\tline: %i\n"
                "\tcolumn: %i\n"
                "\tbyte index: %ld\n"
                "\ttotal bytes: %i\n%s ",
                err_code, error_str, line_num, 
                col_num, byte_idx, byte_total, buf);


          /* error condition */
          if(error) {
              error->parser_code = (long)err_code;
              error->line = line_num;
              error->column = col_num;
              error->byte_index = byte_idx;
              error->parser_error = error_str;
          }
      }
      else {
         xReturn = (xml_element*)Q_Head(&mydata.root->children);
         xReturn->parent = NULL;
      }

      XML_ParserFree(parser);


      xml_elem_free_non_recurse(mydata.root);
   }

   return xReturn;
}