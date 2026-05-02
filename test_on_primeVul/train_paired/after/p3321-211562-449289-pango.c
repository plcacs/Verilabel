pango_log2vis_get_embedding_levels (const gchar    *text,
				    int             length,
				    PangoDirection *pbase_dir)
{
  glong n_chars, i;
  guint8 *embedding_levels_list;
  const gchar *p;
  FriBidiParType fribidi_base_dir;
  FriBidiCharType *bidi_types;
#ifdef USE_FRIBIDI_EX_API
  FriBidiBracketType *bracket_types;
#endif
  FriBidiLevel max_level;
  FriBidiCharType ored_types = 0;
  FriBidiCharType anded_strongs = FRIBIDI_TYPE_RLE;

  G_STATIC_ASSERT (sizeof (FriBidiLevel) == sizeof (guint8));
  G_STATIC_ASSERT (sizeof (FriBidiChar) == sizeof (gunichar));

  switch (*pbase_dir)
    {
    case PANGO_DIRECTION_LTR:
    case PANGO_DIRECTION_TTB_RTL:
      fribidi_base_dir = FRIBIDI_PAR_LTR;
      break;
    case PANGO_DIRECTION_RTL:
    case PANGO_DIRECTION_TTB_LTR:
      fribidi_base_dir = FRIBIDI_PAR_RTL;
      break;
    case PANGO_DIRECTION_WEAK_RTL:
      fribidi_base_dir = FRIBIDI_PAR_WRTL;
      break;
    case PANGO_DIRECTION_WEAK_LTR:
    case PANGO_DIRECTION_NEUTRAL:
    default:
      fribidi_base_dir = FRIBIDI_PAR_WLTR;
      break;
    }

  if (length < 0)
    length = strlen (text);

  n_chars = g_utf8_strlen (text, length);

  bidi_types = g_new (FriBidiCharType, n_chars);
#ifdef USE_FRIBIDI_EX_API
  bracket_types = g_new (FriBidiBracketType, n_chars);
#endif
  embedding_levels_list = g_new (guint8, n_chars);

  for (i = 0, p = text; p < text + length; p = g_utf8_next_char(p), i++)
    {
      gunichar ch = g_utf8_get_char (p);
      FriBidiCharType char_type = fribidi_get_bidi_type (ch);

      if (i == n_chars)
        break;

      bidi_types[i] = char_type;
      ored_types |= char_type;
      if (FRIBIDI_IS_STRONG (char_type))
        anded_strongs &= char_type;
#ifdef USE_FRIBIDI_EX_API
      if (G_UNLIKELY(bidi_types[i] == FRIBIDI_TYPE_ON))
        bracket_types[i] = fribidi_get_bracket (ch);
      else
        bracket_types[i] = FRIBIDI_NO_BRACKET;
#endif
    }

    /* Short-circuit (malloc-expensive) FriBidi call for unidirectional
     * text.
     *
     * For details see:
     * https://bugzilla.gnome.org/show_bug.cgi?id=590183
     */

#ifndef FRIBIDI_IS_ISOLATE
#define FRIBIDI_IS_ISOLATE(x) 0
#endif
    /* The case that all resolved levels will be ltr.
     * No isolates, all strongs be LTR, there should be no Arabic numbers
     * (or letters for that matter), and one of the following:
     *
     * o base_dir doesn't have an RTL taste.
     * o there are letters, and base_dir is weak.
     */
    if (!FRIBIDI_IS_ISOLATE (ored_types) &&
	!FRIBIDI_IS_RTL (ored_types) &&
	!FRIBIDI_IS_ARABIC (ored_types) &&
	(!FRIBIDI_IS_RTL (fribidi_base_dir) ||
	  (FRIBIDI_IS_WEAK (fribidi_base_dir) &&
	   FRIBIDI_IS_LETTER (ored_types))
	))
      {
        /* all LTR */
	fribidi_base_dir = FRIBIDI_PAR_LTR;
	memset (embedding_levels_list, 0, n_chars);
	goto resolved;
      }
    /* The case that all resolved levels will be RTL is much more complex.
     * No isolates, no numbers, all strongs are RTL, and one of
     * the following:
     *
     * o base_dir has an RTL taste (may be weak).
     * o there are letters, and base_dir is weak.
     */
    else if (!FRIBIDI_IS_ISOLATE (ored_types) &&
	     !FRIBIDI_IS_NUMBER (ored_types) &&
	     FRIBIDI_IS_RTL (anded_strongs) &&
	     (FRIBIDI_IS_RTL (fribidi_base_dir) ||
	       (FRIBIDI_IS_WEAK (fribidi_base_dir) &&
		FRIBIDI_IS_LETTER (ored_types))
	     ))
      {
        /* all RTL */
	fribidi_base_dir = FRIBIDI_PAR_RTL;
	memset (embedding_levels_list, 1, n_chars);
	goto resolved;
      }


#ifdef USE_FRIBIDI_EX_API
  max_level = fribidi_get_par_embedding_levels_ex (bidi_types, bracket_types, n_chars,
						   &fribidi_base_dir,
						   (FriBidiLevel*)embedding_levels_list);
#else
  max_level = fribidi_get_par_embedding_levels (bidi_types, n_chars,
						&fribidi_base_dir,
						(FriBidiLevel*)embedding_levels_list);
#endif

  if (G_UNLIKELY(max_level == 0))
    {
      /* fribidi_get_par_embedding_levels() failed. */
      memset (embedding_levels_list, 0, length);
    }

resolved:
  g_free (bidi_types);

#ifdef USE_FRIBIDI_EX_API
  g_free (bracket_types);
#endif

  *pbase_dir = (fribidi_base_dir == FRIBIDI_PAR_LTR) ?  PANGO_DIRECTION_LTR : PANGO_DIRECTION_RTL;

  return embedding_levels_list;
}