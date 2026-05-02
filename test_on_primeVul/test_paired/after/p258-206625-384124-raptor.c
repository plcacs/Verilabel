raptor_xml_writer_start_element_common(raptor_xml_writer* xml_writer,
                                       raptor_xml_element* element,
                                       int auto_empty)
{
  raptor_iostream* iostr = xml_writer->iostr;
  raptor_namespace_stack *nstack = xml_writer->nstack;
  int depth = xml_writer->depth;
  int auto_indent = XML_WRITER_AUTO_INDENT(xml_writer);
  struct nsd *nspace_declarations = NULL;
  size_t nspace_declarations_count = 0;  
  unsigned int i;

  if(nstack) {
    int nspace_max_count = element->attribute_count * 2; /* attr and value */
    if(element->name->nspace)
      nspace_max_count++;
    if(element->declared_nspaces)
      nspace_max_count += raptor_sequence_size(element->declared_nspaces);
    if(element->xml_language)
      nspace_max_count++;

    nspace_declarations = RAPTOR_CALLOC(struct nsd*, nspace_max_count,
                                        sizeof(struct nsd));
    if(!nspace_declarations)
      return 1;
  }

  if(element->name->nspace) {
    if(nstack && !raptor_namespaces_namespace_in_scope(nstack, element->name->nspace)) {
      nspace_declarations[0].declaration=
        raptor_namespace_format_as_xml(element->name->nspace,
                                       &nspace_declarations[0].length);
      if(!nspace_declarations[0].declaration)
        goto error;
      nspace_declarations[0].nspace = element->name->nspace;
      nspace_declarations_count++;
    }
  }

  if(nstack && element->attributes) {
    for(i = 0; i < element->attribute_count; i++) {
      /* qname */
      if(element->attributes[i]->nspace) {
        /* Check if we need a namespace declaration attribute */
        if(nstack && 
           !raptor_namespaces_namespace_in_scope(nstack, element->attributes[i]->nspace) && element->attributes[i]->nspace != element->name->nspace) {
          /* not in scope and not same as element (so already going to be declared)*/
          unsigned int j;
          int declare_me = 1;
          
          /* check it wasn't an earlier declaration too */
          for(j = 0; j < nspace_declarations_count; j++)
            if(nspace_declarations[j].nspace == element->attributes[j]->nspace) {
              declare_me = 0;
              break;
            }
            
          if(declare_me) {
            nspace_declarations[nspace_declarations_count].declaration=
              raptor_namespace_format_as_xml(element->attributes[i]->nspace,
                                             &nspace_declarations[nspace_declarations_count].length);
            if(!nspace_declarations[nspace_declarations_count].declaration)
              goto error;
            nspace_declarations[nspace_declarations_count].nspace = element->attributes[i]->nspace;
            nspace_declarations_count++;
          }
        }
      }

      /* Add the attribute's value */
      nspace_declarations[nspace_declarations_count].declaration=
        raptor_qname_format_as_xml(element->attributes[i],
                                   &nspace_declarations[nspace_declarations_count].length);
      if(!nspace_declarations[nspace_declarations_count].declaration)
        goto error;
      nspace_declarations[nspace_declarations_count].nspace = NULL;
      nspace_declarations_count++;

    }
  }

  if(nstack && element->declared_nspaces &&
     raptor_sequence_size(element->declared_nspaces) > 0) {
    for(i = 0; i< (unsigned int)raptor_sequence_size(element->declared_nspaces); i++) {
      raptor_namespace* nspace = (raptor_namespace*)raptor_sequence_get_at(element->declared_nspaces, i);
      unsigned int j;
      int declare_me = 1;
      
      /* check it wasn't an earlier declaration too */
      for(j = 0; j < nspace_declarations_count; j++)
        if(nspace_declarations[j].nspace == nspace) {
          declare_me = 0;
          break;
        }
      
      if(declare_me) {
        nspace_declarations[nspace_declarations_count].declaration=
          raptor_namespace_format_as_xml(nspace,
                                         &nspace_declarations[nspace_declarations_count].length);
        if(!nspace_declarations[nspace_declarations_count].declaration)
          goto error;
        nspace_declarations[nspace_declarations_count].nspace = nspace;
        nspace_declarations_count++;
      }

    }
  }

  if(nstack && element->xml_language) {
    size_t lang_len = strlen(RAPTOR_GOOD_CAST(char*, element->xml_language));
#define XML_LANG_PREFIX_LEN 10
    size_t buf_length = XML_LANG_PREFIX_LEN + lang_len + 1;
    unsigned char* buffer = RAPTOR_MALLOC(unsigned char*, buf_length + 1);
    const char quote = '\"';
    unsigned char* p;

    memcpy(buffer, "xml:lang=\"", XML_LANG_PREFIX_LEN);
    p = buffer + XML_LANG_PREFIX_LEN;
    p += raptor_xml_escape_string(xml_writer->world,
                                  element->xml_language, lang_len,
                                  p, buf_length, quote);
    *p++ = quote;
    *p = '\0';

    nspace_declarations[nspace_declarations_count].declaration = buffer;
    nspace_declarations[nspace_declarations_count].length = buf_length;
    nspace_declarations[nspace_declarations_count].nspace = NULL;
    nspace_declarations_count++;
  }
  

  raptor_iostream_write_byte('<', iostr);

  if(element->name->nspace && element->name->nspace->prefix_length > 0) {
    raptor_iostream_counted_string_write((const char*)element->name->nspace->prefix, 
                                         element->name->nspace->prefix_length,
                                         iostr);
    raptor_iostream_write_byte(':', iostr);
  }
  raptor_iostream_counted_string_write((const char*)element->name->local_name,
                                       element->name->local_name_length,
                                       iostr);

  /* declare namespaces and attributes */
  if(nspace_declarations_count) {
    int need_indent = 0;
    
    /* sort them into the canonical order */
    qsort((void*)nspace_declarations, 
          nspace_declarations_count, sizeof(struct nsd),
          raptor_xml_writer_nsd_compare);

    /* declare namespaces first */
    for(i = 0; i < nspace_declarations_count; i++) {
      if(!nspace_declarations[i].nspace)
        continue;

      if(auto_indent && need_indent) {
        /* indent attributes */
        raptor_xml_writer_newline(xml_writer);
        xml_writer->depth++;
        raptor_xml_writer_indent(xml_writer);
        xml_writer->depth--;
      }
      raptor_iostream_write_byte(' ', iostr);
      raptor_iostream_counted_string_write((const char*)nspace_declarations[i].declaration,
                                           nspace_declarations[i].length,
                                           iostr);
      RAPTOR_FREE(char*, nspace_declarations[i].declaration);
      nspace_declarations[i].declaration = NULL;
      need_indent = 1;
      
      if(raptor_namespace_stack_start_namespace(nstack,
                                                (raptor_namespace*)nspace_declarations[i].nspace,
                                                depth))
        goto error;
    }

    /* declare attributes */
    for(i = 0; i < nspace_declarations_count; i++) {
      if(nspace_declarations[i].nspace)
        continue;

      if(auto_indent && need_indent) {
        /* indent attributes */
        raptor_xml_writer_newline(xml_writer);
        xml_writer->depth++;
        raptor_xml_writer_indent(xml_writer);
        xml_writer->depth--;
      }
      raptor_iostream_write_byte(' ', iostr);
      raptor_iostream_counted_string_write((const char*)nspace_declarations[i].declaration,
                                           nspace_declarations[i].length,
                                           iostr);
      need_indent = 1;

      RAPTOR_FREE(char*, nspace_declarations[i].declaration);
      nspace_declarations[i].declaration = NULL;
    }
  }

  if(!auto_empty)
    raptor_iostream_write_byte('>', iostr);

  if(nstack)
    RAPTOR_FREE(stringarray, nspace_declarations);

  return 0;

  /* Clean up nspace_declarations on error */
  error:

  for(i = 0; i < nspace_declarations_count; i++) {
    if(nspace_declarations[i].declaration)
      RAPTOR_FREE(char*, nspace_declarations[i].declaration);
  }

  RAPTOR_FREE(stringarray, nspace_declarations);

  return 1;
}