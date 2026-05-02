static int codeCompare(
  Parse *pParse,    /* The parsing (and code generating) context */
  Expr *pLeft,      /* The left operand */
  Expr *pRight,     /* The right operand */
  int opcode,       /* The comparison opcode */
  int in1, int in2, /* Register holding operands */
  int dest,         /* Jump here if true.  */
  int jumpIfNull,   /* If true, jump if either operand is NULL */
  int isCommuted    /* The comparison has been commuted */
){
  int p5;
  int addr;
  CollSeq *p4;

  if( isCommuted ){
    p4 = sqlite3BinaryCompareCollSeq(pParse, pRight, pLeft);
  }else{
    p4 = sqlite3BinaryCompareCollSeq(pParse, pLeft, pRight);
  }
  p5 = binaryCompareP5(pLeft, pRight, jumpIfNull);
  addr = sqlite3VdbeAddOp4(pParse->pVdbe, opcode, in2, dest, in1,
                           (void*)p4, P4_COLLSEQ);
  sqlite3VdbeChangeP5(pParse->pVdbe, (u8)p5);
  return addr;
}