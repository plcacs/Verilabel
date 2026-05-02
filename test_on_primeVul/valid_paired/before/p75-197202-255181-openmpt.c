std::vector<GetLengthType> CSoundFile::GetLength(enmGetLengthResetMode adjustMode, GetLengthTarget target)
{
	std::vector<GetLengthType> results;
	GetLengthType retval;
	retval.startOrder = target.startOrder;
	retval.startRow = target.startRow;

	// Are we trying to reach a certain pattern position?
	const bool hasSearchTarget = target.mode != GetLengthTarget::NoTarget;
	const bool adjustSamplePos = (adjustMode & eAdjustSamplePositions) == eAdjustSamplePositions;

	SEQUENCEINDEX sequence = target.sequence;
	if(sequence >= Order.GetNumSequences()) sequence = Order.GetCurrentSequenceIndex();
	const ModSequence &orderList = Order(sequence);

	GetLengthMemory memory(*this);
	CSoundFile::PlayState &playState = *memory.state;
	// Temporary visited rows vector (so that GetLength() won't interfere with the player code if the module is playing at the same time)
	RowVisitor visitedRows(*this, sequence);

	playState.m_nNextRow = playState.m_nRow = target.startRow;
	playState.m_nNextOrder = playState.m_nCurrentOrder = target.startOrder;

	// Fast LUTs for commands that are too weird / complicated / whatever to emulate in sample position adjust mode.
	std::bitset<MAX_EFFECTS> forbiddenCommands;
	std::bitset<MAX_VOLCMDS> forbiddenVolCommands;

	if(adjustSamplePos)
	{
		forbiddenCommands.set(CMD_ARPEGGIO);             forbiddenCommands.set(CMD_PORTAMENTOUP);
		forbiddenCommands.set(CMD_PORTAMENTODOWN);       forbiddenCommands.set(CMD_XFINEPORTAUPDOWN);
		forbiddenCommands.set(CMD_NOTESLIDEUP);          forbiddenCommands.set(CMD_NOTESLIDEUPRETRIG);
		forbiddenCommands.set(CMD_NOTESLIDEDOWN);        forbiddenCommands.set(CMD_NOTESLIDEDOWNRETRIG);
		forbiddenVolCommands.set(VOLCMD_PORTAUP);        forbiddenVolCommands.set(VOLCMD_PORTADOWN);

		// Optimize away channels for which it's pointless to adjust sample positions
		for(CHANNELINDEX i = 0; i < GetNumChannels(); i++)
		{
			if(ChnSettings[i].dwFlags[CHN_MUTE]) memory.chnSettings[i].ticksToRender = GetLengthMemory::IGNORE_CHANNEL;
		}
		if(target.mode == GetLengthTarget::SeekPosition && target.pos.order < orderList.size())
		{
			// If we know where to seek, we can directly rule out any channels on which a new note would be triggered right at the start.
			const PATTERNINDEX seekPat = orderList[target.pos.order];
			if(Patterns.IsValidPat(seekPat) && Patterns[seekPat].IsValidRow(target.pos.row))
			{
				const ModCommand *m = Patterns[seekPat].GetRow(target.pos.row);
				for(CHANNELINDEX i = 0; i < GetNumChannels(); i++, m++)
				{
					if(m->note == NOTE_NOTECUT || m->note == NOTE_KEYOFF || (m->note == NOTE_FADE && GetNumInstruments())
						|| (m->IsNote() && !m->IsPortamento()))
					{
						memory.chnSettings[i].ticksToRender = GetLengthMemory::IGNORE_CHANNEL;
					}
				}
			}
		}
	}

	// If samples are being synced, force them to resync if tick duration changes
	uint32 oldTickDuration = 0;

	for (;;)
	{
		// Time target reached.
		if(target.mode == GetLengthTarget::SeekSeconds && memory.elapsedTime >= target.time)
		{
			retval.targetReached = true;
			break;
		}

		uint32 rowDelay = 0, tickDelay = 0;
		playState.m_nRow = playState.m_nNextRow;
		playState.m_nCurrentOrder = playState.m_nNextOrder;

		if(orderList.IsValidPat(playState.m_nCurrentOrder) && playState.m_nRow >= Patterns[orderList[playState.m_nCurrentOrder]].GetNumRows())
		{
			playState.m_nRow = 0;
			if(m_playBehaviour[kFT2LoopE60Restart])
			{
				playState.m_nRow = playState.m_nNextPatStartRow;
				playState.m_nNextPatStartRow = 0;
			}
			playState.m_nCurrentOrder = ++playState.m_nNextOrder;
		}

		// Check if pattern is valid
		playState.m_nPattern = playState.m_nCurrentOrder < orderList.size() ? orderList[playState.m_nCurrentOrder] : orderList.GetInvalidPatIndex();
		bool positionJumpOnThisRow = false;
		bool patternBreakOnThisRow = false;
		bool patternLoopEndedOnThisRow = false, patternLoopStartedOnThisRow = false;

		if(!Patterns.IsValidPat(playState.m_nPattern) && playState.m_nPattern != orderList.GetInvalidPatIndex() && target.mode == GetLengthTarget::SeekPosition && playState.m_nCurrentOrder == target.pos.order)
		{
			// Early test: Target is inside +++ or non-existing pattern
			retval.targetReached = true;
			break;
		}

		while(playState.m_nPattern >= Patterns.Size())
		{
			// End of song?
			if((playState.m_nPattern == orderList.GetInvalidPatIndex()) || (playState.m_nCurrentOrder >= orderList.size()))
			{
				if(playState.m_nCurrentOrder == orderList.GetRestartPos())
					break;
				else
					playState.m_nCurrentOrder = orderList.GetRestartPos();
			} else
			{
				playState.m_nCurrentOrder++;
			}
			playState.m_nPattern = (playState.m_nCurrentOrder < orderList.size()) ? orderList[playState.m_nCurrentOrder] : orderList.GetInvalidPatIndex();
			playState.m_nNextOrder = playState.m_nCurrentOrder;
			if((!Patterns.IsValidPat(playState.m_nPattern)) && visitedRows.IsVisited(playState.m_nCurrentOrder, 0, true))
			{
				if(!hasSearchTarget || !visitedRows.GetFirstUnvisitedRow(playState.m_nNextOrder, playState.m_nRow, true))
				{
					// We aren't searching for a specific row, or we couldn't find any more unvisited rows.
					break;
				} else
				{
					// We haven't found the target row yet, but we found some other unplayed row... continue searching from here.
					retval.duration = memory.elapsedTime;
					results.push_back(retval);
					retval.startRow = playState.m_nRow;
					retval.startOrder = playState.m_nNextOrder;
					memory.Reset();

					playState.m_nCurrentOrder = playState.m_nNextOrder;
					playState.m_nPattern = orderList[playState.m_nCurrentOrder];
					playState.m_nNextRow = playState.m_nRow;
					break;
				}
			}
		}
		if(playState.m_nNextOrder == ORDERINDEX_INVALID)
		{
			// GetFirstUnvisitedRow failed, so there is nothing more to play
			break;
		}

		// Skip non-existing patterns
		if(!Patterns.IsValidPat(playState.m_nPattern))
		{
			// If there isn't even a tune, we should probably stop here.
			if(playState.m_nCurrentOrder == orderList.GetRestartPos())
			{
				if(!hasSearchTarget || !visitedRows.GetFirstUnvisitedRow(playState.m_nNextOrder, playState.m_nRow, true))
				{
					// We aren't searching for a specific row, or we couldn't find any more unvisited rows.
					break;
				} else
				{
					// We haven't found the target row yet, but we found some other unplayed row... continue searching from here.
					retval.duration = memory.elapsedTime;
					results.push_back(retval);
					retval.startRow = playState.m_nRow;
					retval.startOrder = playState.m_nNextOrder;
					memory.Reset();
					playState.m_nNextRow = playState.m_nRow;
					continue;
				}
			}
			playState.m_nNextOrder = playState.m_nCurrentOrder + 1;
			continue;
		}
		// Should never happen
		if(playState.m_nRow >= Patterns[playState.m_nPattern].GetNumRows())
			playState.m_nRow = 0;

		// Check whether target was reached.
		if(target.mode == GetLengthTarget::SeekPosition && playState.m_nCurrentOrder == target.pos.order && playState.m_nRow == target.pos.row)
		{
			retval.targetReached = true;
			break;
		}

		if(visitedRows.IsVisited(playState.m_nCurrentOrder, playState.m_nRow, true))
		{
			if(!hasSearchTarget || !visitedRows.GetFirstUnvisitedRow(playState.m_nNextOrder, playState.m_nRow, true))
			{
				// We aren't searching for a specific row, or we couldn't find any more unvisited rows.
				break;
			} else
			{
				// We haven't found the target row yet, but we found some other unplayed row... continue searching from here.
				retval.duration = memory.elapsedTime;
				results.push_back(retval);
				retval.startRow = playState.m_nRow;
				retval.startOrder = playState.m_nNextOrder;
				memory.Reset();
				playState.m_nNextRow = playState.m_nRow;
				continue;
			}
		}

		retval.endOrder = playState.m_nCurrentOrder;
		retval.endRow = playState.m_nRow;

		// Update next position
		playState.m_nNextRow = playState.m_nRow + 1;

		// Jumped to invalid pattern row?
		if(playState.m_nRow >= Patterns[playState.m_nPattern].GetNumRows())
		{
			playState.m_nRow = 0;
		}
		// New pattern?
		if(!playState.m_nRow)
		{
			for(CHANNELINDEX chn = 0; chn < GetNumChannels(); chn++)
			{
				memory.chnSettings[chn].patLoop = memory.elapsedTime;
				memory.chnSettings[chn].patLoopSmp = playState.m_lTotalSampleCount;
			}
		}

		ModChannel *pChn = playState.Chn;
		
		// For various effects, we need to know first how many ticks there are in this row.
		const ModCommand *p = Patterns[playState.m_nPattern].GetpModCommand(playState.m_nRow, 0);
		for(CHANNELINDEX nChn = 0; nChn < GetNumChannels(); nChn++, p++)
		{
			if(m_playBehaviour[kST3NoMutedChannels] && ChnSettings[nChn].dwFlags[CHN_MUTE])	// not even effects are processed on muted S3M channels
				continue;
			if(p->IsPcNote())
			{
#ifndef NO_PLUGINS
				if((adjustMode & eAdjust) && p->instr > 0 && p->instr <= MAX_MIXPLUGINS)
				{
					memory.plugParams[std::make_pair(p->instr, p->GetValueVolCol())] = p->GetValueEffectCol();
				}
#endif // NO_PLUGINS
				pChn[nChn].rowCommand.Clear();
				continue;
			}
			pChn[nChn].rowCommand = *p;
			switch(p->command)
			{
			case CMD_SPEED:
				SetSpeed(playState, p->param);
				break;

			case CMD_TEMPO:
				if(m_playBehaviour[kMODVBlankTiming])
				{
					// ProTracker MODs with VBlank timing: All Fxx parameters set the tick count.
					if(p->param != 0) SetSpeed(playState, p->param);
				}
				break;

			case CMD_S3MCMDEX:
				if((p->param & 0xF0) == 0x60)
				{
					// Fine Pattern Delay
					tickDelay += (p->param & 0x0F);
				} else if((p->param & 0xF0) == 0xE0 && !rowDelay)
				{
					// Pattern Delay
					if(!(GetType() & MOD_TYPE_S3M) || (p->param & 0x0F) != 0)
					{
						// While Impulse Tracker *does* count S60 as a valid row delay (and thus ignores any other row delay commands on the right),
						// Scream Tracker 3 simply ignores such commands.
						rowDelay = 1 + (p->param & 0x0F);
					}
				}
				break;

			case CMD_MODCMDEX:
				if((p->param & 0xF0) == 0xE0)
				{
					// Pattern Delay
					rowDelay = 1 + (p->param & 0x0F);
				}
				break;
			}
		}
		if(rowDelay == 0) rowDelay = 1;
		const uint32 numTicks = (playState.m_nMusicSpeed + tickDelay) * rowDelay;
		const uint32 nonRowTicks = numTicks - rowDelay;

		for(CHANNELINDEX nChn = 0; nChn < GetNumChannels(); pChn++, nChn++) if(!pChn->rowCommand.IsEmpty())
		{
			if(m_playBehaviour[kST3NoMutedChannels] && ChnSettings[nChn].dwFlags[CHN_MUTE])	// not even effects are processed on muted S3M channels
				continue;
			ModCommand::COMMAND command = pChn->rowCommand.command;
			ModCommand::PARAM param = pChn->rowCommand.param;
			ModCommand::NOTE note = pChn->rowCommand.note;

			if (pChn->rowCommand.instr)
			{
				pChn->nNewIns = pChn->rowCommand.instr;
				pChn->nLastNote = NOTE_NONE;
				memory.chnSettings[nChn].vol = 0xFF;
			}
			if (pChn->rowCommand.IsNote()) pChn->nLastNote = note;

			// Update channel panning
			if(pChn->rowCommand.IsNote() || pChn->rowCommand.instr)
			{
				SAMPLEINDEX smp = 0;
				if(GetNumInstruments())
				{
					ModInstrument *pIns;
					if(pChn->nNewIns <= GetNumInstruments() && (pIns = Instruments[pChn->nNewIns]) != nullptr)
					{
						if(pIns->dwFlags[INS_SETPANNING])
							pChn->nPan = pIns->nPan;
						if(ModCommand::IsNote(note))
							smp = pIns->Keyboard[note - NOTE_MIN];
					}
				} else
				{
					smp = pChn->nNewIns;
				}
				if(smp > 0 && smp <= GetNumSamples() && Samples[smp].uFlags[CHN_PANNING])
				{
					pChn->nPan = Samples[smp].nPan;
				}
			}

			switch(pChn->rowCommand.volcmd)
			{
			case VOLCMD_VOLUME:
				memory.chnSettings[nChn].vol = pChn->rowCommand.vol;
				break;
			case VOLCMD_VOLSLIDEUP:
			case VOLCMD_VOLSLIDEDOWN:
				if(pChn->rowCommand.vol != 0)
					pChn->nOldVolParam = pChn->rowCommand.vol;
				break;
			}

			switch(command)
			{
			// Position Jump
			case CMD_POSITIONJUMP:
				positionJumpOnThisRow = true;
				playState.m_nNextOrder = static_cast<ORDERINDEX>(CalculateXParam(playState.m_nPattern, playState.m_nRow, nChn));
				playState.m_nNextPatStartRow = 0;  // FT2 E60 bug
				// see https://forum.openmpt.org/index.php?topic=2769.0 - FastTracker resets Dxx if Bxx is called _after_ Dxx
				// Test case: PatternJump.mod
				if(!patternBreakOnThisRow || (GetType() & (MOD_TYPE_MOD | MOD_TYPE_XM)))
					playState.m_nNextRow = 0;

				if (adjustMode & eAdjust)
				{
					pChn->nPatternLoopCount = 0;
					pChn->nPatternLoop = 0;
				}
				break;
			// Pattern Break
			case CMD_PATTERNBREAK:
				{
					ROWINDEX row = PatternBreak(playState, nChn, param);
					if(row != ROWINDEX_INVALID)
					{
						patternBreakOnThisRow = true;
						playState.m_nNextRow = row;

						if(!positionJumpOnThisRow)
						{
							playState.m_nNextOrder = playState.m_nCurrentOrder + 1;
						}
						if(adjustMode & eAdjust)
						{
							pChn->nPatternLoopCount = 0;
							pChn->nPatternLoop = 0;
						}
					}
				}
				break;
			// Set Tempo
			case CMD_TEMPO:
				if(!m_playBehaviour[kMODVBlankTiming])
				{
					TEMPO tempo(CalculateXParam(playState.m_nPattern, playState.m_nRow, nChn), 0);
					if ((adjustMode & eAdjust) && (GetType() & (MOD_TYPE_S3M | MOD_TYPE_IT | MOD_TYPE_MPT)))
					{
						if (tempo.GetInt()) pChn->nOldTempo = static_cast<uint8>(tempo.GetInt()); else tempo.Set(pChn->nOldTempo);
					}

					if (tempo.GetInt() >= 0x20) playState.m_nMusicTempo = tempo;
					else
					{
						// Tempo Slide
						TEMPO tempoDiff((tempo.GetInt() & 0x0F) * nonRowTicks, 0);
						if ((tempo.GetInt() & 0xF0) == 0x10)
						{
							playState.m_nMusicTempo += tempoDiff;
						} else
						{
							if(tempoDiff < playState.m_nMusicTempo)
								playState.m_nMusicTempo -= tempoDiff;
							else
								playState.m_nMusicTempo.Set(0);
						}
					}

					TEMPO tempoMin = GetModSpecifications().GetTempoMin(), tempoMax = GetModSpecifications().GetTempoMax();
					if(m_playBehaviour[kTempoClamp])	// clamp tempo correctly in compatible mode
					{
						tempoMax.Set(255);
					}
					Limit(playState.m_nMusicTempo, tempoMin, tempoMax);
				}
				break;

			case CMD_S3MCMDEX:
				switch(param & 0xF0)
				{
				case 0x90:
					if(param <= 0x91)
					{
						pChn->dwFlags.set(CHN_SURROUND, param == 0x91);
					}
					break;

				case 0xA0:
					// High sample offset
					pChn->nOldHiOffset = param & 0x0F;
					break;
				
				case 0xB0:
					// Pattern Loop
					if (param & 0x0F)
					{
						patternLoopEndedOnThisRow = true;
					} else
					{
						CHANNELINDEX firstChn = nChn, lastChn = nChn;
						if(GetType() == MOD_TYPE_S3M)
						{
							// ST3 has only one global loop memory.
							firstChn = 0;
							lastChn = GetNumChannels() - 1;
						}
						for(CHANNELINDEX c = firstChn; c <= lastChn; c++)
						{
							memory.chnSettings[c].patLoop = memory.elapsedTime;
							memory.chnSettings[c].patLoopSmp = playState.m_lTotalSampleCount;
							memory.chnSettings[c].patLoopStart = playState.m_nRow;
						}
						patternLoopStartedOnThisRow = true;
					}
					break;

				case 0xF0:
					// Active macro
					pChn->nActiveMacro = param & 0x0F;
					break;
				}
				break;

			case CMD_MODCMDEX:
				switch(param & 0xF0)
				{
				case 0x60:
					// Pattern Loop
					if (param & 0x0F)
					{
						playState.m_nNextPatStartRow = memory.chnSettings[nChn].patLoopStart; // FT2 E60 bug
						patternLoopEndedOnThisRow = true;
					} else
					{
						patternLoopStartedOnThisRow = true;
						memory.chnSettings[nChn].patLoop = memory.elapsedTime;
						memory.chnSettings[nChn].patLoopSmp = playState.m_lTotalSampleCount;
						memory.chnSettings[nChn].patLoopStart = playState.m_nRow;
					}
					break;

				case 0xF0:
					// Active macro
					pChn->nActiveMacro = param & 0x0F;
					break;
				}
				break;

			case CMD_XFINEPORTAUPDOWN:
				// ignore high offset in compatible mode
				if(((param & 0xF0) == 0xA0) && !m_playBehaviour[kFT2RestrictXCommand]) pChn->nOldHiOffset = param & 0x0F;
				break;
			}

			// The following calculations are not interesting if we just want to get the song length.
			if (!(adjustMode & eAdjust)) continue;
			switch(command)
			{
			// Portamento Up/Down
			case CMD_PORTAMENTOUP:
				if(param)
				{
					// FT2 compatibility: Separate effect memory for all portamento commands
					// Test case: Porta-LinkMem.xm
					if(!m_playBehaviour[kFT2PortaUpDownMemory])
						pChn->nOldPortaDown = param;
					pChn->nOldPortaUp = param;
				}
				break;
			case CMD_PORTAMENTODOWN:
				if(param)
				{
					// FT2 compatibility: Separate effect memory for all portamento commands
					// Test case: Porta-LinkMem.xm
					if(!m_playBehaviour[kFT2PortaUpDownMemory])
						pChn->nOldPortaUp = param;
					pChn->nOldPortaDown = param;
				}
				break;
			// Tone-Portamento
			case CMD_TONEPORTAMENTO:
				if (param) pChn->nPortamentoSlide = param << 2;
				break;
			// Offset
			case CMD_OFFSET:
				if (param) pChn->oldOffset = param << 8;
				break;
			// Volume Slide
			case CMD_VOLUMESLIDE:
			case CMD_TONEPORTAVOL:
				if (param) pChn->nOldVolumeSlide = param;
				break;
			// Set Volume
			case CMD_VOLUME:
				memory.chnSettings[nChn].vol = param;
				break;
			// Global Volume
			case CMD_GLOBALVOLUME:
				if(!(GetType() & GLOBALVOL_7BIT_FORMATS) && param < 128) param *= 2;
				// IT compatibility 16. ST3 and IT ignore out-of-range values
				if(param <= 128)
				{
					playState.m_nGlobalVolume = param * 2;
				} else if(!(GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_S3M)))
				{
					playState.m_nGlobalVolume = 256;
				}
				break;
			// Global Volume Slide
			case CMD_GLOBALVOLSLIDE:
				if(m_playBehaviour[kPerChannelGlobalVolSlide])
				{
					// IT compatibility 16. Global volume slide params are stored per channel (FT2/IT)
					if (param) pChn->nOldGlobalVolSlide = param; else param = pChn->nOldGlobalVolSlide;
				} else
				{
					if (param) playState.Chn[0].nOldGlobalVolSlide = param; else param = playState.Chn[0].nOldGlobalVolSlide;
				}
				if (((param & 0x0F) == 0x0F) && (param & 0xF0))
				{
					param >>= 4;
					if (!(GetType() & GLOBALVOL_7BIT_FORMATS)) param <<= 1;
					playState.m_nGlobalVolume += param << 1;
				} else if (((param & 0xF0) == 0xF0) && (param & 0x0F))
				{
					param = (param & 0x0F) << 1;
					if (!(GetType() & GLOBALVOL_7BIT_FORMATS)) param <<= 1;
					playState.m_nGlobalVolume -= param;
				} else if (param & 0xF0)
				{
					param >>= 4;
					param <<= 1;
					if (!(GetType() & GLOBALVOL_7BIT_FORMATS)) param <<= 1;
					playState.m_nGlobalVolume += param * nonRowTicks;
				} else
				{
					param = (param & 0x0F) << 1;
					if (!(GetType() & GLOBALVOL_7BIT_FORMATS)) param <<= 1;
					playState.m_nGlobalVolume -= param * nonRowTicks;
				}
				Limit(playState.m_nGlobalVolume, 0, 256);
				break;
			case CMD_CHANNELVOLUME:
				if (param <= 64) pChn->nGlobalVol = param;
				break;
			case CMD_CHANNELVOLSLIDE:
				{
					if (param) pChn->nOldChnVolSlide = param; else param = pChn->nOldChnVolSlide;
					int32 volume = pChn->nGlobalVol;
					if((param & 0x0F) == 0x0F && (param & 0xF0))
						volume += (param >> 4);		// Fine Up
					else if((param & 0xF0) == 0xF0 && (param & 0x0F))
						volume -= (param & 0x0F);	// Fine Down
					else if(param & 0x0F)			// Down
						volume -= (param & 0x0F) * nonRowTicks;
					else							// Up
						volume += ((param & 0xF0) >> 4) * nonRowTicks;
					Limit(volume, 0, 64);
					pChn->nGlobalVol = volume;
				}
				break;
			case CMD_PANNING8:
				Panning(pChn, param, Pan8bit);
				break;
			case CMD_MODCMDEX:
				if(param < 0x10)
				{
					// LED filter
					for(CHANNELINDEX chn = 0; chn < GetNumChannels(); chn++)
					{
						playState.Chn[chn].dwFlags.set(CHN_AMIGAFILTER, !(param & 1));
					}
				}
				MPT_FALLTHROUGH;
			case CMD_S3MCMDEX:
				if((param & 0xF0) == 0x80)
				{
					Panning(pChn, (param & 0x0F), Pan4bit);
				}
				break;

			case CMD_VIBRATOVOL:
				if (param) pChn->nOldVolumeSlide = param;
				param = 0;
				MPT_FALLTHROUGH;
			case CMD_VIBRATO:
				Vibrato(pChn, param);
				break;
			case CMD_FINEVIBRATO:
				FineVibrato(pChn, param);
				break;
			case CMD_TREMOLO:
				Tremolo(pChn, param);
				break;
			case CMD_PANBRELLO:
				Panbrello(pChn, param);
				break;
			}

			switch(pChn->rowCommand.volcmd)
			{
			case VOLCMD_PANNING:
				Panning(pChn, pChn->rowCommand.vol, Pan6bit);
				break;

			case VOLCMD_VIBRATOSPEED:
				// FT2 does not automatically enable vibrato with the "set vibrato speed" command
				if(m_playBehaviour[kFT2VolColVibrato])
					pChn->nVibratoSpeed = pChn->rowCommand.vol & 0x0F;
				else
					Vibrato(pChn, pChn->rowCommand.vol << 4);
				break;
			case VOLCMD_VIBRATODEPTH:
				Vibrato(pChn, pChn->rowCommand.vol);
				break;
			}

			// Process vibrato / tremolo / panbrello
			switch(pChn->rowCommand.command)
			{
			case CMD_VIBRATO:
			case CMD_FINEVIBRATO:
			case CMD_VIBRATOVOL:
				if(adjustMode & eAdjust)
				{
					uint32 vibTicks = ((GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) && !m_SongFlags[SONG_ITOLDEFFECTS]) ? numTicks : nonRowTicks;
					uint32 inc = pChn->nVibratoSpeed * vibTicks;
					if(m_playBehaviour[kITVibratoTremoloPanbrello])
						inc *= 4;
					pChn->nVibratoPos += static_cast<uint8>(inc);
				}
				break;

			case CMD_TREMOLO:
				if(adjustMode & eAdjust)
				{
					uint32 tremTicks = ((GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) && !m_SongFlags[SONG_ITOLDEFFECTS]) ? numTicks : nonRowTicks;
					uint32 inc = pChn->nTremoloSpeed * tremTicks;
					if(m_playBehaviour[kITVibratoTremoloPanbrello])
						inc *= 4;
					pChn->nTremoloPos += static_cast<uint8>(inc);
				}
				break;

			case CMD_PANBRELLO:
				if(adjustMode & eAdjust)
				{
					// Panbrello effect is permanent in compatible mode, so actually apply panbrello for the last tick of this row
					pChn->nPanbrelloPos += static_cast<uint8>(pChn->nPanbrelloSpeed * (numTicks - 1));
					ProcessPanbrello(pChn);
				}
				break;
			}
		}

		// Interpret F00 effect in XM files as "stop song"
		if(GetType() == MOD_TYPE_XM && playState.m_nMusicSpeed == uint16_max)
		{
			break;
		}

		playState.m_nCurrentRowsPerBeat = m_nDefaultRowsPerBeat;
		if(Patterns[playState.m_nPattern].GetOverrideSignature())
		{
			playState.m_nCurrentRowsPerBeat = Patterns[playState.m_nPattern].GetRowsPerBeat();
		}

		const uint32 tickDuration = GetTickDuration(playState);
		const uint32 rowDuration = tickDuration * numTicks;
		memory.elapsedTime += static_cast<double>(rowDuration) / static_cast<double>(m_MixerSettings.gdwMixingFreq);
		playState.m_lTotalSampleCount += rowDuration;

		if(adjustSamplePos)
		{
			// Super experimental and dirty sample seeking
			pChn = playState.Chn;
			for(CHANNELINDEX nChn = 0; nChn < GetNumChannels(); pChn++, nChn++)
			{
				if(memory.chnSettings[nChn].ticksToRender == GetLengthMemory::IGNORE_CHANNEL)
					continue;

				uint32 startTick = 0;
				const ModCommand &m = pChn->rowCommand;
				uint32 paramHi = m.param >> 4, paramLo = m.param & 0x0F;
				bool porta = m.command == CMD_TONEPORTAMENTO || m.command == CMD_TONEPORTAVOL || m.volcmd == VOLCMD_TONEPORTAMENTO;
				bool stopNote = patternLoopStartedOnThisRow;	// It's too much trouble to keep those pattern loops in sync...

				if(m.instr) pChn->proTrackerOffset = 0;
				if(m.IsNote())
				{
					if(porta && memory.chnSettings[nChn].incChanged)
					{
						// If there's a portamento, the current channel increment mustn't be 0 in NoteChange()
						pChn->increment = GetChannelIncrement(pChn, pChn->nPeriod, 0);
					}
					int32 setPan = pChn->nPan;
					pChn->nNewNote = pChn->nLastNote;
					if(pChn->nNewIns != 0) InstrumentChange(pChn, pChn->nNewIns, porta);
					NoteChange(pChn, m.note, porta);
					memory.chnSettings[nChn].incChanged = true;

					if((m.command == CMD_MODCMDEX || m.command == CMD_S3MCMDEX) && (m.param & 0xF0) == 0xD0 && paramLo < numTicks)
					{
						startTick = paramLo;
					} else if(m.command == CMD_DELAYCUT && paramHi < numTicks)
					{
						startTick = paramHi;
					}
					if(rowDelay > 1 && startTick != 0 && (GetType() & (MOD_TYPE_S3M | MOD_TYPE_IT | MOD_TYPE_MPT)))
					{
						startTick += (playState.m_nMusicSpeed + tickDelay) * (rowDelay - 1);
					}
					if(!porta) memory.chnSettings[nChn].ticksToRender = 0;

					// Panning commands have to be re-applied after a note change with potential pan change.
					if(m.command == CMD_PANNING8
						|| ((m.command == CMD_MODCMDEX || m.command == CMD_S3MCMDEX) && paramHi == 0x8)
						|| m.volcmd == VOLCMD_PANNING)
					{
						pChn->nPan = setPan;
					}

					if(m.command == CMD_OFFSET)
					{
						bool isExtended = false;
						SmpLength offset = CalculateXParam(playState.m_nPattern, playState.m_nRow, nChn, &isExtended);
						if(!isExtended)
						{
							offset <<= 8;
							if(offset == 0) offset = pChn->oldOffset;
							offset += static_cast<SmpLength>(pChn->nOldHiOffset) << 16;
						}
						SampleOffset(*pChn, offset);
					} else if(m.command == CMD_OFFSETPERCENTAGE)
					{
						SampleOffset(*pChn, Util::muldiv_unsigned(pChn->nLength, m.param, 255));
					} else if(m.command == CMD_REVERSEOFFSET && pChn->pModSample != nullptr)
					{
						memory.RenderChannel(nChn, oldTickDuration);	// Re-sync what we've got so far
						ReverseSampleOffset(*pChn, m.param);
						startTick = playState.m_nMusicSpeed - 1;
					} else if(m.volcmd == VOLCMD_OFFSET)
					{
						if(m.vol <= CountOf(pChn->pModSample->cues) && pChn->pModSample != nullptr)
						{
							SmpLength offset;
							if(m.vol == 0)
								offset = pChn->oldOffset;
							else
								offset = pChn->oldOffset = pChn->pModSample->cues[m.vol - 1];
							SampleOffset(*pChn, offset);
						}
					}
				}

				if(m.note == NOTE_KEYOFF || m.note == NOTE_NOTECUT || (m.note == NOTE_FADE && GetNumInstruments())
					|| ((m.command == CMD_MODCMDEX || m.command == CMD_S3MCMDEX) && (m.param & 0xF0) == 0xC0 && paramLo < numTicks)
					|| (m.command == CMD_DELAYCUT && paramLo != 0 && startTick + paramLo < numTicks))
				{
					stopNote = true;
				}

				if(m.command == CMD_VOLUME)
				{
					pChn->nVolume = m.param * 4;
				} else if(m.volcmd == VOLCMD_VOLUME)
				{
					pChn->nVolume = m.vol * 4;
				}
				
				if(pChn->pModSample && !stopNote)
				{
					// Check if we don't want to emulate some effect and thus stop processing.
					if(m.command < MAX_EFFECTS)
					{
						if(forbiddenCommands[m.command])
						{
							stopNote = true;
						} else if(m.command == CMD_MODCMDEX)
						{
							// Special case: Slides using extended commands
							switch(m.param & 0xF0)
							{
							case 0x10:
							case 0x20:
								stopNote = true;
							}
						}
					}

					if(m.volcmd < forbiddenVolCommands.size() && forbiddenVolCommands[m.volcmd])
					{
						stopNote = true;
					}
				}

				if(stopNote)
				{
					pChn->Stop();
					memory.chnSettings[nChn].ticksToRender = 0;
				} else
				{
					if(oldTickDuration != tickDuration && oldTickDuration != 0)
					{
						memory.RenderChannel(nChn, oldTickDuration);	// Re-sync what we've got so far
					}

					switch(m.command)
					{
					case CMD_TONEPORTAVOL:
					case CMD_VOLUMESLIDE:
					case CMD_VIBRATOVOL:
						if(m.param || (GetType() != MOD_TYPE_MOD))
						{
							for(uint32 i = 0; i < numTicks; i++)
							{
								pChn->isFirstTick = (i == 0);
								VolumeSlide(pChn, m.param);
							}
						}
						break;

					case CMD_MODCMDEX:
						if((m.param & 0x0F) || (GetType() & (MOD_TYPE_XM | MOD_TYPE_MT2)))
						{
							pChn->isFirstTick = true;
							switch(m.param & 0xF0)
							{
							case 0xA0: FineVolumeUp(pChn, m.param & 0x0F, false); break;
							case 0xB0: FineVolumeDown(pChn, m.param & 0x0F, false); break;
							}
						}
						break;

					case CMD_S3MCMDEX:
						if(m.param == 0x9E)
						{
							// Play forward
							memory.RenderChannel(nChn, oldTickDuration);	// Re-sync what we've got so far
							pChn->dwFlags.reset(CHN_PINGPONGFLAG);
						} else if(m.param == 0x9F)
						{
							// Reverse
							memory.RenderChannel(nChn, oldTickDuration);	// Re-sync what we've got so far
							pChn->dwFlags.set(CHN_PINGPONGFLAG);
							if(!pChn->position.GetInt() && pChn->nLength && (m.IsNote() || !pChn->dwFlags[CHN_LOOP]))
							{
								pChn->position.Set(pChn->nLength - 1, SamplePosition::fractMax);
							}
						} else if((m.param & 0xF0) == 0x70)
						{
							// TODO
							//ExtendedS3MCommands(nChn, param);
						}
						break;
					}
					pChn->isFirstTick = true;
					switch(m.volcmd)
					{
					case VOLCMD_FINEVOLUP:		FineVolumeUp(pChn, m.vol, m_playBehaviour[kITVolColMemory]); break;
					case VOLCMD_FINEVOLDOWN:	FineVolumeDown(pChn, m.vol, m_playBehaviour[kITVolColMemory]); break;
					case VOLCMD_VOLSLIDEUP:
					case VOLCMD_VOLSLIDEDOWN:
						{
							// IT Compatibility: Volume column volume slides have their own memory
							// Test case: VolColMemory.it
							ModCommand::VOL vol = m.vol;
							if(vol == 0 && m_playBehaviour[kITVolColMemory])
							{
								vol = pChn->nOldVolParam;
								if(vol == 0)
									break;
							}
							if(m.volcmd == VOLCMD_VOLSLIDEUP)
								vol <<= 4;
							for(uint32 i = 0; i < numTicks; i++)
							{
								pChn->isFirstTick = (i == 0);
								VolumeSlide(pChn, vol);
							}
						}
						break;
					}

					if(porta)
					{
						// Portamento needs immediate syncing, as the pitch changes on each tick
						uint32 portaTick = memory.chnSettings[nChn].ticksToRender + startTick + 1;
						memory.chnSettings[nChn].ticksToRender += numTicks;
						memory.RenderChannel(nChn, tickDuration, portaTick);
					} else
					{
						memory.chnSettings[nChn].ticksToRender += (numTicks - startTick);
					}
				}
			}
		}
		oldTickDuration = tickDuration;

		// Pattern loop is not executed in FT2 if there are any position jump or pattern break commands on the same row.
		// Pattern loop is not executed in IT if there are any position jump commands on the same row.
		// Test case for FT2 exception: PatLoop-Jumps.xm, PatLoop-Various.xm
		// Test case for IT: exception: LoopBreak.it
		if(patternLoopEndedOnThisRow
			&& (!m_playBehaviour[kFT2PatternLoopWithJumps] || !(positionJumpOnThisRow || patternBreakOnThisRow))
			&& (!m_playBehaviour[kITPatternLoopWithJumps] || !positionJumpOnThisRow))
		{
			std::map<double, int> startTimes;
			// This is really just a simple estimation for nested pattern loops. It should handle cases correctly where all parallel loops start and end on the same row.
			// If one of them starts or ends "in between", it will most likely calculate a wrong duration.
			// For S3M files, it's also way off.
			pChn = playState.Chn;
			for(CHANNELINDEX nChn = 0; nChn < GetNumChannels(); nChn++, pChn++)
			{
				ModCommand::COMMAND command = pChn->rowCommand.command;
				ModCommand::PARAM param = pChn->rowCommand.param;
				if((command == CMD_S3MCMDEX && param >= 0xB1 && param <= 0xBF)
					|| (command == CMD_MODCMDEX && param >= 0x61 && param <= 0x6F))
				{
					const double start = memory.chnSettings[nChn].patLoop;
					if(!startTimes[start]) startTimes[start] = 1;
					startTimes[start] = mpt::lcm(startTimes[start], 1 + (param & 0x0F));
				}
			}
			for(const auto &i : startTimes)
			{
				memory.elapsedTime += (memory.elapsedTime - i.first) * (double)(i.second - 1);
				for(CHANNELINDEX nChn = 0; nChn < GetNumChannels(); nChn++, pChn++)
				{
					if(memory.chnSettings[nChn].patLoop == i.first)
					{
						playState.m_lTotalSampleCount += (playState.m_lTotalSampleCount - memory.chnSettings[nChn].patLoopSmp) * (i.second - 1);
						if(m_playBehaviour[kITPatternLoopTargetReset] || (GetType() == MOD_TYPE_S3M))
						{
							memory.chnSettings[nChn].patLoop = memory.elapsedTime;
							memory.chnSettings[nChn].patLoopSmp = playState.m_lTotalSampleCount;
							memory.chnSettings[nChn].patLoopStart = playState.m_nRow + 1;
						}
						break;
					}
				}
			}
			if(GetType() == MOD_TYPE_IT)
			{
				// IT pattern loop start row update - at the end of a pattern loop, set pattern loop start to next row (for upcoming pattern loops with missing SB0)
				for(CHANNELINDEX nChn = 0; nChn < GetNumChannels(); nChn++)
				{
					if((pChn->rowCommand.command == CMD_S3MCMDEX && pChn->rowCommand.param >= 0xB1 && pChn->rowCommand.param <= 0xBF))
					{
						memory.chnSettings[nChn].patLoop = memory.elapsedTime;
						memory.chnSettings[nChn].patLoopSmp = playState.m_lTotalSampleCount;
					}
				}
			}
		}
	}

	// Now advance the sample positions for sample seeking on channels that are still playing
	if(adjustSamplePos)
	{
		for(CHANNELINDEX nChn = 0; nChn < GetNumChannels(); nChn++)
		{
			if(memory.chnSettings[nChn].ticksToRender != GetLengthMemory::IGNORE_CHANNEL)
			{
				memory.RenderChannel(nChn, oldTickDuration);
			}
		}
	}

	if(retval.targetReached || target.mode == GetLengthTarget::NoTarget)
	{
		retval.lastOrder = playState.m_nCurrentOrder;
		retval.lastRow = playState.m_nRow;
	}
	retval.duration = memory.elapsedTime;
	results.push_back(retval);

	// Store final variables
	if(adjustMode & eAdjust)
	{
		if(retval.targetReached || target.mode == GetLengthTarget::NoTarget)
		{
			// Target found, or there is no target (i.e. play whole song)...
			m_PlayState = std::move(playState);
			m_PlayState.m_nNextRow = m_PlayState.m_nRow;
			m_PlayState.m_nFrameDelay = m_PlayState.m_nPatternDelay = 0;
			m_PlayState.m_nTickCount = Util::MaxValueOfType(m_PlayState.m_nTickCount) - 1;
			m_PlayState.m_bPositionChanged = true;
			for(CHANNELINDEX n = 0; n < GetNumChannels(); n++)
			{
				if(m_PlayState.Chn[n].nLastNote != NOTE_NONE)
				{
					m_PlayState.Chn[n].nNewNote = m_PlayState.Chn[n].nLastNote;
				}
				if(memory.chnSettings[n].vol != 0xFF && !adjustSamplePos)
				{
					m_PlayState.Chn[n].nVolume = std::min(memory.chnSettings[n].vol, uint8(64)) * 4;
				}
			}

#ifndef NO_PLUGINS
			// If there were any PC events, update plugin parameters to their latest value.
			std::bitset<MAX_MIXPLUGINS> plugSetProgram;
			for(const auto &param : memory.plugParams)
			{
				PLUGINDEX plug = param.first.first - 1;
				IMixPlugin *plugin = m_MixPlugins[plug].pMixPlugin;
				if(plugin != nullptr)
				{
					if(!plugSetProgram[plug])
					{
						// Used for bridged plugins to avoid sending out individual messages for each parameter.
						plugSetProgram.set(plug);
						plugin->BeginSetProgram();
					}
					plugin->SetParameter(param.first.second, param.second / PlugParamValue(ModCommand::maxColumnValue));
				}
			}
			if(plugSetProgram.any())
			{
				for(PLUGINDEX i = 0; i < MAX_MIXPLUGINS; i++)
				{
					if(plugSetProgram[i])
					{
						m_MixPlugins[i].pMixPlugin->EndSetProgram();
					}
				}
			}
#endif // NO_PLUGINS
		} else if(adjustMode != eAdjustOnSuccess)
		{
			// Target not found (e.g. when jumping to a hidden sub song), reset global variables...
			m_PlayState.m_nMusicSpeed = m_nDefaultSpeed;
			m_PlayState.m_nMusicTempo = m_nDefaultTempo;
			m_PlayState.m_nGlobalVolume = m_nDefaultGlobalVolume;
		}
		// When adjusting the playback status, we will also want to update the visited rows vector according to the current position.
		if(sequence != Order.GetCurrentSequenceIndex())
		{
			Order.SetSequence(sequence);
		}
		visitedSongRows.Set(visitedRows);
	}

	return results;

}