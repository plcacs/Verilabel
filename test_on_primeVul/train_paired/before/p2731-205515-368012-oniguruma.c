search_in_range(regex_t* reg, const UChar* str, const UChar* end,
                const UChar* start, const UChar* range, /* match start range */
                const UChar* data_range, /* subject string range */
                OnigRegion* region,
                OnigOptionType option, OnigMatchParam* mp)
{
  int r;
  UChar *s, *prev;
  MatchArg msa;
  const UChar *orig_start = start;

#ifdef ONIG_DEBUG_SEARCH
  fprintf(stderr,
     "onig_search (entry point): str: %p, end: %d, start: %d, range: %d\n",
     str, (int )(end - str), (int )(start - str), (int )(range - str));
#endif

  ADJUST_MATCH_PARAM(reg, mp);

  if (region
#ifdef USE_POSIX_API_REGION_OPTION
      && !IS_POSIX_REGION(option)
#endif
      ) {
    r = onig_region_resize_clear(region, reg->num_mem + 1);
    if (r != 0) goto finish_no_msa;
  }

  if (start > end || start < str) goto mismatch_no_msa;

  if (ONIG_IS_OPTION_ON(option, ONIG_OPTION_CHECK_VALIDITY_OF_STRING)) {
    if (! ONIGENC_IS_VALID_MBC_STRING(reg->enc, str, end)) {
      r = ONIGERR_INVALID_WIDE_CHAR_VALUE;
      goto finish_no_msa;
    }
  }


#ifdef USE_FIND_LONGEST_SEARCH_ALL_OF_RANGE
#define MATCH_AND_RETURN_CHECK(upper_range) \
  r = match_at(reg, str, end, (upper_range), s, prev, &msa); \
  if (r != ONIG_MISMATCH) {\
    if (r >= 0) {\
      if (! IS_FIND_LONGEST(reg->options)) {\
        goto match;\
      }\
    }\
    else goto finish; /* error */ \
  }
#else
#define MATCH_AND_RETURN_CHECK(upper_range) \
  r = match_at(reg, str, end, (upper_range), s, prev, &msa); \
  if (r != ONIG_MISMATCH) {\
    if (r >= 0) {\
      goto match;\
    }\
    else goto finish; /* error */ \
  }
#endif /* USE_FIND_LONGEST_SEARCH_ALL_OF_RANGE */


  /* anchor optimize: resume search range */
  if (reg->anchor != 0 && str < end) {
    UChar *min_semi_end, *max_semi_end;

    if (reg->anchor & ANCR_BEGIN_POSITION) {
      /* search start-position only */
    begin_position:
      if (range > start)
        range = start + 1;
      else
        range = start;
    }
    else if (reg->anchor & ANCR_BEGIN_BUF) {
      /* search str-position only */
      if (range > start) {
        if (start != str) goto mismatch_no_msa;
        range = str + 1;
      }
      else {
        if (range <= str) {
          start = str;
          range = str;
        }
        else
          goto mismatch_no_msa;
      }
    }
    else if (reg->anchor & ANCR_END_BUF) {
      min_semi_end = max_semi_end = (UChar* )end;

    end_buf:
      if ((OnigLen )(max_semi_end - str) < reg->anchor_dmin)
        goto mismatch_no_msa;

      if (range > start) {
        if ((OnigLen )(min_semi_end - start) > reg->anchor_dmax) {
          start = min_semi_end - reg->anchor_dmax;
          if (start < end)
            start = onigenc_get_right_adjust_char_head(reg->enc, str, start);
        }
        if ((OnigLen )(max_semi_end - (range - 1)) < reg->anchor_dmin) {
          range = max_semi_end - reg->anchor_dmin + 1;
        }

        if (start > range) goto mismatch_no_msa;
        /* If start == range, match with empty at end.
           Backward search is used. */
      }
      else {
        if ((OnigLen )(min_semi_end - range) > reg->anchor_dmax) {
          range = min_semi_end - reg->anchor_dmax;
        }
        if ((OnigLen )(max_semi_end - start) < reg->anchor_dmin) {
          start = max_semi_end - reg->anchor_dmin;
          start = ONIGENC_LEFT_ADJUST_CHAR_HEAD(reg->enc, str, start);
        }
        if (range > start) goto mismatch_no_msa;
      }
    }
    else if (reg->anchor & ANCR_SEMI_END_BUF) {
      UChar* pre_end = ONIGENC_STEP_BACK(reg->enc, str, end, 1);

      max_semi_end = (UChar* )end;
      if (ONIGENC_IS_MBC_NEWLINE(reg->enc, pre_end, end)) {
        min_semi_end = pre_end;

#ifdef USE_CRNL_AS_LINE_TERMINATOR
        pre_end = ONIGENC_STEP_BACK(reg->enc, str, pre_end, 1);
        if (IS_NOT_NULL(pre_end) &&
            ONIGENC_IS_MBC_CRNL(reg->enc, pre_end, end)) {
          min_semi_end = pre_end;
        }
#endif
        if (min_semi_end > str && start <= min_semi_end) {
          goto end_buf;
        }
      }
      else {
        min_semi_end = (UChar* )end;
        goto end_buf;
      }
    }
    else if ((reg->anchor & ANCR_ANYCHAR_INF_ML)) {
      goto begin_position;
    }
  }
  else if (str == end) { /* empty string */
    static const UChar* address_for_empty_string = (UChar* )"";

#ifdef ONIG_DEBUG_SEARCH
    fprintf(stderr, "onig_search: empty string.\n");
#endif

    if (reg->threshold_len == 0) {
      start = end = str = address_for_empty_string;
      s = (UChar* )start;
      prev = (UChar* )NULL;

      MATCH_ARG_INIT(msa, reg, option, region, start, mp);
      MATCH_AND_RETURN_CHECK(end);
      goto mismatch;
    }
    goto mismatch_no_msa;
  }

#ifdef ONIG_DEBUG_SEARCH
  fprintf(stderr, "onig_search(apply anchor): end: %d, start: %d, range: %d\n",
          (int )(end - str), (int )(start - str), (int )(range - str));
#endif

  MATCH_ARG_INIT(msa, reg, option, region, orig_start, mp);

  s = (UChar* )start;
  if (range > start) {   /* forward search */
    if (s > str)
      prev = onigenc_get_prev_char_head(reg->enc, str, s);
    else
      prev = (UChar* )NULL;

    if (reg->optimize != OPTIMIZE_NONE) {
      UChar *sch_range, *low, *high, *low_prev;

      sch_range = (UChar* )range;
      if (reg->dist_max != 0) {
        if (reg->dist_max == INFINITE_LEN)
          sch_range = (UChar* )end;
        else {
          sch_range += reg->dist_max;
          if (sch_range > end) sch_range = (UChar* )end;
        }
      }

      if ((end - start) < reg->threshold_len)
        goto mismatch;

      if (reg->dist_max != INFINITE_LEN) {
        do {
          if (! forward_search(reg, str, end, s, sch_range, &low, &high,
                               &low_prev)) goto mismatch;
          if (s < low) {
            s    = low;
            prev = low_prev;
          }
          while (s <= high) {
            MATCH_AND_RETURN_CHECK(data_range);
            prev = s;
            s += enclen(reg->enc, s);
          }
        } while (s < range);
        goto mismatch;
      }
      else { /* check only. */
        if (! forward_search(reg, str, end, s, sch_range, &low, &high,
                             (UChar** )NULL)) goto mismatch;

        if ((reg->anchor & ANCR_ANYCHAR_INF) != 0) {
          do {
            MATCH_AND_RETURN_CHECK(data_range);
            prev = s;
            s += enclen(reg->enc, s);

            if ((reg->anchor & (ANCR_LOOK_BEHIND | ANCR_PREC_READ_NOT)) == 0) {
              while (!ONIGENC_IS_MBC_NEWLINE(reg->enc, prev, end) && s < range) {
                prev = s;
                s += enclen(reg->enc, s);
              }
            }
          } while (s < range);
          goto mismatch;
        }
      }
    }

    do {
      MATCH_AND_RETURN_CHECK(data_range);
      prev = s;
      s += enclen(reg->enc, s);
    } while (s < range);

    if (s == range) { /* because empty match with /$/. */
      MATCH_AND_RETURN_CHECK(data_range);
    }
  }
  else {  /* backward search */
    if (range < str) goto mismatch;

    if (orig_start < end)
      orig_start += enclen(reg->enc, orig_start); /* is upper range */

    if (reg->optimize != OPTIMIZE_NONE) {
      UChar *low, *high, *adjrange, *sch_start;
      const UChar *min_range;

      if ((end - range) < reg->threshold_len) goto mismatch;

      if (range < end)
        adjrange = ONIGENC_LEFT_ADJUST_CHAR_HEAD(reg->enc, str, range);
      else
        adjrange = (UChar* )end;

      if ((ptrdiff_t )(end - range) > (ptrdiff_t )reg->dist_min)
        min_range = range + reg->dist_min;
      else
        min_range = end;

      if (reg->dist_max != INFINITE_LEN) {
        do {
          sch_start = s + reg->dist_max;
          if (sch_start >= end)
            sch_start = onigenc_get_prev_char_head(reg->enc, str, end);

          if (backward_search(reg, str, end, sch_start, min_range, adjrange,
                              &low, &high) <= 0)
            goto mismatch;

          if (s > high)
            s = high;

          while (s >= low) {
            prev = onigenc_get_prev_char_head(reg->enc, str, s);
            MATCH_AND_RETURN_CHECK(orig_start);
            s = prev;
          }
        } while (s >= range);
        goto mismatch;
      }
      else { /* check only. */
        sch_start = s;
        if (reg->dist_max != 0) {
          if (reg->dist_max == INFINITE_LEN)
            sch_start = (UChar* )end;
          else {
            sch_start += reg->dist_max;
            if (sch_start >= end) sch_start = (UChar* )end;
            else
              sch_start = ONIGENC_LEFT_ADJUST_CHAR_HEAD(reg->enc,
                                                        start, sch_start);
          }
          if (sch_start >= end)
            sch_start = onigenc_get_prev_char_head(reg->enc, str, end);
        }
        if (backward_search(reg, str, end, sch_start, min_range, adjrange,
                            &low, &high) <= 0) goto mismatch;
      }
    }

    do {
      prev = onigenc_get_prev_char_head(reg->enc, str, s);
      MATCH_AND_RETURN_CHECK(orig_start);
      s = prev;
    } while (s >= range);
  }

 mismatch:
#ifdef USE_FIND_LONGEST_SEARCH_ALL_OF_RANGE
  if (IS_FIND_LONGEST(reg->options)) {
    if (msa.best_len >= 0) {
      s = msa.best_s;
      goto match;
    }
  }
#endif
  r = ONIG_MISMATCH;

 finish:
  MATCH_ARG_FREE(msa);

  /* If result is mismatch and no FIND_NOT_EMPTY option,
     then the region is not set in match_at(). */
  if (IS_FIND_NOT_EMPTY(reg->options) && region
#ifdef USE_POSIX_API_REGION_OPTION
      && !IS_POSIX_REGION(option)
#endif
      ) {
    onig_region_clear(region);
  }

#ifdef ONIG_DEBUG
  if (r != ONIG_MISMATCH)
    fprintf(stderr, "onig_search: error %d\n", r);
#endif
  return r;

 mismatch_no_msa:
  r = ONIG_MISMATCH;
 finish_no_msa:
#ifdef ONIG_DEBUG
  if (r != ONIG_MISMATCH)
    fprintf(stderr, "onig_search: error %d\n", r);
#endif
  return r;

 match:
  MATCH_ARG_FREE(msa);
  return (int )(s - str);
}