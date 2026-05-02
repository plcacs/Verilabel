bool SingleComponentLSScan::ParseMCU(void)
{ 
#if ACCUSOFT_CODE
  int lines             = m_ulRemaining[0]; // total number of MCU lines processed.
  UBYTE preshift        = m_ucLowBit + FractionalColorBitsOf();
  struct Line *line     = CurrentLine(0);
  
  //
  // If a DNL marker is present, the number of remaining lines is zero. Fix it.
  if (m_pFrame->HeightOf() == 0) {
    assert(lines == 0);
    lines = 8;
  }

  assert(m_ucCount == 1);

  //
  // A "MCU" in respect to the code organization is eight lines.
  if (lines > 8) {
    lines = 8;
  }
  if (m_pFrame->HeightOf() > 0)
    m_ulRemaining[0] -= lines;
  
  assert(lines > 0);

  // Loop over lines and columns
  do {
    LONG length = m_ulWidth[0];
    LONG *lp    = line->m_pData;

#ifdef DEBUG_LS
    int xpos    = 0;
    static int linenumber = 0;
    printf("\n%4d : ",++linenumber);
#endif
     
    StartLine(0);
    if (BeginReadMCU(m_Stream.ByteStreamOf())) { // No error handling strategy. No RST in scans. Bummer!
      do {
        LONG a,b,c,d;   // neighbouring values.
        LONG d1,d2,d3;  // local gradients.
      
        GetContext(0,a,b,c,d);
        d1  = d - b;    // compute local gradients
        d2  = b - c;
        d3  = c - a;
        
        if (isRunMode(d1,d2,d3)) {
          LONG run = DecodeRun(length,m_lRunIndex[0]);
          //
          // Now fill the data.
          while(run) {
            // Update so that the next process gets the correct value.
            UpdateContext(0,a);
            // And insert the value into the target line as well.
            *lp++ = a << preshift;
#ifdef DEBUG_LS
            printf("%4d:<%2x> ",xpos++,a);
#endif
            run--,length--;
            // As long as there are pixels on the line.
          }
          //
          // More data on the line? I.e. the run did not cover the full m_lJ samples?
          // Now decode the run interruption sample.
          if (length) {
            bool negative; // the sign variable
            bool rtype;    // run interruption type
            LONG errval;   // the prediction error
            LONG merr;     // the mapped error (symbol)
            LONG rx;       // the reconstructed value
            UBYTE k;       // golomb parameter
            // Get the neighbourhood.
            GetContext(0,a,b,c,d);
            // Get the prediction mode.
            rtype  = InterruptedPredictionMode(negative,a,b);
            // Get the golomb parameter for run interruption coding.
            k      = GolombParameter(rtype);
            // Golomb-decode the error symbol.
            merr   = GolombDecode(k,m_lLimit - m_lJ[m_lRunIndex[0]] - 1);
            // Inverse the error mapping procedure.
            errval = InverseErrorMapping(merr + rtype,ErrorMappingOffset(rtype,rtype || merr,k));
            // Compute the reconstructed value.
            rx     = Reconstruct(negative,rtype?a:b,errval);
            // Update so that the next process gets the correct value.
            UpdateContext(0,rx);
            // Fill in the value into the line
            *lp    = rx << preshift;
#ifdef DEBUG_LS
            printf("%4d:<%2x> ",xpos++,*lp);
#endif
            // Update the variables of the run mode.
            UpdateState(rtype,errval);
            // Update the run index now. This is not part of
            // EncodeRun because the non-reduced run-index is
            // required for the golomb coder length limit. 
            if (m_lRunIndex[0] > 0)
              m_lRunIndex[0]--;
          } else break; // end of line.
        } else {
          UWORD ctxt;
          bool  negative; // the sign variable.
          LONG  px;       // the predicted variable.
          LONG  rx;       // the reconstructed value.
          LONG  errval;   // the error value.
          LONG  merr;     // the mapped error value.
          UBYTE k;        // the Golomb parameter.
          // Quantize the gradients.
          d1     = QuantizedGradient(d1);
          d2     = QuantizedGradient(d2);
          d3     = QuantizedGradient(d3);
          // Compute the context.
          ctxt   = Context(negative,d1,d2,d3); 
          // Compute the predicted value.
          px     = Predict(a,b,c);
          // Correct the prediction.
          px     = CorrectPrediction(ctxt,negative,px);
          // Compute the golomb parameter k from the context.
          k      = GolombParameter(ctxt);
          // Decode the error symbol.
          merr   = GolombDecode(k,m_lLimit);
          // Inverse the error symbol into an error value.
          errval = InverseErrorMapping(merr,ErrorMappingOffset(ctxt,k));
          // Update the variables.
          UpdateState(ctxt,errval);
          // Compute the reconstructed value.
          rx     = Reconstruct(negative,px,errval);
          // Update so that the next process gets the correct value.
          UpdateContext(0,rx);
          // And insert the value into the target line as well.
          *lp    = rx << preshift;
#ifdef DEBUG_LS
          printf("%4d:<%2x> ",xpos++,*lp);
#endif
        }
      } while(++lp,--length);
    } // No error handling here.
    EndLine(0);
    line = line->m_pNext;
  } while(--lines); 
  //
  // If this is the last line, gobble up all the
  // bits from bitstuffing the last byte may have left.
  // As SkipStuffing is idempotent, we can also do that
  // all the time.
  m_Stream.SkipStuffing();
#endif  
  return false;
}