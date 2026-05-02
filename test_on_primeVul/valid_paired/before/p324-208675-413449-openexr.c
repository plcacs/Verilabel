FastHufDecoder::FastHufDecoder
    (const char *&table,
     int numBytes,
     int minSymbol,
     int maxSymbol,
     int rleSymbol)
:
    _rleSymbol (rleSymbol),
    _numSymbols (0),
    _minCodeLength (255),
    _maxCodeLength (0),
    _idToSymbol (0)
{
    //
    // List of symbols that we find with non-zero code lengths
    // (listed in the order we find them). Store these in the
    // same format as the code book stores codes + lengths - 
    // low 6 bits are the length, everything above that is
    // the symbol.
    //

    std::vector<Int64> symbols;

    //
    // The 'base' table is the minimum code at each code length. base[i]
    // is the smallest code (numerically) of length i.
    //

    Int64 base[MAX_CODE_LEN + 1];     

    //
    // The 'offset' table is the position (in sorted order) of the first id
    // of a given code lenght. Array is indexed by code length, like base.  
    //

    Int64 offset[MAX_CODE_LEN + 1];   

    //
    // Count of how many codes at each length there are. Array is 
    // indexed by code length, like base and offset.
    //

    size_t codeCount[MAX_CODE_LEN + 1];    

    for (int i = 0; i <= MAX_CODE_LEN; ++i)
    {
        codeCount[i] = 0;
        base[i]      = 0xffffffffffffffffULL;
        offset[i]    = 0;
    }

    //
    // Count the number of codes, the min/max code lengths, the number of
    // codes with each length, and record symbols with non-zero code
    // length as we find them.
    //

    const char *currByte     = table;
    Int64       currBits     = 0;
    int         currBitCount = 0;

    const int SHORT_ZEROCODE_RUN = 59;
    const int LONG_ZEROCODE_RUN  = 63;
    const int SHORTEST_LONG_RUN  = 2 + LONG_ZEROCODE_RUN - SHORT_ZEROCODE_RUN;

    for (Int64 symbol = static_cast<Int64>(minSymbol); symbol <= static_cast<Int64>(maxSymbol); symbol++)
    {
        if (currByte - table > numBytes)
        {
            throw IEX_NAMESPACE::InputExc ("Error decoding Huffman table "
                                           "(Truncated table data).");
        }

        //
        // Next code length - either:
        //       0-58  (literal code length)
        //       59-62 (various lengths runs of 0)
        //       63    (run of n 0's, with n is the next 8 bits)
        //

        Int64 codeLen = readBits (6, currBits, currBitCount, currByte);

        if (codeLen == (Int64) LONG_ZEROCODE_RUN)
        {
            if (currByte - table > numBytes)
            {
                throw IEX_NAMESPACE::InputExc ("Error decoding Huffman table "
                                               "(Truncated table data).");
            }

            int runLen = readBits (8, currBits, currBitCount, currByte) +
                         SHORTEST_LONG_RUN;

            if (symbol + runLen > static_cast<Int64>(maxSymbol + 1))
            {
                throw IEX_NAMESPACE::InputExc ("Error decoding Huffman table "
                                               "(Run beyond end of table).");
            }
            
            symbol += runLen - 1;

        }
        else if (codeLen >= static_cast<Int64>(SHORT_ZEROCODE_RUN))
        {
            int runLen = codeLen - SHORT_ZEROCODE_RUN + 2;

            if (symbol + runLen > static_cast<Int64>(maxSymbol + 1))
            {
                throw IEX_NAMESPACE::InputExc ("Error decoding Huffman table "
                                               "(Run beyond end of table).");
            }

            symbol += runLen - 1;

        }
        else if (codeLen != 0)
        {
            symbols.push_back ((symbol << 6) | (codeLen & 63));

            if (codeLen < _minCodeLength)
                _minCodeLength = codeLen;

            if (codeLen > _maxCodeLength)
                _maxCodeLength = codeLen;

            codeCount[codeLen]++;
        }
    }

    for (int i = 0; i < MAX_CODE_LEN; ++i)
        _numSymbols += codeCount[i];

    table = currByte;

    //
    // Compute base - once we have the code length counts, there
    //                is a closed form solution for this
    //

    {
        double* countTmp = new double[_maxCodeLength+1];

        for (int l = _minCodeLength; l <= _maxCodeLength; ++l)
        {
            countTmp[l] = (double)codeCount[l] * 
                          (double)(2 << (_maxCodeLength-l));
        }
    
        for (int l = _minCodeLength; l <= _maxCodeLength; ++l)
        {
            double tmp = 0;

            for (int k =l + 1; k <= _maxCodeLength; ++k)
                tmp += countTmp[k];
            
            tmp /= (double)(2 << (_maxCodeLength - l));

            base[l] = (Int64)ceil (tmp);
        }

        delete [] countTmp;
    }
   
    //
    // Compute offset - these are the positions of the first
    //                  id (not symbol) that has length [i]
    //

    offset[_maxCodeLength] = 0;

    for (int i= _maxCodeLength - 1; i >= _minCodeLength; i--)
        offset[i] = offset[i + 1] + codeCount[i + 1];

    //
    // Allocate and fill the symbol-to-id mapping. Smaller Ids should be
    // mapped to less-frequent symbols (which have longer codes). Use
    // the offset table to tell us where the id's for a given code 
    // length start off.
    //

    _idToSymbol = new int[_numSymbols];

    Int64 mapping[MAX_CODE_LEN + 1];
    for (int i = 0; i < MAX_CODE_LEN + 1; ++i) 
        mapping[i] = -1;
    for (int i = _minCodeLength; i <= _maxCodeLength; ++i)
        mapping[i] = offset[i];

    for (std::vector<Int64>::const_iterator i = symbols.begin(); 
         i != symbols.end();
         ++i)
    {
        int codeLen = *i & 63;
        int symbol  = *i >> 6;

        if (mapping[codeLen] >= static_cast<Int64>(_numSymbols))
        {
            delete[] _idToSymbol;
            _idToSymbol = NULL;
            throw IEX_NAMESPACE::InputExc ("Huffman decode error "
                                           "(Invalid symbol in header).");
        }
        _idToSymbol[mapping[codeLen]] = symbol;
        mapping[codeLen]++;
    }

    //
    // exceptions can be thrown whilst building tables. Delete
    // _idToSynmbol before re-throwing to prevent memory leak
    //
    try
    {
      buildTables(base, offset);
    }catch(...)
    {
            delete[] _idToSymbol;
            _idToSymbol = NULL;
            throw;
    }
}