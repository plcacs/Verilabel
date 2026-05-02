bool SampleInterleavedLSScan::ParseMCU(void)
{
#if ACCUSOFT_CODE
  int lines             = m_ulRemaining[0]; // total number of MCU lines processed.
  UBYTE preshift        = m_ucLowBit + FractionalColorBitsOf();
  struct Line *line[4];
  UBYTE cx;

  //
  // If a DNL marker is present, the number of remaining lines is zero. Fix it.
  if (m_pFrame->HeightOf() == 0) {
    assert(lines == 0);
    lines = 8;
  }
  //
  // A "MCU" in respect to the code organization is eight lines.
  if (lines > 8) {
    lines = 8;
  }
  if (m_pFrame->HeightOf() > 0)
    m_ulRemaining[0] -= lines;
  assert(lines > 0);
  assert(m_ucCount < 4);

  //
  // Fill the line pointers.
  for(cx = 0;cx < m_ucCount;cx++) {
    line[cx] = CurrentLine(cx);
  }

  // Loop over lines and columns
  do {
    LONG length = m_ulWidth[0];
    LONG *lp[4];

    // Get the line pointers and initialize the internal backup lines.
    for(cx = 0;cx < m_ucCount;cx++) {
      lp[cx] = line[cx]->m_pData;
      StartLine(cx);
    }

    if (BeginReadMCU(m_Stream.ByteStreamOf())) { 
      // No error handling strategy. No RST in scans. Bummer!
      do {
        LONG a[4],b[4],c[4],d[4]; // neighbouring values.
        LONG d1[4],d2[4],d3[4];   // local gradients.
        bool isrun = true;
      
        for(cx = 0;cx < m_ucCount;cx++) {
          GetContext(cx,a[cx],b[cx],c[cx],d[cx]);

          d1[cx]  = d[cx] - b[cx];    // compute local gradients
          d2[cx]  = b[cx] - c[cx];
          d3[cx]  = c[cx] - a[cx];

          //
          // Run mode only if the run condition is met for all components
          if (isrun && !isRunMode(d1[cx],d2[cx],d3[cx]))
            isrun = false;
        }
        
        if (isrun) {
          LONG run = DecodeRun(length,m_lRunIndex[0]);
          //
          // Now fill the data.
          while(run) {
            // Update so that the next process gets the correct value.
            // There is one sample per component.
            for(cx = 0;cx < m_ucCount;cx++) {
              UpdateContext(cx,a[cx]);
              // And insert the value into the target line as well.
              *lp[cx]++ = a[cx] << preshift;
            }
            run--,length--;
            // As long as there are pixels on the line.
          }
          //
          // More data on the line? I.e. the run did not cover the full m_lJ samples?
          // Now decode the run interruption sample. The rtype is here always zero.
          if (length) {
            bool negative; // the sign variable
            LONG errval;   // the prediction error
            LONG merr;     // the mapped error (symbol)
            LONG rx;       // the reconstructed value
            UBYTE k;       // golomb parameter
            //
            // Decode the interrupting pixels.
            for(cx = 0;cx < m_ucCount;cx++) {
              // Get the neighbourhood.
              GetContext(cx,a[cx],b[cx],c[cx],d[cx]);
              // The prediction mode is always false, but the sign information
              // is required.
              negative = a[cx] > b[cx];
              // Get the golomb parameter for run interruption coding.
              k       = GolombParameter(false);
              // Golomb-decode the error symbol. It is always using the common
              // run index.
              merr    = GolombDecode(k,m_lLimit - m_lJ[m_lRunIndex[0]] - 1);
              // Inverse the error mapping procedure.
              errval  = InverseErrorMapping(merr,ErrorMappingOffset(false,merr != 0,k));
              // Compute the reconstructed value.
              rx      = Reconstruct(negative,b[cx],errval);
              // Update so that the next process gets the correct value.
              UpdateContext(cx,rx);
              // Fill in the value into the line
              *lp[cx]++ = rx << preshift;
              // Update the variables of the run mode.
              UpdateState(false,errval);
            }
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
          //
          for(cx = 0;cx < m_ucCount;cx++) {
            // Quantize the gradients.
            d1[cx]  = QuantizedGradient(d1[cx]);
            d2[cx]  = QuantizedGradient(d2[cx]);
            d3[cx]  = QuantizedGradient(d3[cx]);
            // Compute the context.
            ctxt    = Context(negative,d1[cx],d2[cx],d3[cx]); 
            // Compute the predicted value.
            px      = Predict(a[cx],b[cx],c[cx]);
            // Correct the prediction.
            px      = CorrectPrediction(ctxt,negative,px);
            // Compute the golomb parameter k from the context.
            k       = GolombParameter(ctxt);
            // Decode the error symbol.
            merr    = GolombDecode(k,m_lLimit);
            // Inverse the error symbol into an error value.
            errval  = InverseErrorMapping(merr,ErrorMappingOffset(ctxt,k));
            // Update the variables.
            UpdateState(ctxt,errval);
            // Compute the reconstructed value.
            rx      = Reconstruct(negative,px,errval);
            // Update so that the next process gets the correct value.
            UpdateContext(cx,rx);
            // And insert the value into the target line as well.
            *lp[cx]++ = rx << preshift;
          }
        }
      } while(--length);
    } // No error handling here.
    //
    // Advance the line pointers.
    for(cx = 0;cx < m_ucCount;cx++) {
      EndLine(cx);
      line[cx] = line[cx]->m_pNext;
    }
    //
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