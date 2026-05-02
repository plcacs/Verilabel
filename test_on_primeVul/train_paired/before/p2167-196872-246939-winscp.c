void __fastcall TSCPFileSystem::SCPSink(const UnicodeString TargetDir,
  const UnicodeString FileName, const UnicodeString SourceDir,
  const TCopyParamType * CopyParam, bool & Success,
  TFileOperationProgressType * OperationProgress, int Params,
  int Level)
{
  struct
  {
    int SetTime;
    TDateTime Modification;
    TRights RemoteRights;
    int Attrs;
    bool Exists;
  } FileData;

  bool SkipConfirmed = false;
  bool Initialized = (Level > 0);

  FileData.SetTime = 0;

  FSecureShell->SendNull();

  while (!OperationProgress->Cancel)
  {
    // See (switch ... case 'T':)
    if (FileData.SetTime) FileData.SetTime--;

    // In case of error occurred before control record arrived.
    // We can finally use full path here, as we get current path in FileName param
    // (we used to set the file into OperationProgress->FileName, but it collided
    // with progress outputting, particularly for scripting)
    UnicodeString FullFileName = FileName;

    try
    {
      // Receive control record
      UnicodeString Line = FSecureShell->ReceiveLine();

      if (Line.Length() == 0) FTerminal->FatalError(NULL, LoadStr(SCP_EMPTY_LINE));

      if (IsLastLine(Line))
      {
        // Remote side finished copying, so remote SCP was closed
        // and we don't need to terminate it manually, see CopyToLocal()
        OperationProgress->SetCancel(csRemoteAbort);
        /* TODO 1 : Show stderror to user? */
        FSecureShell->ClearStdError();
        try
        {
          // coIgnoreWarnings should allow batch transfer to continue when
          // download of one the files fails (user denies overwriting
          // of target local file, no read permissions...)
          ReadCommandOutput(coExpectNoOutput | coRaiseExcept |
            coOnlyReturnCode | coIgnoreWarnings);
          if (!Initialized)
          {
            throw Exception(L"");
          }
        }
        catch(Exception & E)
        {
          if (!Initialized && FTerminal->Active)
          {
            FTerminal->TerminalError(&E, LoadStr(SCP_INIT_ERROR));
          }
          else
          {
            throw;
          }
        }
        return;
      }
      else
      {
        Initialized = true;

        // First character distinguish type of control record
        wchar_t Ctrl = Line[1];
        Line.Delete(1, 1);

        switch (Ctrl) {
          case 1:
            // Error (already logged by ReceiveLine())
            throw EScpFileSkipped(NULL, FMTLOAD(REMOTE_ERROR, (Line)));

          case 2:
            // Fatal error, terminate copying
            FTerminal->TerminalError(Line);
            return; // Unreachable

          case L'E': // Exit
            FSecureShell->SendNull();
            return;

          case L'T':
            unsigned long MTime, ATime;
            if (swscanf(Line.c_str(), L"%ld %*d %ld %*d",  &MTime, &ATime) == 2)
            {
              FileData.Modification = UnixToDateTime(MTime, FTerminal->SessionData->DSTMode);
              FSecureShell->SendNull();
              // File time is only valid until next pass
              FileData.SetTime = 2;
              continue;
            }
              else
            {
              SCPError(LoadStr(SCP_ILLEGAL_TIME_FORMAT), False);
            }

          case L'C':
          case L'D':
            break; // continue pass switch{}

          default:
            FTerminal->FatalError(NULL, FMTLOAD(SCP_INVALID_CONTROL_RECORD, (Ctrl, Line)));
        }

        TFileMasks::TParams MaskParams;
        MaskParams.Modification = FileData.Modification;

        // We reach this point only if control record was 'C' or 'D'
        try
        {
          FileData.RemoteRights.Octal = CutToChar(Line, L' ', True);
          // do not trim leading spaces of the filename
          __int64 TSize = StrToInt64(CutToChar(Line, L' ', False).TrimRight());
          MaskParams.Size = TSize;
          // Security fix: ensure the file ends up where we asked for it.
          // (accept only filename, not path)
          UnicodeString OnlyFileName = UnixExtractFileName(Line);
          if (Line != OnlyFileName)
          {
            FTerminal->LogEvent(FORMAT(L"Warning: Remote host set a compound pathname '%s'", (Line)));
          }

          FullFileName = SourceDir + OnlyFileName;
          OperationProgress->SetFile(FullFileName);
          OperationProgress->SetTransferSize(TSize);
        }
        catch (Exception &E)
        {
          {
            TSuspendFileOperationProgress Suspend(OperationProgress);
            FTerminal->Log->AddException(&E);
          }
          SCPError(LoadStr(SCP_ILLEGAL_FILE_DESCRIPTOR), false);
        }

        // last possibility to cancel transfer before it starts
        if (OperationProgress->Cancel)
        {
          throw ESkipFile(NULL, MainInstructions(LoadStr(USER_TERMINATED)));
        }

        bool Dir = (Ctrl == L'D');
        UnicodeString BaseFileName = FTerminal->GetBaseFileName(FullFileName);
        if (!CopyParam->AllowTransfer(BaseFileName, osRemote, Dir, MaskParams, IsUnixHiddenFile(BaseFileName)))
        {
          FTerminal->LogEvent(FORMAT(L"File \"%s\" excluded from transfer",
            (FullFileName)));
          SkipConfirmed = true;
          SCPError(L"", false);
        }

        if (CopyParam->SkipTransfer(FullFileName, Dir))
        {
          SkipConfirmed = true;
          SCPError(L"", false);
          OperationProgress->AddSkippedFileSize(MaskParams.Size);
        }

        FTerminal->LogFileDetails(FileName, FileData.Modification, MaskParams.Size);

        UnicodeString DestFileNameOnly =
          FTerminal->ChangeFileName(
            CopyParam, OperationProgress->FileName, osRemote,
            Level == 0);
        UnicodeString DestFileName =
          IncludeTrailingBackslash(TargetDir) + DestFileNameOnly;

        FileData.Attrs = FileGetAttrFix(ApiPath(DestFileName));
        // If getting attrs fails, we suppose, that file/folder doesn't exists
        FileData.Exists = (FileData.Attrs != -1);
        if (Dir)
        {
          if (FileData.Exists && !(FileData.Attrs & faDirectory))
          {
            SCPError(FMTLOAD(NOT_DIRECTORY_ERROR, (DestFileName)), false);
          }

          if (!FileData.Exists)
          {
            FILE_OPERATION_LOOP_BEGIN
            {
              THROWOSIFFALSE(ForceDirectories(ApiPath(DestFileName)));
            }
            FILE_OPERATION_LOOP_END(FMTLOAD(CREATE_DIR_ERROR, (DestFileName)));
            /* SCP: can we set the timestamp for directories ? */
          }
          UnicodeString FullFileName = SourceDir + OperationProgress->FileName;
          SCPSink(DestFileName, FullFileName, UnixIncludeTrailingBackslash(FullFileName),
            CopyParam, Success, OperationProgress, Params, Level + 1);
          continue;
        }
        else if (Ctrl == L'C')
        {
          TDownloadSessionAction Action(FTerminal->ActionLog);
          Action.FileName(FTerminal->AbsolutePath(FullFileName, true));

          try
          {
            HANDLE File = NULL;
            TStream * FileStream = NULL;

            /* TODO 1 : Turn off read-only attr */

            try
            {
              try
              {
                if (FileExists(ApiPath(DestFileName)))
                {
                  __int64 MTime;
                  TOverwriteFileParams FileParams;
                  FileParams.SourceSize = OperationProgress->TransferSize;
                  FileParams.SourceTimestamp = FileData.Modification;
                  FTerminal->OpenLocalFile(DestFileName, GENERIC_READ,
                    NULL, NULL, NULL, &MTime, NULL,
                    &FileParams.DestSize);
                  FileParams.DestTimestamp = UnixToDateTime(MTime,
                    FTerminal->SessionData->DSTMode);

                  unsigned int Answer =
                    ConfirmOverwrite(OperationProgress->FileName, DestFileNameOnly, osLocal,
                      &FileParams, CopyParam, Params, OperationProgress);

                  switch (Answer)
                  {
                    case qaCancel:
                      OperationProgress->SetCancel(csCancel); // continue on next case
                    case qaNo:
                      SkipConfirmed = true;
                      EXCEPTION;
                  }
                }

                Action.Destination(DestFileName);

                if (!FTerminal->CreateLocalFile(DestFileName, OperationProgress,
                       &File, FLAGSET(Params, cpNoConfirmation)))
                {
                  SkipConfirmed = true;
                  EXCEPTION;
                }

                FileStream = new TSafeHandleStream((THandle)File);
              }
              catch (Exception &E)
              {
                // In this step we can still cancel transfer, so we do it
                SCPError(E.Message, false);
                throw;
              }

              // We succeeded, so we confirm transfer to remote side
              FSecureShell->SendNull();
              // From now we need to finish file transfer, if not it's fatal error
              OperationProgress->SetTransferringFile(true);

              // Suppose same data size to transfer as to write
              // (not true with ASCII transfer)
              OperationProgress->SetLocalSize(OperationProgress->TransferSize);

              // Will we use ASCII of BINARY file transfer?
              OperationProgress->SetAsciiTransfer(
                CopyParam->UseAsciiTransfer(BaseFileName, osRemote, MaskParams));
              if (FTerminal->Configuration->ActualLogProtocol >= 0)
              {
                FTerminal->LogEvent(UnicodeString((OperationProgress->AsciiTransfer ? L"Ascii" : L"Binary")) +
                  L" transfer mode selected.");
              }

              try
              {
                // Buffer for one block of data
                TFileBuffer BlockBuf;
                bool ConvertToken = false;

                do
                {
                  BlockBuf.Size = OperationProgress->TransferBlockSize();
                  BlockBuf.Position = 0;

                  FSecureShell->Receive(reinterpret_cast<unsigned char *>(BlockBuf.Data), BlockBuf.Size);
                  OperationProgress->AddTransferred(BlockBuf.Size);

                  if (OperationProgress->AsciiTransfer)
                  {
                    unsigned int PrevBlockSize = BlockBuf.Size;
                    BlockBuf.Convert(FTerminal->SessionData->EOLType,
                      FTerminal->Configuration->LocalEOLType, 0, ConvertToken);
                    OperationProgress->SetLocalSize(
                      OperationProgress->LocalSize - PrevBlockSize + BlockBuf.Size);
                  }

                  // This is crucial, if it fails during file transfer, it's fatal error
                  FILE_OPERATION_LOOP_BEGIN
                  {
                    BlockBuf.WriteToStream(FileStream, BlockBuf.Size);
                  }
                  FILE_OPERATION_LOOP_END_EX(FMTLOAD(WRITE_ERROR, (DestFileName)), folNone);

                  OperationProgress->AddLocallyUsed(BlockBuf.Size);

                  if (OperationProgress->Cancel == csCancelTransfer)
                  {
                    throw Exception(MainInstructions(LoadStr(USER_TERMINATED)));
                  }
                }
                while (!OperationProgress->IsLocallyDone() || !
                    OperationProgress->IsTransferDone());
              }
              catch (Exception &E)
              {
                // Every exception during file transfer is fatal
                FTerminal->FatalError(&E,
                  FMTLOAD(COPY_FATAL, (OperationProgress->FileName)));
              }

              OperationProgress->SetTransferringFile(false);

              try
              {
                SCPResponse();
                // If one of following exception occurs, we still need
                // to send confirmation to other side
              }
              catch (EScp &E)
              {
                FSecureShell->SendNull();
                throw;
              }
              catch (EScpFileSkipped &E)
              {
                FSecureShell->SendNull();
                throw;
              }

              FSecureShell->SendNull();

              if (FileData.SetTime && CopyParam->PreserveTime)
              {
                FTerminal->UpdateTargetTime(File, FileData.Modification, FTerminal->SessionData->DSTMode);
              }
            }
            __finally
            {
              if (File) CloseHandle(File);
              if (FileStream) delete FileStream;
            }
          }
          catch(Exception & E)
          {
            if (SkipConfirmed)
            {
              Action.Cancel();
            }
            else
            {
              FTerminal->RollbackAction(Action, OperationProgress, &E);
            }
            throw;
          }

          if (FileData.Attrs == -1) FileData.Attrs = faArchive;
          int NewAttrs = CopyParam->LocalFileAttrs(FileData.RemoteRights);
          if ((NewAttrs & FileData.Attrs) != NewAttrs)
          {
            FILE_OPERATION_LOOP_BEGIN
            {
              THROWOSIFFALSE(FileSetAttr(ApiPath(DestFileName), FileData.Attrs | NewAttrs) == 0);
            }
            FILE_OPERATION_LOOP_END(FMTLOAD(CANT_SET_ATTRS, (DestFileName)));
          }

          FTerminal->LogFileDone(OperationProgress, DestFileName);
        }
      }
    }
    catch (EScpFileSkipped &E)
    {
      if (!SkipConfirmed)
      {
        TSuspendFileOperationProgress Suspend(OperationProgress);
        TQueryParams Params(qpAllowContinueOnError);
        if (FTerminal->QueryUserException(FMTLOAD(COPY_ERROR, (FullFileName)),
              &E, qaOK | qaAbort, &Params, qtError) == qaAbort)
        {
          OperationProgress->SetCancel(csCancel);
        }
        FTerminal->Log->AddException(&E);
      }
      // this was inside above condition, but then transfer was considered
      // successful, even when for example user refused to overwrite file
      Success = false;
    }
    catch (ESkipFile &E)
    {
      SCPSendError(E.Message, false);
      Success = false;
      if (!FTerminal->HandleException(&E)) throw;
    }
  }
}