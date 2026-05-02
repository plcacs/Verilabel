filepos_t KaxInternalBlock::ReadData(IOCallback & input, ScopeMode ReadFully)
{
  filepos_t Result;

  FirstFrameLocation = input.getFilePointer(); // will be updated accordingly below

  SetValueIsSet(false);

  try {
    if (ReadFully == SCOPE_ALL_DATA) {
      Result = EbmlBinary::ReadData(input, ReadFully);
      if (Result != GetSize())
        throw SafeReadIOCallback::EndOfStreamX(GetSize() - Result);

      binary *BufferStart = EbmlBinary::GetBuffer();

      SafeReadIOCallback Mem(*this);
      uint8 BlockHeadSize = 4;

      // update internal values
      TrackNumber = Mem.GetUInt8();
      if ((TrackNumber & 0x80) == 0) {
        // there is extra data
        if ((TrackNumber & 0x40) == 0) {
          // We don't support track numbers that large !
          throw SafeReadIOCallback::EndOfStreamX(0);
        }
        TrackNumber = (TrackNumber & 0x3F) << 8;
        TrackNumber += Mem.GetUInt8();
        BlockHeadSize++;
      } else {
        TrackNumber &= 0x7F;
      }

      LocalTimecode = int16(Mem.GetUInt16BE());
      bLocalTimecodeUsed = true;

      uint8 Flags = Mem.GetUInt8();
      if (EbmlId(*this) == EBML_ID(KaxSimpleBlock)) {
        bIsKeyframe = (Flags & 0x80) != 0;
        bIsDiscardable = (Flags & 0x01) != 0;
      }
      mInvisible = (Flags & 0x08) >> 3;
      mLacing = LacingType((Flags & 0x06) >> 1);

      // put all Frames in the list
      if (mLacing == LACING_NONE) {
        FirstFrameLocation += Mem.GetPosition();
        DataBuffer * soloFrame = new DataBuffer(BufferStart + Mem.GetPosition(), GetSize() - BlockHeadSize);
        myBuffers.push_back(soloFrame);
        SizeList.resize(1);
        SizeList[0] = GetSize() - BlockHeadSize;
      } else {
        // read the number of frames in the lace
        uint32 LastBufferSize = GetSize() - BlockHeadSize - 1; // 1 for number of frame
        uint8 FrameNum = Mem.GetUInt8(); // number of frames in the lace - 1
        // read the list of frame sizes
        uint8 Index;
        int32 FrameSize;
        uint32 SizeRead;
        uint64 SizeUnknown;

        SizeList.resize(FrameNum + 1);

        switch (mLacing) {
          case LACING_XIPH:
            for (Index=0; Index<FrameNum; Index++) {
              // get the size of the frame
              FrameSize = 0;
              uint8 Value;
              do {
                Value = Mem.GetUInt8();
                FrameSize += Value;
                LastBufferSize--;
              } while (Value == 0xFF);
              SizeList[Index] = FrameSize;
              LastBufferSize -= FrameSize;
            }
            SizeList[Index] = LastBufferSize;
            break;
          case LACING_EBML:
            SizeRead = LastBufferSize;
            FrameSize = ReadCodedSizeValue(BufferStart + Mem.GetPosition(), SizeRead, SizeUnknown);
            SizeList[0] = FrameSize;
            Mem.Skip(SizeRead);
            LastBufferSize -= FrameSize + SizeRead;

            for (Index=1; Index<FrameNum; Index++) {
              // get the size of the frame
              SizeRead = LastBufferSize;
              FrameSize += ReadCodedSizeSignedValue(BufferStart + Mem.GetPosition(), SizeRead, SizeUnknown);
              SizeList[Index] = FrameSize;
              Mem.Skip(SizeRead);
              LastBufferSize -= FrameSize + SizeRead;
            }
            if (Index <= FrameNum) // Safety check if FrameNum == 0
              SizeList[Index] = LastBufferSize;
            break;
          case LACING_FIXED:
            for (Index=0; Index<=FrameNum; Index++) {
              // get the size of the frame
              SizeList[Index] = LastBufferSize / (FrameNum + 1);
            }
            break;
          default: // other lacing not supported
            assert(0);
        }

        FirstFrameLocation += Mem.GetPosition();

        for (Index=0; Index<=FrameNum; Index++) {
          DataBuffer * lacedFrame = new DataBuffer(BufferStart + Mem.GetPosition(), SizeList[Index]);
          myBuffers.push_back(lacedFrame);
          Mem.Skip(SizeList[Index]);
        }
      }

      binary *BufferEnd = BufferStart + GetSize();
      size_t NumFrames  = myBuffers.size();

      // Sanity checks for frame pointers and boundaries.
      for (size_t Index = 0; Index < NumFrames; ++Index) {
        binary *FrameStart  = myBuffers[Index]->Buffer();
        binary *FrameEnd    = FrameStart + myBuffers[Index]->Size();
        binary *ExpectedEnd = (Index + 1) < NumFrames ? myBuffers[Index + 1]->Buffer() : BufferEnd;

        if ((FrameStart < BufferStart) || (FrameEnd > BufferEnd) || (FrameEnd != ExpectedEnd))
          throw SafeReadIOCallback::EndOfStreamX(0);
      }

      SetValueIsSet();
    } else if (ReadFully == SCOPE_PARTIAL_DATA) {
      binary _TempHead[5];
      Result = input.read(_TempHead, 5);
      if (Result != 5)
        throw SafeReadIOCallback::EndOfStreamX(0);
      binary *cursor = _TempHead;
      binary *_tmpBuf;
      uint8 BlockHeadSize = 4;

      // update internal values
      TrackNumber = *cursor++;
      if ((TrackNumber & 0x80) == 0) {
        // there is extra data
        if ((TrackNumber & 0x40) == 0) {
          // We don't support track numbers that large !
          return Result;
        }
        TrackNumber = (TrackNumber & 0x3F) << 8;
        TrackNumber += *cursor++;
        BlockHeadSize++;
      } else {
        TrackNumber &= 0x7F;
      }

      big_int16 b16;
      b16.Eval(cursor);
      LocalTimecode = int16(b16);
      bLocalTimecodeUsed = true;
      cursor += 2;

      if (EbmlId(*this) == EBML_ID(KaxSimpleBlock)) {
        bIsKeyframe = (*cursor & 0x80) != 0;
        bIsDiscardable = (*cursor & 0x01) != 0;
      }
      mInvisible = (*cursor & 0x08) >> 3;
      mLacing = LacingType((*cursor++ & 0x06) >> 1);
      if (cursor == &_TempHead[4]) {
        _TempHead[0] = _TempHead[4];
      } else {
        Result += input.read(_TempHead, 1);
      }

      FirstFrameLocation += cursor - _TempHead;

      // put all Frames in the list
      if (mLacing != LACING_NONE) {
        // read the number of frames in the lace
        uint32 LastBufferSize = GetSize() - BlockHeadSize - 1; // 1 for number of frame
        uint8 FrameNum = _TempHead[0]; // number of frames in the lace - 1
        // read the list of frame sizes
        uint8 Index;
        int32 FrameSize;
        uint32 SizeRead;
        uint64 SizeUnknown;

        SizeList.resize(FrameNum + 1);

        switch (mLacing) {
          case LACING_XIPH:
            for (Index=0; Index<FrameNum; Index++) {
              // get the size of the frame
              FrameSize = 0;
              do {
                Result += input.read(_TempHead, 1);
                FrameSize += uint8(_TempHead[0]);
                LastBufferSize--;

                FirstFrameLocation++;
              } while (_TempHead[0] == 0xFF);

              FirstFrameLocation++;
              SizeList[Index] = FrameSize;
              LastBufferSize -= FrameSize;
            }
            SizeList[Index] = LastBufferSize;
            break;
          case LACING_EBML:
            SizeRead = LastBufferSize;
            cursor = _tmpBuf = new binary[FrameNum*4]; /// \warning assume the mean size will be coded in less than 4 bytes
            Result += input.read(cursor, FrameNum*4);
            FrameSize = ReadCodedSizeValue(cursor, SizeRead, SizeUnknown);
            SizeList[0] = FrameSize;
            cursor += SizeRead;
            LastBufferSize -= FrameSize + SizeRead;

            for (Index=1; Index<FrameNum; Index++) {
              // get the size of the frame
              SizeRead = LastBufferSize;
              FrameSize += ReadCodedSizeSignedValue(cursor, SizeRead, SizeUnknown);
              SizeList[Index] = FrameSize;
              cursor += SizeRead;
              LastBufferSize -= FrameSize + SizeRead;
            }

            FirstFrameLocation += cursor - _tmpBuf;

            SizeList[Index] = LastBufferSize;
            delete [] _tmpBuf;
            break;
          case LACING_FIXED:
            for (Index=0; Index<=FrameNum; Index++) {
              // get the size of the frame
              SizeList[Index] = LastBufferSize / (FrameNum + 1);
            }
            break;
          default: // other lacing not supported
            assert(0);
        }
      } else {
        SizeList.resize(1);
        SizeList[0] = GetSize() - BlockHeadSize;
      }
      SetValueIsSet(false);
      Result = GetSize();
    } else {
      SetValueIsSet(false);
      Result = GetSize();
    }

  } catch (SafeReadIOCallback::EndOfStreamX &) {
    SetValueIsSet(false);

    std::memset(EbmlBinary::GetBuffer(), 0, GetSize());
    myBuffers.clear();
    SizeList.clear();
    Timecode           = 0;
    LocalTimecode      = 0;
    TrackNumber        = 0;
    bLocalTimecodeUsed = false;
    FirstFrameLocation = 0;

    return 0;
  }

  return Result;
}