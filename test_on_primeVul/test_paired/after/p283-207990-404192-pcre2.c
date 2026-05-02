static int get_recurse_data_length(compiler_common *common, PCRE2_SPTR cc, PCRE2_SPTR ccend,
  BOOL *needs_control_head, BOOL *has_quit, BOOL *has_accept)
{
int length = 1;
int size, offset;
PCRE2_SPTR alternative;
BOOL quit_found = FALSE;
BOOL accept_found = FALSE;
BOOL setsom_found = FALSE;
BOOL setmark_found = FALSE;
BOOL control_head_found = FALSE;

memset(common->recurse_bitset, 0, common->recurse_bitset_size);

#if defined DEBUG_FORCE_CONTROL_HEAD && DEBUG_FORCE_CONTROL_HEAD
SLJIT_ASSERT(common->control_head_ptr != 0);
control_head_found = TRUE;
#endif

/* Calculate the sum of the private machine words. */
while (cc < ccend)
  {
  size = 0;
  switch(*cc)
    {
    case OP_SET_SOM:
    SLJIT_ASSERT(common->has_set_som);
    setsom_found = TRUE;
    cc += 1;
    break;

    case OP_RECURSE:
    if (common->has_set_som)
      setsom_found = TRUE;
    if (common->mark_ptr != 0)
      setmark_found = TRUE;
    if (common->capture_last_ptr != 0 && recurse_check_bit(common, common->capture_last_ptr))
      length++;
    cc += 1 + LINK_SIZE;
    break;

    case OP_KET:
    offset = PRIVATE_DATA(cc);
    if (offset != 0)
      {
      if (recurse_check_bit(common, offset))
        length++;
      SLJIT_ASSERT(PRIVATE_DATA(cc + 1) != 0);
      cc += PRIVATE_DATA(cc + 1);
      }
    cc += 1 + LINK_SIZE;
    break;

    case OP_ASSERT:
    case OP_ASSERT_NOT:
    case OP_ASSERTBACK:
    case OP_ASSERTBACK_NOT:
    case OP_ASSERT_NA:
    case OP_ASSERTBACK_NA:
    case OP_ONCE:
    case OP_SCRIPT_RUN:
    case OP_BRAPOS:
    case OP_SBRA:
    case OP_SBRAPOS:
    case OP_SCOND:
    SLJIT_ASSERT(PRIVATE_DATA(cc) != 0);
    if (recurse_check_bit(common, PRIVATE_DATA(cc)))
      length++;
    cc += 1 + LINK_SIZE;
    break;

    case OP_CBRA:
    case OP_SCBRA:
    offset = GET2(cc, 1 + LINK_SIZE);
    if (recurse_check_bit(common, OVECTOR(offset << 1)))
      {
      SLJIT_ASSERT(recurse_check_bit(common, OVECTOR((offset << 1) + 1)));
      length += 2;
      }
    if (common->optimized_cbracket[offset] == 0 && recurse_check_bit(common, OVECTOR_PRIV(offset)))
      length++;
    if (common->capture_last_ptr != 0 && recurse_check_bit(common, common->capture_last_ptr))
      length++;
    cc += 1 + LINK_SIZE + IMM2_SIZE;
    break;

    case OP_CBRAPOS:
    case OP_SCBRAPOS:
    offset = GET2(cc, 1 + LINK_SIZE);
    if (recurse_check_bit(common, OVECTOR(offset << 1)))
      {
      SLJIT_ASSERT(recurse_check_bit(common, OVECTOR((offset << 1) + 1)));
      length += 2;
      }
    if (recurse_check_bit(common, OVECTOR_PRIV(offset)))
      length++;
    if (recurse_check_bit(common, PRIVATE_DATA(cc)))
      length++;
    if (common->capture_last_ptr != 0 && recurse_check_bit(common, common->capture_last_ptr))
      length++;
    cc += 1 + LINK_SIZE + IMM2_SIZE;
    break;

    case OP_COND:
    /* Might be a hidden SCOND. */
    alternative = cc + GET(cc, 1);
    if ((*alternative == OP_KETRMAX || *alternative == OP_KETRMIN) && recurse_check_bit(common, PRIVATE_DATA(cc)))
      length++;
    cc += 1 + LINK_SIZE;
    break;

    CASE_ITERATOR_PRIVATE_DATA_1
    offset = PRIVATE_DATA(cc);
    if (offset != 0 && recurse_check_bit(common, offset))
      length++;
    cc += 2;
#ifdef SUPPORT_UNICODE
    if (common->utf && HAS_EXTRALEN(cc[-1])) cc += GET_EXTRALEN(cc[-1]);
#endif
    break;

    CASE_ITERATOR_PRIVATE_DATA_2A
    offset = PRIVATE_DATA(cc);
    if (offset != 0 && recurse_check_bit(common, offset))
      {
      SLJIT_ASSERT(recurse_check_bit(common, offset + sizeof(sljit_sw)));
      length += 2;
      }
    cc += 2;
#ifdef SUPPORT_UNICODE
    if (common->utf && HAS_EXTRALEN(cc[-1])) cc += GET_EXTRALEN(cc[-1]);
#endif
    break;

    CASE_ITERATOR_PRIVATE_DATA_2B
    offset = PRIVATE_DATA(cc);
    if (offset != 0 && recurse_check_bit(common, offset))
      {
      SLJIT_ASSERT(recurse_check_bit(common, offset + sizeof(sljit_sw)));
      length += 2;
      }
    cc += 2 + IMM2_SIZE;
#ifdef SUPPORT_UNICODE
    if (common->utf && HAS_EXTRALEN(cc[-1])) cc += GET_EXTRALEN(cc[-1]);
#endif
    break;

    CASE_ITERATOR_TYPE_PRIVATE_DATA_1
    offset = PRIVATE_DATA(cc);
    if (offset != 0 && recurse_check_bit(common, offset))
      length++;
    cc += 1;
    break;

    CASE_ITERATOR_TYPE_PRIVATE_DATA_2A
    offset = PRIVATE_DATA(cc);
    if (offset != 0 && recurse_check_bit(common, offset))
      {
      SLJIT_ASSERT(recurse_check_bit(common, offset + sizeof(sljit_sw)));
      length += 2;
      }
    cc += 1;
    break;

    CASE_ITERATOR_TYPE_PRIVATE_DATA_2B
    offset = PRIVATE_DATA(cc);
    if (offset != 0 && recurse_check_bit(common, offset))
      {
      SLJIT_ASSERT(recurse_check_bit(common, offset + sizeof(sljit_sw)));
      length += 2;
      }
    cc += 1 + IMM2_SIZE;
    break;

    case OP_CLASS:
    case OP_NCLASS:
#if defined SUPPORT_UNICODE || PCRE2_CODE_UNIT_WIDTH != 8
    case OP_XCLASS:
    size = (*cc == OP_XCLASS) ? GET(cc, 1) : 1 + 32 / (int)sizeof(PCRE2_UCHAR);
#else
    size = 1 + 32 / (int)sizeof(PCRE2_UCHAR);
#endif

    offset = PRIVATE_DATA(cc);
    if (offset != 0 && recurse_check_bit(common, offset))
      length += get_class_iterator_size(cc + size);
    cc += size;
    break;

    case OP_MARK:
    case OP_COMMIT_ARG:
    case OP_PRUNE_ARG:
    case OP_THEN_ARG:
    SLJIT_ASSERT(common->mark_ptr != 0);
    if (!setmark_found)
      setmark_found = TRUE;
    if (common->control_head_ptr != 0)
      control_head_found = TRUE;
    if (*cc != OP_MARK)
      quit_found = TRUE;

    cc += 1 + 2 + cc[1];
    break;

    case OP_PRUNE:
    case OP_SKIP:
    case OP_COMMIT:
    quit_found = TRUE;
    cc++;
    break;

    case OP_SKIP_ARG:
    quit_found = TRUE;
    cc += 1 + 2 + cc[1];
    break;

    case OP_THEN:
    SLJIT_ASSERT(common->control_head_ptr != 0);
    quit_found = TRUE;
    control_head_found = TRUE;
    cc++;
    break;

    case OP_ACCEPT:
    case OP_ASSERT_ACCEPT:
    accept_found = TRUE;
    cc++;
    break;

    default:
    cc = next_opcode(common, cc);
    SLJIT_ASSERT(cc != NULL);
    break;
    }
  }
SLJIT_ASSERT(cc == ccend);

if (control_head_found)
  length++;
if (quit_found)
  {
  if (setsom_found)
    length++;
  if (setmark_found)
    length++;
  }

*needs_control_head = control_head_found;
*has_quit = quit_found;
*has_accept = accept_found;
return length;
}