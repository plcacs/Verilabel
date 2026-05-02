g_markup_parse_context_end_parse (GMarkupParseContext  *context,
                                  GError              **error)
{
  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (!context->parsing, FALSE);
  g_return_val_if_fail (context->state != STATE_ERROR, FALSE);

  if (context->partial_chunk != NULL)
    {
      g_string_free (context->partial_chunk, TRUE);
      context->partial_chunk = NULL;
    }

  if (context->document_empty)
    {
      set_error_literal (context, error, G_MARKUP_ERROR_EMPTY,
                         _("Document was empty or contained only whitespace"));
      return FALSE;
    }

  context->parsing = TRUE;

  switch (context->state)
    {
    case STATE_START:
      /* Nothing to do */
      break;

    case STATE_AFTER_OPEN_ANGLE:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly just after an open angle bracket “<”"));
      break;

    case STATE_AFTER_CLOSE_ANGLE:
      if (context->tag_stack != NULL)
        {
          /* Error message the same as for INSIDE_TEXT */
          set_error (context, error, G_MARKUP_ERROR_PARSE,
                     _("Document ended unexpectedly with elements still open — "
                       "“%s” was the last element opened"),
                     current_element (context));
        }
      break;

    case STATE_AFTER_ELISION_SLASH:
      set_error (context, error, G_MARKUP_ERROR_PARSE,
                 _("Document ended unexpectedly, expected to see a close angle "
                   "bracket ending the tag <%s/>"), current_element (context));
      break;

    case STATE_INSIDE_OPEN_TAG_NAME:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly inside an element name"));
      break;

    case STATE_INSIDE_ATTRIBUTE_NAME:
    case STATE_AFTER_ATTRIBUTE_NAME:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly inside an attribute name"));
      break;

    case STATE_BETWEEN_ATTRIBUTES:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly inside an element-opening "
                           "tag."));
      break;

    case STATE_AFTER_ATTRIBUTE_EQUALS_SIGN:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly after the equals sign "
                           "following an attribute name; no attribute value"));
      break;

    case STATE_INSIDE_ATTRIBUTE_VALUE_SQ:
    case STATE_INSIDE_ATTRIBUTE_VALUE_DQ:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly while inside an attribute "
                           "value"));
      break;

    case STATE_INSIDE_TEXT:
      g_assert (context->tag_stack != NULL);
      set_error (context, error, G_MARKUP_ERROR_PARSE,
                 _("Document ended unexpectedly with elements still open — "
                   "“%s” was the last element opened"),
                 current_element (context));
      break;

    case STATE_AFTER_CLOSE_TAG_SLASH:
    case STATE_INSIDE_CLOSE_TAG_NAME:
    case STATE_AFTER_CLOSE_TAG_NAME:
      if (context->tag_stack != NULL)
        set_error (context, error, G_MARKUP_ERROR_PARSE,
                   _("Document ended unexpectedly inside the close tag for "
                     "element “%s”"), current_element (context));
      else
        set_error (context, error, G_MARKUP_ERROR_PARSE,
                   _("Document ended unexpectedly inside the close tag for an "
                     "unopened element"));
      break;

    case STATE_INSIDE_PASSTHROUGH:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly inside a comment or "
                           "processing instruction"));
      break;

    case STATE_ERROR:
    default:
      g_assert_not_reached ();
      break;
    }

  context->parsing = FALSE;

  return context->state != STATE_ERROR;
}