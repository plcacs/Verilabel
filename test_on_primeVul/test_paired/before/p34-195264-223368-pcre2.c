static void compile_xclass_matchingpath(compiler_common *common, PCRE2_SPTR cc, jump_list **backtracks)
{
DEFINE_COMPILER;
jump_list *found = NULL;
jump_list **list = (cc[0] & XCL_NOT) == 0 ? &found : backtracks;
sljit_uw c, charoffset, max = 256, min = READ_CHAR_MAX;
struct sljit_jump *jump = NULL;
PCRE2_SPTR ccbegin;
int compares, invertcmp, numberofcmps;
#if defined SUPPORT_UNICODE && (PCRE2_CODE_UNIT_WIDTH == 8 || PCRE2_CODE_UNIT_WIDTH == 16)
BOOL utf = common->utf;
#endif /* SUPPORT_UNICODE && PCRE2_CODE_UNIT_WIDTH == [8|16] */

#ifdef SUPPORT_UNICODE
sljit_u32 unicode_status = 0;
int typereg = TMP1;
const sljit_u32 *other_cases;
sljit_uw typeoffset;
#endif /* SUPPORT_UNICODE */

/* Scanning the necessary info. */
cc++;
ccbegin = cc;
compares = 0;

if (cc[-1] & XCL_MAP)
  {
  min = 0;
  cc += 32 / sizeof(PCRE2_UCHAR);
  }

while (*cc != XCL_END)
  {
  compares++;
  if (*cc == XCL_SINGLE)
    {
    cc ++;
    GETCHARINCTEST(c, cc);
    if (c > max) max = c;
    if (c < min) min = c;
#ifdef SUPPORT_UNICODE
    unicode_status |= XCLASS_SAVE_CHAR;
#endif /* SUPPORT_UNICODE */
    }
  else if (*cc == XCL_RANGE)
    {
    cc ++;
    GETCHARINCTEST(c, cc);
    if (c < min) min = c;
    GETCHARINCTEST(c, cc);
    if (c > max) max = c;
#ifdef SUPPORT_UNICODE
    unicode_status |= XCLASS_SAVE_CHAR;
#endif /* SUPPORT_UNICODE */
    }
#ifdef SUPPORT_UNICODE
  else
    {
    SLJIT_ASSERT(*cc == XCL_PROP || *cc == XCL_NOTPROP);
    cc++;
    if (*cc == PT_CLIST && *cc == XCL_PROP)
      {
      other_cases = PRIV(ucd_caseless_sets) + cc[1];
      while (*other_cases != NOTACHAR)
        {
        if (*other_cases > max) max = *other_cases;
        if (*other_cases < min) min = *other_cases;
        other_cases++;
        }
      }
    else
      {
      max = READ_CHAR_MAX;
      min = 0;
      }

    switch(*cc)
      {
      case PT_ANY:
      /* Any either accepts everything or ignored. */
      if (cc[-1] == XCL_PROP)
        {
        compile_char1_matchingpath(common, OP_ALLANY, cc, backtracks, FALSE);
        if (list == backtracks)
          add_jump(compiler, backtracks, JUMP(SLJIT_JUMP));
        return;
        }
      break;

      case PT_LAMP:
      case PT_GC:
      case PT_PC:
      case PT_ALNUM:
      unicode_status |= XCLASS_HAS_TYPE;
      break;

      case PT_SCX:
      unicode_status |= XCLASS_HAS_SCRIPT_EXTENSION;
      if (cc[-1] == XCL_NOTPROP)
        {
        unicode_status |= XCLASS_SCRIPT_EXTENSION_NOTPROP;
        break;
        }
      compares++;
      /* Fall through */ 

      case PT_SC:
      unicode_status |= XCLASS_HAS_SCRIPT;
      break;

      case PT_SPACE:
      case PT_PXSPACE:
      case PT_WORD:
      case PT_PXGRAPH:
      case PT_PXPRINT:
      case PT_PXPUNCT:
      unicode_status |= XCLASS_SAVE_CHAR | XCLASS_HAS_TYPE;
      break;

      case PT_CLIST:
      case PT_UCNC:
      unicode_status |= XCLASS_SAVE_CHAR;
      break;

      case PT_BOOL:
      unicode_status |= XCLASS_HAS_BOOL;
      break;

      case PT_BIDICL:
      unicode_status |= XCLASS_HAS_BIDICL;
      break;

      default:
      SLJIT_UNREACHABLE();
      break;
      }
    cc += 2;
    }
#endif /* SUPPORT_UNICODE */
  }
SLJIT_ASSERT(compares > 0);

/* We are not necessary in utf mode even in 8 bit mode. */
cc = ccbegin;
if ((cc[-1] & XCL_NOT) != 0)
  read_char(common, min, max, backtracks, READ_CHAR_UPDATE_STR_PTR);
else
  {
#ifdef SUPPORT_UNICODE
  read_char(common, min, max, (unicode_status & XCLASS_NEEDS_UCD) ? backtracks : NULL, 0);
#else /* !SUPPORT_UNICODE */
  read_char(common, min, max, NULL, 0);
#endif /* SUPPORT_UNICODE */
  }

if ((cc[-1] & XCL_HASPROP) == 0)
  {
  if ((cc[-1] & XCL_MAP) != 0)
    {
    jump = CMP(SLJIT_GREATER, TMP1, 0, SLJIT_IMM, 255);
    if (!optimize_class(common, (const sljit_u8 *)cc, (((const sljit_u8 *)cc)[31] & 0x80) != 0, TRUE, &found))
      {
      OP2(SLJIT_AND, TMP2, 0, TMP1, 0, SLJIT_IMM, 0x7);
      OP2(SLJIT_LSHR, TMP1, 0, TMP1, 0, SLJIT_IMM, 3);
      OP1(SLJIT_MOV_U8, TMP1, 0, SLJIT_MEM1(TMP1), (sljit_sw)cc);
      OP2(SLJIT_SHL, TMP2, 0, SLJIT_IMM, 1, TMP2, 0);
      OP2U(SLJIT_AND | SLJIT_SET_Z, TMP1, 0, TMP2, 0);
      add_jump(compiler, &found, JUMP(SLJIT_NOT_ZERO));
      }

    add_jump(compiler, backtracks, JUMP(SLJIT_JUMP));
    JUMPHERE(jump);

    cc += 32 / sizeof(PCRE2_UCHAR);
    }
  else
    {
    OP2(SLJIT_SUB, TMP2, 0, TMP1, 0, SLJIT_IMM, min);
    add_jump(compiler, (cc[-1] & XCL_NOT) == 0 ? backtracks : &found, CMP(SLJIT_GREATER, TMP2, 0, SLJIT_IMM, max - min));
    }
  }
else if ((cc[-1] & XCL_MAP) != 0)
  {
  OP1(SLJIT_MOV, RETURN_ADDR, 0, TMP1, 0);
#ifdef SUPPORT_UNICODE
  unicode_status |= XCLASS_CHAR_SAVED;
#endif /* SUPPORT_UNICODE */
  if (!optimize_class(common, (const sljit_u8 *)cc, FALSE, TRUE, list))
    {
#if PCRE2_CODE_UNIT_WIDTH == 8
    jump = NULL;
    if (common->utf)
#endif /* PCRE2_CODE_UNIT_WIDTH == 8 */
      jump = CMP(SLJIT_GREATER, TMP1, 0, SLJIT_IMM, 255);

    OP2(SLJIT_AND, TMP2, 0, TMP1, 0, SLJIT_IMM, 0x7);
    OP2(SLJIT_LSHR, TMP1, 0, TMP1, 0, SLJIT_IMM, 3);
    OP1(SLJIT_MOV_U8, TMP1, 0, SLJIT_MEM1(TMP1), (sljit_sw)cc);
    OP2(SLJIT_SHL, TMP2, 0, SLJIT_IMM, 1, TMP2, 0);
    OP2U(SLJIT_AND | SLJIT_SET_Z, TMP1, 0, TMP2, 0);
    add_jump(compiler, list, JUMP(SLJIT_NOT_ZERO));

#if PCRE2_CODE_UNIT_WIDTH == 8
    if (common->utf)
#endif /* PCRE2_CODE_UNIT_WIDTH == 8 */
      JUMPHERE(jump);
    }

  OP1(SLJIT_MOV, TMP1, 0, RETURN_ADDR, 0);
  cc += 32 / sizeof(PCRE2_UCHAR);
  }

#ifdef SUPPORT_UNICODE
if (unicode_status & XCLASS_NEEDS_UCD)
  {
  if ((unicode_status & (XCLASS_SAVE_CHAR | XCLASS_CHAR_SAVED)) == XCLASS_SAVE_CHAR)
    OP1(SLJIT_MOV, RETURN_ADDR, 0, TMP1, 0);

#if PCRE2_CODE_UNIT_WIDTH == 32
  if (!common->utf)
    {
    jump = CMP(SLJIT_LESS, TMP1, 0, SLJIT_IMM, MAX_UTF_CODE_POINT + 1);
    OP1(SLJIT_MOV, TMP1, 0, SLJIT_IMM, UNASSIGNED_UTF_CHAR);
    JUMPHERE(jump);
    }
#endif /* PCRE2_CODE_UNIT_WIDTH == 32 */

  OP2(SLJIT_LSHR, TMP2, 0, TMP1, 0, SLJIT_IMM, UCD_BLOCK_SHIFT);
  OP2(SLJIT_SHL, TMP2, 0, TMP2, 0, SLJIT_IMM, 1);
  OP1(SLJIT_MOV_U16, TMP2, 0, SLJIT_MEM1(TMP2), (sljit_sw)PRIV(ucd_stage1));
  OP2(SLJIT_AND, TMP1, 0, TMP1, 0, SLJIT_IMM, UCD_BLOCK_MASK);
  OP2(SLJIT_SHL, TMP2, 0, TMP2, 0, SLJIT_IMM, UCD_BLOCK_SHIFT);
  OP2(SLJIT_ADD, TMP1, 0, TMP1, 0, TMP2, 0);
  OP1(SLJIT_MOV, TMP2, 0, SLJIT_IMM, (sljit_sw)PRIV(ucd_stage2));
  OP1(SLJIT_MOV_U16, TMP2, 0, SLJIT_MEM2(TMP2, TMP1), 1);
  OP2(SLJIT_SHL, TMP1, 0, TMP2, 0, SLJIT_IMM, 3);
  OP2(SLJIT_SHL, TMP2, 0, TMP2, 0, SLJIT_IMM, 2);
  OP2(SLJIT_ADD, TMP2, 0, TMP2, 0, TMP1, 0);

  ccbegin = cc;

  if (unicode_status & XCLASS_HAS_BIDICL)
    {
    OP1(SLJIT_MOV_U16, TMP1, 0, SLJIT_MEM1(TMP2), (sljit_sw)PRIV(ucd_records) + SLJIT_OFFSETOF(ucd_record, scriptx_bidiclass));
    OP2(SLJIT_LSHR, TMP1, 0, TMP1, 0, SLJIT_IMM, UCD_BIDICLASS_SHIFT);

    while (*cc != XCL_END)
      {
      if (*cc == XCL_SINGLE)
        {
        cc ++;
        GETCHARINCTEST(c, cc);
        }
      else if (*cc == XCL_RANGE)
        {
        cc ++;
        GETCHARINCTEST(c, cc);
        GETCHARINCTEST(c, cc);
        }
      else
        {
        SLJIT_ASSERT(*cc == XCL_PROP || *cc == XCL_NOTPROP);
        cc++;
        if (*cc == PT_BIDICL)
          {
          compares--;
          invertcmp = (compares == 0 && list != backtracks);
          if (cc[-1] == XCL_NOTPROP)
            invertcmp ^= 0x1;
          jump = CMP(SLJIT_EQUAL ^ invertcmp, TMP1, 0, SLJIT_IMM, (int)cc[1]);
          add_jump(compiler, compares > 0 ? list : backtracks, jump);
          }
        cc += 2;
        }
      }

    cc = ccbegin;
    }

  if (unicode_status & XCLASS_HAS_BOOL)
    {
    OP1(SLJIT_MOV_U16, TMP1, 0, SLJIT_MEM1(TMP2), (sljit_sw)PRIV(ucd_records) + SLJIT_OFFSETOF(ucd_record, bprops));
    OP2(SLJIT_AND, TMP1, 0, TMP1, 0, SLJIT_IMM, UCD_BPROPS_MASK);
    OP2(SLJIT_SHL, TMP1, 0, TMP1, 0, SLJIT_IMM, 2);

    while (*cc != XCL_END)
      {
      if (*cc == XCL_SINGLE)
        {
        cc ++;
        GETCHARINCTEST(c, cc);
        }
      else if (*cc == XCL_RANGE)
        {
        cc ++;
        GETCHARINCTEST(c, cc);
        GETCHARINCTEST(c, cc);
        }
      else
        {
        SLJIT_ASSERT(*cc == XCL_PROP || *cc == XCL_NOTPROP);
        cc++;
        if (*cc == PT_BOOL)
          {
          compares--;
          invertcmp = (compares == 0 && list != backtracks);
          if (cc[-1] == XCL_NOTPROP)
            invertcmp ^= 0x1;

          OP2U(SLJIT_AND32 | SLJIT_SET_Z, SLJIT_MEM1(TMP1), (sljit_sw)(PRIV(ucd_boolprop_sets) + (cc[1] >> 5)), SLJIT_IMM, (sljit_sw)1 << (cc[1] & 0x1f));
          add_jump(compiler, compares > 0 ? list : backtracks, JUMP(SLJIT_NOT_ZERO ^ invertcmp));
          }
        cc += 2;
        }
      }

    cc = ccbegin;
    }

  if (unicode_status & XCLASS_HAS_SCRIPT)
    {
    OP1(SLJIT_MOV_U8, TMP1, 0, SLJIT_MEM1(TMP2), (sljit_sw)PRIV(ucd_records) + SLJIT_OFFSETOF(ucd_record, script));

    while (*cc != XCL_END)
      {
      if (*cc == XCL_SINGLE)
        {
        cc ++;
        GETCHARINCTEST(c, cc);
        }
      else if (*cc == XCL_RANGE)
        {
        cc ++;
        GETCHARINCTEST(c, cc);
        GETCHARINCTEST(c, cc);
        }
      else
        {
        SLJIT_ASSERT(*cc == XCL_PROP || *cc == XCL_NOTPROP);
        cc++;
        switch (*cc)
          {
          case PT_SCX:
          if (cc[-1] == XCL_NOTPROP)
            break;
          /* Fall through */ 

          case PT_SC:
          compares--;
          invertcmp = (compares == 0 && list != backtracks);
          if (cc[-1] == XCL_NOTPROP)
            invertcmp ^= 0x1;

          add_jump(compiler, compares > 0 ? list : backtracks, CMP(SLJIT_EQUAL ^ invertcmp, TMP1, 0, SLJIT_IMM, (int)cc[1]));
          }
        cc += 2;
        }
      }

    cc = ccbegin;
    }

  if (unicode_status & XCLASS_HAS_SCRIPT_EXTENSION)
    {
    OP1(SLJIT_MOV_U16, TMP1, 0, SLJIT_MEM1(TMP2), (sljit_sw)PRIV(ucd_records) + SLJIT_OFFSETOF(ucd_record, scriptx_bidiclass));
    OP2(SLJIT_AND, TMP1, 0, TMP1, 0, SLJIT_IMM, UCD_SCRIPTX_MASK);
    OP2(SLJIT_SHL, TMP1, 0, TMP1, 0, SLJIT_IMM, 2);

    if (unicode_status & XCLASS_SCRIPT_EXTENSION_NOTPROP)
      {
      if (unicode_status & XCLASS_HAS_TYPE)
        {
        if (unicode_status & XCLASS_SAVE_CHAR)
          {
          OP1(SLJIT_MOV, SLJIT_MEM1(SLJIT_SP), LOCALS0, TMP2, 0);
          unicode_status |= XCLASS_SCRIPT_EXTENSION_RESTORE_LOCALS0;
          }
        else
          {
          OP1(SLJIT_MOV, RETURN_ADDR, 0, TMP2, 0);
          unicode_status |= XCLASS_SCRIPT_EXTENSION_RESTORE_RETURN_ADDR;
          }
        }
      OP1(SLJIT_MOV_U8, TMP2, 0, SLJIT_MEM1(TMP2), (sljit_sw)PRIV(ucd_records) + SLJIT_OFFSETOF(ucd_record, script));
      }

    while (*cc != XCL_END)
      {
      if (*cc == XCL_SINGLE)
        {
        cc ++;
        GETCHARINCTEST(c, cc);
        }
      else if (*cc == XCL_RANGE)
        {
        cc ++;
        GETCHARINCTEST(c, cc);
        GETCHARINCTEST(c, cc);
        }
      else
        {
        SLJIT_ASSERT(*cc == XCL_PROP || *cc == XCL_NOTPROP);
        cc++;
        if (*cc == PT_SCX)
          {
          compares--;
          invertcmp = (compares == 0 && list != backtracks);

          jump = NULL;
          if (cc[-1] == XCL_NOTPROP)
            {
            jump = CMP(SLJIT_EQUAL, TMP2, 0, SLJIT_IMM, (int)cc[1]);
            if (invertcmp)
              {
              add_jump(compiler, backtracks, jump);
              jump = NULL;
              }
            invertcmp ^= 0x1;
            }

          OP2U(SLJIT_AND32 | SLJIT_SET_Z, SLJIT_MEM1(TMP1), (sljit_sw)(PRIV(ucd_script_sets) + (cc[1] >> 5)), SLJIT_IMM, (sljit_sw)1 << (cc[1] & 0x1f));
          add_jump(compiler, compares > 0 ? list : backtracks, JUMP(SLJIT_NOT_ZERO ^ invertcmp));

          if (jump != NULL)
            JUMPHERE(jump);
          }
        cc += 2;
        }
      }

    if (unicode_status & XCLASS_SCRIPT_EXTENSION_RESTORE_LOCALS0)
      OP1(SLJIT_MOV, TMP2, 0, SLJIT_MEM1(SLJIT_SP), LOCALS0);
    else if (unicode_status & XCLASS_SCRIPT_EXTENSION_RESTORE_RETURN_ADDR)
      OP1(SLJIT_MOV, TMP2, 0, RETURN_ADDR, 0);
    cc = ccbegin;
    }

  if (unicode_status & XCLASS_SAVE_CHAR)
    OP1(SLJIT_MOV, TMP1, 0, RETURN_ADDR, 0);

  if (unicode_status & XCLASS_HAS_TYPE)
    {
    if (unicode_status & XCLASS_SAVE_CHAR)
      typereg = RETURN_ADDR;

    OP1(SLJIT_MOV_U8, typereg, 0, SLJIT_MEM1(TMP2), (sljit_sw)PRIV(ucd_records) + SLJIT_OFFSETOF(ucd_record, chartype));
    }
  }
#endif /* SUPPORT_UNICODE */

/* Generating code. */
charoffset = 0;
numberofcmps = 0;
#ifdef SUPPORT_UNICODE
typeoffset = 0;
#endif /* SUPPORT_UNICODE */

while (*cc != XCL_END)
  {
  compares--;
  invertcmp = (compares == 0 && list != backtracks);
  jump = NULL;

  if (*cc == XCL_SINGLE)
    {
    cc ++;
    GETCHARINCTEST(c, cc);

    if (numberofcmps < 3 && (*cc == XCL_SINGLE || *cc == XCL_RANGE))
      {
      OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, (sljit_sw)(c - charoffset));
      OP_FLAGS(numberofcmps == 0 ? SLJIT_MOV : SLJIT_OR, TMP2, 0, SLJIT_EQUAL);
      numberofcmps++;
      }
    else if (numberofcmps > 0)
      {
      OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, (sljit_sw)(c - charoffset));
      OP_FLAGS(SLJIT_OR | SLJIT_SET_Z, TMP2, 0, SLJIT_EQUAL);
      jump = JUMP(SLJIT_NOT_ZERO ^ invertcmp);
      numberofcmps = 0;
      }
    else
      {
      jump = CMP(SLJIT_EQUAL ^ invertcmp, TMP1, 0, SLJIT_IMM, (sljit_sw)(c - charoffset));
      numberofcmps = 0;
      }
    }
  else if (*cc == XCL_RANGE)
    {
    cc ++;
    GETCHARINCTEST(c, cc);
    SET_CHAR_OFFSET(c);
    GETCHARINCTEST(c, cc);

    if (numberofcmps < 3 && (*cc == XCL_SINGLE || *cc == XCL_RANGE))
      {
      OP2U(SLJIT_SUB | SLJIT_SET_LESS_EQUAL, TMP1, 0, SLJIT_IMM, (sljit_sw)(c - charoffset));
      OP_FLAGS(numberofcmps == 0 ? SLJIT_MOV : SLJIT_OR, TMP2, 0, SLJIT_LESS_EQUAL);
      numberofcmps++;
      }
    else if (numberofcmps > 0)
      {
      OP2U(SLJIT_SUB | SLJIT_SET_LESS_EQUAL, TMP1, 0, SLJIT_IMM, (sljit_sw)(c - charoffset));
      OP_FLAGS(SLJIT_OR | SLJIT_SET_Z, TMP2, 0, SLJIT_LESS_EQUAL);
      jump = JUMP(SLJIT_NOT_ZERO ^ invertcmp);
      numberofcmps = 0;
      }
    else
      {
      jump = CMP(SLJIT_LESS_EQUAL ^ invertcmp, TMP1, 0, SLJIT_IMM, (sljit_sw)(c - charoffset));
      numberofcmps = 0;
      }
    }
#ifdef SUPPORT_UNICODE
  else
    {
    SLJIT_ASSERT(*cc == XCL_PROP || *cc == XCL_NOTPROP);
    if (*cc == XCL_NOTPROP)
      invertcmp ^= 0x1;
    cc++;
    switch(*cc)
      {
      case PT_ANY:
      if (!invertcmp)
        jump = JUMP(SLJIT_JUMP);
      break;

      case PT_LAMP:
      OP2U(SLJIT_SUB | SLJIT_SET_Z, typereg, 0, SLJIT_IMM, ucp_Lu - typeoffset);
      OP_FLAGS(SLJIT_MOV, TMP2, 0, SLJIT_EQUAL);
      OP2U(SLJIT_SUB | SLJIT_SET_Z, typereg, 0, SLJIT_IMM, ucp_Ll - typeoffset);
      OP_FLAGS(SLJIT_OR, TMP2, 0, SLJIT_EQUAL);
      OP2U(SLJIT_SUB | SLJIT_SET_Z, typereg, 0, SLJIT_IMM, ucp_Lt - typeoffset);
      OP_FLAGS(SLJIT_OR | SLJIT_SET_Z, TMP2, 0, SLJIT_EQUAL);
      jump = JUMP(SLJIT_NOT_ZERO ^ invertcmp);
      break;

      case PT_GC:
      c = PRIV(ucp_typerange)[(int)cc[1] * 2];
      SET_TYPE_OFFSET(c);
      jump = CMP(SLJIT_LESS_EQUAL ^ invertcmp, typereg, 0, SLJIT_IMM, PRIV(ucp_typerange)[(int)cc[1] * 2 + 1] - c);
      break;

      case PT_PC:
      jump = CMP(SLJIT_EQUAL ^ invertcmp, typereg, 0, SLJIT_IMM, (int)cc[1] - typeoffset);
      break;

      case PT_SC:
      case PT_SCX:
      case PT_BOOL:
      case PT_BIDICL:
      compares++;
      /* Do nothing. */
      break;

      case PT_SPACE:
      case PT_PXSPACE:
      SET_CHAR_OFFSET(9);
      OP2U(SLJIT_SUB | SLJIT_SET_LESS_EQUAL, TMP1, 0, SLJIT_IMM, 0xd - 0x9);
      OP_FLAGS(SLJIT_MOV, TMP2, 0, SLJIT_LESS_EQUAL);

      OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, 0x85 - 0x9);
      OP_FLAGS(SLJIT_OR, TMP2, 0, SLJIT_EQUAL);

      OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, 0x180e - 0x9);
      OP_FLAGS(SLJIT_OR, TMP2, 0, SLJIT_EQUAL);

      SET_TYPE_OFFSET(ucp_Zl);
      OP2U(SLJIT_SUB | SLJIT_SET_LESS_EQUAL, typereg, 0, SLJIT_IMM, ucp_Zs - ucp_Zl);
      OP_FLAGS(SLJIT_OR | SLJIT_SET_Z, TMP2, 0, SLJIT_LESS_EQUAL);
      jump = JUMP(SLJIT_NOT_ZERO ^ invertcmp);
      break;

      case PT_WORD:
      OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, (sljit_sw)(CHAR_UNDERSCORE - charoffset));
      OP_FLAGS(SLJIT_MOV, TMP2, 0, SLJIT_EQUAL);
      /* Fall through. */

      case PT_ALNUM:
      SET_TYPE_OFFSET(ucp_Ll);
      OP2U(SLJIT_SUB | SLJIT_SET_LESS_EQUAL, typereg, 0, SLJIT_IMM, ucp_Lu - ucp_Ll);
      OP_FLAGS((*cc == PT_ALNUM) ? SLJIT_MOV : SLJIT_OR, TMP2, 0, SLJIT_LESS_EQUAL);
      SET_TYPE_OFFSET(ucp_Nd);
      OP2U(SLJIT_SUB | SLJIT_SET_LESS_EQUAL, typereg, 0, SLJIT_IMM, ucp_No - ucp_Nd);
      OP_FLAGS(SLJIT_OR | SLJIT_SET_Z, TMP2, 0, SLJIT_LESS_EQUAL);
      jump = JUMP(SLJIT_NOT_ZERO ^ invertcmp);
      break;

      case PT_CLIST:
      other_cases = PRIV(ucd_caseless_sets) + cc[1];

      /* At least three characters are required.
         Otherwise this case would be handled by the normal code path. */
      SLJIT_ASSERT(other_cases[0] != NOTACHAR && other_cases[1] != NOTACHAR && other_cases[2] != NOTACHAR);
      SLJIT_ASSERT(other_cases[0] < other_cases[1] && other_cases[1] < other_cases[2]);

      /* Optimizing character pairs, if their difference is power of 2. */
      if (is_powerof2(other_cases[1] ^ other_cases[0]))
        {
        if (charoffset == 0)
          OP2(SLJIT_OR, TMP2, 0, TMP1, 0, SLJIT_IMM, other_cases[1] ^ other_cases[0]);
        else
          {
          OP2(SLJIT_ADD, TMP2, 0, TMP1, 0, SLJIT_IMM, (sljit_sw)charoffset);
          OP2(SLJIT_OR, TMP2, 0, TMP2, 0, SLJIT_IMM, other_cases[1] ^ other_cases[0]);
          }
        OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP2, 0, SLJIT_IMM, other_cases[1]);
        OP_FLAGS(SLJIT_MOV, TMP2, 0, SLJIT_EQUAL);
        other_cases += 2;
        }
      else if (is_powerof2(other_cases[2] ^ other_cases[1]))
        {
        if (charoffset == 0)
          OP2(SLJIT_OR, TMP2, 0, TMP1, 0, SLJIT_IMM, other_cases[2] ^ other_cases[1]);
        else
          {
          OP2(SLJIT_ADD, TMP2, 0, TMP1, 0, SLJIT_IMM, (sljit_sw)charoffset);
          OP2(SLJIT_OR, TMP2, 0, TMP2, 0, SLJIT_IMM, other_cases[1] ^ other_cases[0]);
          }
        OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP2, 0, SLJIT_IMM, other_cases[2]);
        OP_FLAGS(SLJIT_MOV, TMP2, 0, SLJIT_EQUAL);

        OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, (sljit_sw)(other_cases[0] - charoffset));
        OP_FLAGS(SLJIT_OR | ((other_cases[3] == NOTACHAR) ? SLJIT_SET_Z : 0), TMP2, 0, SLJIT_EQUAL);

        other_cases += 3;
        }
      else
        {
        OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, (sljit_sw)(*other_cases++ - charoffset));
        OP_FLAGS(SLJIT_MOV, TMP2, 0, SLJIT_EQUAL);
        }

      while (*other_cases != NOTACHAR)
        {
        OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, (sljit_sw)(*other_cases++ - charoffset));
        OP_FLAGS(SLJIT_OR | ((*other_cases == NOTACHAR) ? SLJIT_SET_Z : 0), TMP2, 0, SLJIT_EQUAL);
        }
      jump = JUMP(SLJIT_NOT_ZERO ^ invertcmp);
      break;

      case PT_UCNC:
      OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, (sljit_sw)(CHAR_DOLLAR_SIGN - charoffset));
      OP_FLAGS(SLJIT_MOV, TMP2, 0, SLJIT_EQUAL);
      OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, (sljit_sw)(CHAR_COMMERCIAL_AT - charoffset));
      OP_FLAGS(SLJIT_OR, TMP2, 0, SLJIT_EQUAL);
      OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, (sljit_sw)(CHAR_GRAVE_ACCENT - charoffset));
      OP_FLAGS(SLJIT_OR, TMP2, 0, SLJIT_EQUAL);

      SET_CHAR_OFFSET(0xa0);
      OP2U(SLJIT_SUB | SLJIT_SET_LESS_EQUAL, TMP1, 0, SLJIT_IMM, (sljit_sw)(0xd7ff - charoffset));
      OP_FLAGS(SLJIT_OR, TMP2, 0, SLJIT_LESS_EQUAL);
      SET_CHAR_OFFSET(0);
      OP2U(SLJIT_SUB | SLJIT_SET_GREATER_EQUAL, TMP1, 0, SLJIT_IMM, 0xe000 - 0);
      OP_FLAGS(SLJIT_OR | SLJIT_SET_Z, TMP2, 0, SLJIT_GREATER_EQUAL);
      jump = JUMP(SLJIT_NOT_ZERO ^ invertcmp);
      break;

      case PT_PXGRAPH:
      /* C and Z groups are the farthest two groups. */
      SET_TYPE_OFFSET(ucp_Ll);
      OP2U(SLJIT_SUB | SLJIT_SET_GREATER, typereg, 0, SLJIT_IMM, ucp_So - ucp_Ll);
      OP_FLAGS(SLJIT_MOV, TMP2, 0, SLJIT_GREATER);

      jump = CMP(SLJIT_NOT_EQUAL, typereg, 0, SLJIT_IMM, ucp_Cf - ucp_Ll);

      /* In case of ucp_Cf, we overwrite the result. */
      SET_CHAR_OFFSET(0x2066);
      OP2U(SLJIT_SUB | SLJIT_SET_LESS_EQUAL, TMP1, 0, SLJIT_IMM, 0x2069 - 0x2066);
      OP_FLAGS(SLJIT_MOV, TMP2, 0, SLJIT_LESS_EQUAL);

      OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, 0x061c - 0x2066);
      OP_FLAGS(SLJIT_OR, TMP2, 0, SLJIT_EQUAL);

      OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, 0x180e - 0x2066);
      OP_FLAGS(SLJIT_OR, TMP2, 0, SLJIT_EQUAL);

      JUMPHERE(jump);
      jump = CMP(SLJIT_ZERO ^ invertcmp, TMP2, 0, SLJIT_IMM, 0);
      break;

      case PT_PXPRINT:
      /* C and Z groups are the farthest two groups. */
      SET_TYPE_OFFSET(ucp_Ll);
      OP2U(SLJIT_SUB | SLJIT_SET_GREATER, typereg, 0, SLJIT_IMM, ucp_So - ucp_Ll);
      OP_FLAGS(SLJIT_MOV, TMP2, 0, SLJIT_GREATER);

      OP2U(SLJIT_SUB | SLJIT_SET_Z, typereg, 0, SLJIT_IMM, ucp_Zs - ucp_Ll);
      OP_FLAGS(SLJIT_AND, TMP2, 0, SLJIT_NOT_EQUAL);

      jump = CMP(SLJIT_NOT_EQUAL, typereg, 0, SLJIT_IMM, ucp_Cf - ucp_Ll);

      /* In case of ucp_Cf, we overwrite the result. */
      SET_CHAR_OFFSET(0x2066);
      OP2U(SLJIT_SUB | SLJIT_SET_LESS_EQUAL, TMP1, 0, SLJIT_IMM, 0x2069 - 0x2066);
      OP_FLAGS(SLJIT_MOV, TMP2, 0, SLJIT_LESS_EQUAL);

      OP2U(SLJIT_SUB | SLJIT_SET_Z, TMP1, 0, SLJIT_IMM, 0x061c - 0x2066);
      OP_FLAGS(SLJIT_OR, TMP2, 0, SLJIT_EQUAL);

      JUMPHERE(jump);
      jump = CMP(SLJIT_ZERO ^ invertcmp, TMP2, 0, SLJIT_IMM, 0);
      break;

      case PT_PXPUNCT:
      SET_TYPE_OFFSET(ucp_Sc);
      OP2U(SLJIT_SUB | SLJIT_SET_LESS_EQUAL, typereg, 0, SLJIT_IMM, ucp_So - ucp_Sc);
      OP_FLAGS(SLJIT_MOV, TMP2, 0, SLJIT_LESS_EQUAL);

      SET_CHAR_OFFSET(0);
      OP2U(SLJIT_SUB | SLJIT_SET_LESS_EQUAL, TMP1, 0, SLJIT_IMM, 0x7f);
      OP_FLAGS(SLJIT_AND, TMP2, 0, SLJIT_LESS_EQUAL);

      SET_TYPE_OFFSET(ucp_Pc);
      OP2U(SLJIT_SUB | SLJIT_SET_LESS_EQUAL, typereg, 0, SLJIT_IMM, ucp_Ps - ucp_Pc);
      OP_FLAGS(SLJIT_OR | SLJIT_SET_Z, TMP2, 0, SLJIT_LESS_EQUAL);
      jump = JUMP(SLJIT_NOT_ZERO ^ invertcmp);
      break;

      default:
      SLJIT_UNREACHABLE();
      break;
      }
    cc += 2;
    }
#endif /* SUPPORT_UNICODE */

  if (jump != NULL)
    add_jump(compiler, compares > 0 ? list : backtracks, jump);
  }

if (found != NULL)
  set_jumps(found, LABEL());
}