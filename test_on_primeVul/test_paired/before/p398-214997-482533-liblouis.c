compileRule(FileInfo *file, TranslationTableHeader **table,
		DisplayTableHeader **displayTable, const MacroList **inScopeMacros) {
	CharsString token;
	TranslationTableOpcode opcode;
	CharsString ruleChars;
	CharsString ruleDots;
	CharsString cells;
	CharsString scratchPad;
	CharsString emphClass;
	TranslationTableCharacterAttributes after = 0;
	TranslationTableCharacterAttributes before = 0;
	int noback, nofor, nocross;
	noback = nofor = nocross = 0;
doOpcode:
	if (!getToken(file, &token, NULL)) return 1;				  /* blank line */
	if (token.chars[0] == '#' || token.chars[0] == '<') return 1; /* comment */
	if (file->lineNumber == 1 &&
			(eqasc2uni((unsigned char *)"ISO", token.chars, 3) ||
					eqasc2uni((unsigned char *)"UTF-8", token.chars, 5))) {
		if (table)
			compileHyphenation(file, &token, table);
		else
			/* ignore the whole file */
			while (_lou_getALine(file))
				;
		return 1;
	}
	opcode = getOpcode(file, &token);
	switch (opcode) {
	case CTO_Macro: {
		const Macro *macro;
#ifdef ENABLE_MACROS
		if (!inScopeMacros) {
			compileError(file, "Defining macros only allowed in table files.");
			return 0;
		}
		if (compileMacro(file, &macro)) {
			*inScopeMacros = cons_macro(macro, *inScopeMacros);
			return 1;
		}
		return 0;
#else
		compileError(file, "Macro feature is disabled.");
		return 0;
#endif
	}
	case CTO_IncludeFile: {
		CharsString includedFile;
		if (!getToken(file, &token, "include file name")) return 0;
		if (!parseChars(file, &includedFile, &token)) return 0;
		return includeFile(file, &includedFile, table, displayTable);
	}
	case CTO_NoBack:
		if (nofor) {
			compileError(file, "%s already specified.", _lou_findOpcodeName(CTO_NoFor));
			return 0;
		}
		noback = 1;
		goto doOpcode;
	case CTO_NoFor:
		if (noback) {
			compileError(file, "%s already specified.", _lou_findOpcodeName(CTO_NoBack));
			return 0;
		}
		nofor = 1;
		goto doOpcode;
	case CTO_Space:
		return compileCharDef(
				file, opcode, CTC_Space, noback, nofor, table, displayTable);
	case CTO_Digit:
		return compileCharDef(
				file, opcode, CTC_Digit, noback, nofor, table, displayTable);
	case CTO_LitDigit:
		return compileCharDef(
				file, opcode, CTC_LitDigit, noback, nofor, table, displayTable);
	case CTO_Punctuation:
		return compileCharDef(
				file, opcode, CTC_Punctuation, noback, nofor, table, displayTable);
	case CTO_Math:
		return compileCharDef(file, opcode, CTC_Math, noback, nofor, table, displayTable);
	case CTO_Sign:
		return compileCharDef(file, opcode, CTC_Sign, noback, nofor, table, displayTable);
	case CTO_Letter:
		return compileCharDef(
				file, opcode, CTC_Letter, noback, nofor, table, displayTable);
	case CTO_UpperCase:
		return compileCharDef(
				file, opcode, CTC_UpperCase, noback, nofor, table, displayTable);
	case CTO_LowerCase:
		return compileCharDef(
				file, opcode, CTC_LowerCase, noback, nofor, table, displayTable);
	case CTO_Grouping:
		return compileGrouping(file, noback, nofor, table, displayTable);
	case CTO_Display:
		if (!displayTable) return 1;  // ignore
		if (!getRuleCharsText(file, &ruleChars)) return 0;
		if (!getRuleDotsPattern(file, &ruleDots)) return 0;
		if (ruleChars.length != 1 || ruleDots.length != 1) {
			compileError(file, "Exactly one character and one cell are required.");
			return 0;
		}
		return putCharDotsMapping(
				file, ruleChars.chars[0], ruleDots.chars[0], displayTable);
	case CTO_UpLow:
	case CTO_None: {
		// check if token is a macro name
		if (inScopeMacros) {
			const MacroList *macros = *inScopeMacros;
			while (macros) {
				const Macro *m = macros->head;
				if (token.length == strlen(m->name) &&
						eqasc2uni((unsigned char *)m->name, token.chars, token.length)) {
					if (!inScopeMacros) {
						compileError(file, "Calling macros only allowed in table files.");
						return 0;
					}
					FileInfo tmpFile;
					memset(&tmpFile, 0, sizeof(tmpFile));
					tmpFile.fileName = file->fileName;
					tmpFile.sourceFile = file->sourceFile;
					tmpFile.lineNumber = file->lineNumber;
					tmpFile.encoding = noEncoding;
					tmpFile.status = 0;
					tmpFile.linepos = 0;
					tmpFile.linelen = 0;
					int argument_count = 0;
					CharsString *arguments =
							malloc(m->argument_count * sizeof(CharsString));
					while (argument_count < m->argument_count) {
						if (getToken(file, &token, "macro argument"))
							arguments[argument_count++] = token;
						else
							break;
					}
					if (argument_count < m->argument_count) {
						compileError(file, "Expected %d arguments", m->argument_count);
						return 0;
					}
					int i = 0;
					int subst = 0;
					int next = subst < m->substitution_count ? m->substitutions[2 * subst]
															 : m->definition_length;
					for (;;) {
						while (i < next) {
							widechar c = m->definition[i++];
							if (c == '\n') {
								if (!compileRule(&tmpFile, table, displayTable,
											inScopeMacros)) {
									_lou_logMessage(LOU_LOG_ERROR,
											"result of macro expansion was: %s",
											_lou_showString(
													tmpFile.line, tmpFile.linelen, 0));
									return 0;
								}
								tmpFile.linepos = 0;
								tmpFile.linelen = 0;
							} else if (tmpFile.linelen >= MAXSTRING) {
								compileError(file,
										"Line exceeds %d characters (post macro "
										"expansion)",
										MAXSTRING);
								return 0;
							} else
								tmpFile.line[tmpFile.linelen++] = c;
						}
						if (subst < m->substitution_count) {
							CharsString arg =
									arguments[m->substitutions[2 * subst + 1] - 1];
							for (int j = 0; j < arg.length; j++)
								tmpFile.line[tmpFile.linelen++] = arg.chars[j];
							subst++;
							next = subst < m->substitution_count
									? m->substitutions[2 * subst]
									: m->definition_length;
						} else {
							if (!compileRule(
										&tmpFile, table, displayTable, inScopeMacros)) {
								_lou_logMessage(LOU_LOG_ERROR,
										"result of macro expansion was: %s",
										_lou_showString(
												tmpFile.line, tmpFile.linelen, 0));
								return 0;
							}
							break;
						}
					}
					return 1;
				}
				macros = macros->tail;
			}
		}
		if (opcode == CTO_UpLow) {
			compileError(file, "The uplow opcode is deprecated.");
			return 0;
		}
		compileError(file, "opcode %s not defined.",
				_lou_showString(token.chars, token.length, 0));
		return 0;
	}

	/* now only opcodes follow that don't modify the display table */
	default:
		if (!table) return 1;
		switch (opcode) {
		case CTO_Locale:
			compileWarning(file,
					"The locale opcode is not implemented. Use the locale meta data "
					"instead.");
			return 1;
		case CTO_Undefined: {
			// not passing pointer because compileBrailleIndicator may reallocate table
			TranslationTableOffset ruleOffset = (*table)->undefined;
			if (!compileBrailleIndicator(file, "undefined character opcode",
						CTO_Undefined, &ruleOffset, noback, nofor, table))
				return 0;
			(*table)->undefined = ruleOffset;
			return 1;
		}
		case CTO_Match: {
			int ok = 0;
			widechar *patterns = NULL;
			TranslationTableRule *rule;
			TranslationTableOffset ruleOffset;
			CharsString ptn_before, ptn_after;
			TranslationTableOffset patternsOffset;
			int len, mrk;
			size_t patternsByteSize = sizeof(*patterns) * 27720;
			patterns = (widechar *)malloc(patternsByteSize);
			if (!patterns) _lou_outOfMemory();
			memset(patterns, 0xffff, patternsByteSize);
			noback = 1;
			getCharacters(file, &ptn_before);
			getRuleCharsText(file, &ruleChars);
			getCharacters(file, &ptn_after);
			getRuleDotsPattern(file, &ruleDots);
			if (!addRule(file, opcode, &ruleChars, &ruleDots, after, before, &ruleOffset,
						&rule, noback, nofor, table))
				goto CTO_Match_cleanup;
			if (ptn_before.chars[0] == '-' && ptn_before.length == 1)
				len = _lou_pattern_compile(
						&ptn_before.chars[0], 0, &patterns[1], 13841, *table, file);
			else
				len = _lou_pattern_compile(&ptn_before.chars[0], ptn_before.length,
						&patterns[1], 13841, *table, file);
			if (!len) goto CTO_Match_cleanup;
			mrk = patterns[0] = len + 1;
			_lou_pattern_reverse(&patterns[1]);
			if (ptn_after.chars[0] == '-' && ptn_after.length == 1)
				len = _lou_pattern_compile(
						&ptn_after.chars[0], 0, &patterns[mrk], 13841, *table, file);
			else
				len = _lou_pattern_compile(&ptn_after.chars[0], ptn_after.length,
						&patterns[mrk], 13841, *table, file);
			if (!len) goto CTO_Match_cleanup;
			len += mrk;
			if (!allocateSpaceInTranslationTable(
						file, &patternsOffset, len * sizeof(widechar), table))
				goto CTO_Match_cleanup;
			// allocateSpaceInTranslationTable may have moved table, so make sure rule is
			// still valid
			rule = (TranslationTableRule *)&(*table)->ruleArea[ruleOffset];
			memcpy(&(*table)->ruleArea[patternsOffset], patterns, len * sizeof(widechar));
			rule->patterns = patternsOffset;
			ok = 1;
		CTO_Match_cleanup:
			free(patterns);
			return ok;
		}

		case CTO_BackMatch: {
			int ok = 0;
			widechar *patterns = NULL;
			TranslationTableRule *rule;
			TranslationTableOffset ruleOffset;
			CharsString ptn_before, ptn_after;
			TranslationTableOffset patternOffset;
			int len, mrk;
			size_t patternsByteSize = sizeof(*patterns) * 27720;
			patterns = (widechar *)malloc(patternsByteSize);
			if (!patterns) _lou_outOfMemory();
			memset(patterns, 0xffff, patternsByteSize);
			nofor = 1;
			getCharacters(file, &ptn_before);
			getRuleCharsText(file, &ruleChars);
			getCharacters(file, &ptn_after);
			getRuleDotsPattern(file, &ruleDots);
			if (!addRule(file, opcode, &ruleChars, &ruleDots, 0, 0, &ruleOffset, &rule,
						noback, nofor, table))
				goto CTO_BackMatch_cleanup;
			if (ptn_before.chars[0] == '-' && ptn_before.length == 1)
				len = _lou_pattern_compile(
						&ptn_before.chars[0], 0, &patterns[1], 13841, *table, file);
			else
				len = _lou_pattern_compile(&ptn_before.chars[0], ptn_before.length,
						&patterns[1], 13841, *table, file);
			if (!len) goto CTO_BackMatch_cleanup;
			mrk = patterns[0] = len + 1;
			_lou_pattern_reverse(&patterns[1]);
			if (ptn_after.chars[0] == '-' && ptn_after.length == 1)
				len = _lou_pattern_compile(
						&ptn_after.chars[0], 0, &patterns[mrk], 13841, *table, file);
			else
				len = _lou_pattern_compile(&ptn_after.chars[0], ptn_after.length,
						&patterns[mrk], 13841, *table, file);
			if (!len) goto CTO_BackMatch_cleanup;
			len += mrk;
			if (!allocateSpaceInTranslationTable(
						file, &patternOffset, len * sizeof(widechar), table))
				goto CTO_BackMatch_cleanup;
			// allocateSpaceInTranslationTable may have moved table, so make sure rule is
			// still valid
			rule = (TranslationTableRule *)&(*table)->ruleArea[ruleOffset];
			memcpy(&(*table)->ruleArea[patternOffset], patterns, len * sizeof(widechar));
			rule->patterns = patternOffset;
			ok = 1;
		CTO_BackMatch_cleanup:
			free(patterns);
			return ok;
		}

		case CTO_CapsLetter:
		case CTO_BegCapsWord:
		case CTO_EndCapsWord:
		case CTO_BegCaps:
		case CTO_EndCaps:
		case CTO_BegCapsPhrase:
		case CTO_EndCapsPhrase:
		case CTO_LenCapsPhrase:
		/* these 8 general purpose opcodes are compiled further down to more specific
		 * internal opcodes:
		 * - modeletter
		 * - begmodeword
		 * - endmodeword
		 * - begmode
		 * - endmode
		 * - begmodephrase
		 * - endmodephrase
		 * - lenmodephrase
		 */
		case CTO_ModeLetter:
		case CTO_BegModeWord:
		case CTO_EndModeWord:
		case CTO_BegMode:
		case CTO_EndMode:
		case CTO_BegModePhrase:
		case CTO_EndModePhrase:
		case CTO_LenModePhrase: {
			TranslationTableCharacterAttributes mode;
			int i;
			switch (opcode) {
			case CTO_CapsLetter:
			case CTO_BegCapsWord:
			case CTO_EndCapsWord:
			case CTO_BegCaps:
			case CTO_EndCaps:
			case CTO_BegCapsPhrase:
			case CTO_EndCapsPhrase:
			case CTO_LenCapsPhrase:
				mode = CTC_UpperCase;
				i = 0;
				opcode += (CTO_ModeLetter - CTO_CapsLetter);
				break;
			default:
				if (!getToken(file, &token, "attribute name")) return 0;
				if (!(*table)->characterClasses && !allocateCharacterClasses(*table)) {
					return 0;
				}
				const CharacterClass *characterClass = findCharacterClass(&token, *table);
				if (!characterClass) {
					characterClass =
							addCharacterClass(file, token.chars, token.length, *table, 1);
					if (!characterClass) return 0;
				}
				mode = characterClass->attribute;
				if (!(mode == CTC_UpperCase || mode == CTC_Digit) && mode >= CTC_Space &&
						mode <= CTC_LitDigit) {
					compileError(file,
							"mode must be \"uppercase\", \"digit\", or a custom "
							"attribute name.");
					return 0;
				}
				/* check if this mode is already defined and if the number of modes does
				 * not exceed the maximal number */
				if (mode == CTC_UpperCase)
					i = 0;
				else {
					for (i = 1; i < MAX_MODES && (*table)->modes[i].value; i++) {
						if ((*table)->modes[i].mode == mode) {
							break;
						}
					}
					if (i == MAX_MODES) {
						compileError(file, "Max number of modes (%i) reached", MAX_MODES);
						return 0;
					}
				}
			}
			if (!(*table)->modes[i].value)
				(*table)->modes[i] = (EmphasisClass){ plain_text, mode,
					0x1 << (MAX_EMPH_CLASSES + i), MAX_EMPH_CLASSES + i };
			switch (opcode) {
			case CTO_BegModePhrase: {
				// not passing pointer because compileBrailleIndicator may reallocate
				// table
				TranslationTableOffset ruleOffset =
						(*table)->emphRules[MAX_EMPH_CLASSES + i][begPhraseOffset];
				if (!compileBrailleIndicator(file, "first word capital sign",
							CTO_BegCapsPhraseRule + (8 * i), &ruleOffset, noback, nofor,
							table))
					return 0;
				(*table)->emphRules[MAX_EMPH_CLASSES + i][begPhraseOffset] = ruleOffset;
				return 1;
			}
			case CTO_EndModePhrase: {
				TranslationTableOffset ruleOffset;
				switch (compileBeforeAfter(file)) {
				case 1:	 // before
					if ((*table)->emphRules[MAX_EMPH_CLASSES + i][endPhraseAfterOffset]) {
						compileError(
								file, "Capital sign after last word already defined.");
						return 0;
					}
					// not passing pointer because compileBrailleIndicator may reallocate
					// table
					ruleOffset = (*table)->emphRules[MAX_EMPH_CLASSES + i]
													[endPhraseBeforeOffset];
					if (!compileBrailleIndicator(file, "capital sign before last word",
								CTO_EndCapsPhraseBeforeRule + (8 * i), &ruleOffset,
								noback, nofor, table))
						return 0;
					(*table)->emphRules[MAX_EMPH_CLASSES + i][endPhraseBeforeOffset] =
							ruleOffset;
					return 1;
				case 2:	 // after
					if ((*table)->emphRules[MAX_EMPH_CLASSES + i]
										   [endPhraseBeforeOffset]) {
						compileError(
								file, "Capital sign before last word already defined.");
						return 0;
					}
					// not passing pointer because compileBrailleIndicator may reallocate
					// table
					ruleOffset = (*table)->emphRules[MAX_EMPH_CLASSES + i]
													[endPhraseAfterOffset];
					if (!compileBrailleIndicator(file, "capital sign after last word",
								CTO_EndCapsPhraseAfterRule + (8 * i), &ruleOffset, noback,
								nofor, table))
						return 0;
					(*table)->emphRules[MAX_EMPH_CLASSES + i][endPhraseAfterOffset] =
							ruleOffset;
					return 1;
				default:  // error
					compileError(file, "Invalid lastword indicator location.");
					return 0;
				}
				return 0;
			}
			case CTO_BegMode: {
				// not passing pointer because compileBrailleIndicator may reallocate
				// table
				TranslationTableOffset ruleOffset =
						(*table)->emphRules[MAX_EMPH_CLASSES + i][begOffset];
				if (!compileBrailleIndicator(file, "first letter capital sign",
							CTO_BegCapsRule + (8 * i), &ruleOffset, noback, nofor, table))
					return 0;
				(*table)->emphRules[MAX_EMPH_CLASSES + i][begOffset] = ruleOffset;
				return 1;
			}
			case CTO_EndMode: {
				// not passing pointer because compileBrailleIndicator may reallocate
				// table
				TranslationTableOffset ruleOffset =
						(*table)->emphRules[MAX_EMPH_CLASSES + i][endOffset];
				if (!compileBrailleIndicator(file, "last letter capital sign",
							CTO_EndCapsRule + (8 * i), &ruleOffset, noback, nofor, table))
					return 0;
				(*table)->emphRules[MAX_EMPH_CLASSES + i][endOffset] = ruleOffset;
				return 1;
			}
			case CTO_ModeLetter: {
				// not passing pointer because compileBrailleIndicator may reallocate
				// table
				TranslationTableOffset ruleOffset =
						(*table)->emphRules[MAX_EMPH_CLASSES + i][letterOffset];
				if (!compileBrailleIndicator(file, "single letter capital sign",
							CTO_CapsLetterRule + (8 * i), &ruleOffset, noback, nofor,
							table))
					return 0;
				(*table)->emphRules[MAX_EMPH_CLASSES + i][letterOffset] = ruleOffset;
				return 1;
			}
			case CTO_BegModeWord: {
				// not passing pointer because compileBrailleIndicator may reallocate
				// table
				TranslationTableOffset ruleOffset =
						(*table)->emphRules[MAX_EMPH_CLASSES + i][begWordOffset];
				if (!compileBrailleIndicator(file, "capital word",
							CTO_BegCapsWordRule + (8 * i), &ruleOffset, noback, nofor,
							table))
					return 0;
				(*table)->emphRules[MAX_EMPH_CLASSES + i][begWordOffset] = ruleOffset;
				return 1;
			}
			case CTO_EndModeWord: {
				// not passing pointer because compileBrailleIndicator may reallocate
				// table
				TranslationTableOffset ruleOffset =
						(*table)->emphRules[MAX_EMPH_CLASSES + i][endWordOffset];
				if (!compileBrailleIndicator(file, "capital word stop",
							CTO_EndCapsWordRule + (8 * i), &ruleOffset, noback, nofor,
							table))
					return 0;
				(*table)->emphRules[MAX_EMPH_CLASSES + i][endWordOffset] = ruleOffset;
				return 1;
			}
			case CTO_LenModePhrase:
				return (*table)->emphRules[MAX_EMPH_CLASSES + i][lenPhraseOffset] =
							   compileNumber(file);
			default:
				break;
			}
			break;
		}

		/* these 8 general purpose emphasis opcodes are compiled further down to more
		 * specific internal opcodes:
		 * - emphletter
		 * - begemphword
		 * - endemphword
		 * - begemph
		 * - endemph
		 * - begemphphrase
		 * - endemphphrase
		 * - lenemphphrase
		 */
		case CTO_EmphClass:
			if (!getToken(file, &emphClass, "emphasis class")) {
				compileError(file, "emphclass must be followed by a valid class name.");
				return 0;
			}
			int k, i;
			char *s = malloc(sizeof(char) * (emphClass.length + 1));
			for (k = 0; k < emphClass.length; k++) s[k] = (char)emphClass.chars[k];
			s[k++] = '\0';
			for (i = 0; i < MAX_EMPH_CLASSES && (*table)->emphClassNames[i]; i++)
				if (strcmp(s, (*table)->emphClassNames[i]) == 0) {
					_lou_logMessage(LOU_LOG_WARN, "Duplicate emphasis class: %s", s);
					warningCount++;
					free(s);
					return 1;
				}
			if (i == MAX_EMPH_CLASSES) {
				_lou_logMessage(LOU_LOG_ERROR,
						"Max number of emphasis classes (%i) reached", MAX_EMPH_CLASSES);
				errorCount++;
				free(s);
				return 0;
			}
			switch (i) {
			/* For backwards compatibility (i.e. because programs will assume
			 * the first 3 typeform bits are `italic', `underline' and `bold')
			 * we require that the first 3 emphclass definitions are (in that
			 * order):
			 *
			 *   emphclass italic
			 *   emphclass underline
			 *   emphclass bold
			 *
			 * While it would be possible to use the emphclass opcode only for
			 * defining _additional_ classes (not allowing for them to be called
			 * italic, underline or bold), thereby reducing the amount of
			 * boilerplate, we deliberately choose not to do that in order to
			 * not give italic, underline and bold any special status. The
			 * hope is that eventually all programs will use liblouis for
			 * emphasis the recommended way (i.e. by looking up the supported
			 * typeforms in the documentation or API) so that we can drop this
			 * restriction.
			 */
			case 0:
				if (strcmp(s, "italic") != 0) {
					_lou_logMessage(LOU_LOG_ERROR,
							"First emphasis class must be \"italic\" but got "
							"%s",
							s);
					errorCount++;
					free(s);
					return 0;
				}
				break;
			case 1:
				if (strcmp(s, "underline") != 0) {
					_lou_logMessage(LOU_LOG_ERROR,
							"Second emphasis class must be \"underline\" but "
							"got "
							"%s",
							s);
					errorCount++;
					free(s);
					return 0;
				}
				break;
			case 2:
				if (strcmp(s, "bold") != 0) {
					_lou_logMessage(LOU_LOG_ERROR,
							"Third emphasis class must be \"bold\" but got "
							"%s",
							s);
					errorCount++;
					free(s);
					return 0;
				}
				break;
			}
			(*table)->emphClassNames[i] = s;
			(*table)->emphClasses[i] = (EmphasisClass){ emph_1
						<< i, /* relies on the order of typeforms emph_1..emph_10 */
				0, 0x1 << i, i };
			return 1;
		case CTO_EmphLetter:
		case CTO_BegEmphWord:
		case CTO_EndEmphWord:
		case CTO_BegEmph:
		case CTO_EndEmph:
		case CTO_BegEmphPhrase:
		case CTO_EndEmphPhrase:
		case CTO_LenEmphPhrase:
		case CTO_EmphModeChars:
		case CTO_NoEmphChars: {
			if (!getToken(file, &token, "emphasis class")) return 0;
			if (!parseChars(file, &emphClass, &token)) return 0;
			char *s = malloc(sizeof(char) * (emphClass.length + 1));
			int k, i;
			for (k = 0; k < emphClass.length; k++) s[k] = (char)emphClass.chars[k];
			s[k++] = '\0';
			for (i = 0; i < MAX_EMPH_CLASSES && (*table)->emphClassNames[i]; i++)
				if (strcmp(s, (*table)->emphClassNames[i]) == 0) break;
			if (i == MAX_EMPH_CLASSES || !(*table)->emphClassNames[i]) {
				_lou_logMessage(LOU_LOG_ERROR, "Emphasis class %s not declared", s);
				errorCount++;
				free(s);
				return 0;
			}
			int ok = 0;
			switch (opcode) {
			case CTO_EmphLetter: {
				// not passing pointer because compileBrailleIndicator may reallocate
				// table
				TranslationTableOffset ruleOffset = (*table)->emphRules[i][letterOffset];
				if (!compileBrailleIndicator(file, "single letter",
							CTO_Emph1LetterRule + letterOffset + (8 * i), &ruleOffset,
							noback, nofor, table))
					break;
				(*table)->emphRules[i][letterOffset] = ruleOffset;
				ok = 1;
				break;
			}
			case CTO_BegEmphWord: {
				// not passing pointer because compileBrailleIndicator may reallocate
				// table
				TranslationTableOffset ruleOffset = (*table)->emphRules[i][begWordOffset];
				if (!compileBrailleIndicator(file, "word",
							CTO_Emph1LetterRule + begWordOffset + (8 * i), &ruleOffset,
							noback, nofor, table))
					break;
				(*table)->emphRules[i][begWordOffset] = ruleOffset;
				ok = 1;
				break;
			}
			case CTO_EndEmphWord: {
				// not passing pointer because compileBrailleIndicator may reallocate
				// table
				TranslationTableOffset ruleOffset = (*table)->emphRules[i][endWordOffset];
				if (!compileBrailleIndicator(file, "word stop",
							CTO_Emph1LetterRule + endWordOffset + (8 * i), &ruleOffset,
							noback, nofor, table))
					break;
				(*table)->emphRules[i][endWordOffset] = ruleOffset;
				ok = 1;
				break;
			}
			case CTO_BegEmph: {
				/* fail if both begemph and any of begemphphrase or begemphword are
				 * defined */
				if ((*table)->emphRules[i][begWordOffset] ||
						(*table)->emphRules[i][begPhraseOffset]) {
					compileError(file,
							"Cannot define emphasis for both no context and word or "
							"phrase context, i.e. cannot have both begemph and "
							"begemphword or begemphphrase.");
					break;
				}
				// not passing pointer because compileBrailleIndicator may reallocate
				// table
				TranslationTableOffset ruleOffset = (*table)->emphRules[i][begOffset];
				if (!compileBrailleIndicator(file, "first letter",
							CTO_Emph1LetterRule + begOffset + (8 * i), &ruleOffset,
							noback, nofor, table))
					break;
				(*table)->emphRules[i][begOffset] = ruleOffset;
				ok = 1;
				break;
			}
			case CTO_EndEmph: {
				if ((*table)->emphRules[i][endWordOffset] ||
						(*table)->emphRules[i][endPhraseBeforeOffset] ||
						(*table)->emphRules[i][endPhraseAfterOffset]) {
					compileError(file,
							"Cannot define emphasis for both no context and word or "
							"phrase context, i.e. cannot have both endemph and "
							"endemphword or endemphphrase.");
					break;
				}
				// not passing pointer because compileBrailleIndicator may reallocate
				// table
				TranslationTableOffset ruleOffset = (*table)->emphRules[i][endOffset];
				if (!compileBrailleIndicator(file, "last letter",
							CTO_Emph1LetterRule + endOffset + (8 * i), &ruleOffset,
							noback, nofor, table))
					break;
				(*table)->emphRules[i][endOffset] = ruleOffset;
				ok = 1;
				break;
			}
			case CTO_BegEmphPhrase: {
				// not passing pointer because compileBrailleIndicator may reallocate
				// table
				TranslationTableOffset ruleOffset =
						(*table)->emphRules[i][begPhraseOffset];
				if (!compileBrailleIndicator(file, "first word",
							CTO_Emph1LetterRule + begPhraseOffset + (8 * i), &ruleOffset,
							noback, nofor, table))
					break;
				(*table)->emphRules[i][begPhraseOffset] = ruleOffset;
				ok = 1;
				break;
			}
			case CTO_EndEmphPhrase:
				switch (compileBeforeAfter(file)) {
				case 1: {  // before
					if ((*table)->emphRules[i][endPhraseAfterOffset]) {
						compileError(file, "last word after already defined.");
						break;
					}
					// not passing pointer because compileBrailleIndicator may reallocate
					// table
					TranslationTableOffset ruleOffset =
							(*table)->emphRules[i][endPhraseBeforeOffset];
					if (!compileBrailleIndicator(file, "last word before",
								CTO_Emph1LetterRule + endPhraseBeforeOffset + (8 * i),
								&ruleOffset, noback, nofor, table))
						break;
					(*table)->emphRules[i][endPhraseBeforeOffset] = ruleOffset;
					ok = 1;
					break;
				}
				case 2: {  // after
					if ((*table)->emphRules[i][endPhraseBeforeOffset]) {
						compileError(file, "last word before already defined.");
						break;
					}
					// not passing pointer because compileBrailleIndicator may reallocate
					// table
					TranslationTableOffset ruleOffset =
							(*table)->emphRules[i][endPhraseAfterOffset];
					if (!compileBrailleIndicator(file, "last word after",
								CTO_Emph1LetterRule + endPhraseAfterOffset + (8 * i),
								&ruleOffset, noback, nofor, table))
						break;
					(*table)->emphRules[i][endPhraseAfterOffset] = ruleOffset;
					ok = 1;
					break;
				}
				default:  // error
					compileError(file, "Invalid lastword indicator location.");
					break;
				}
				break;
			case CTO_LenEmphPhrase:
				if (((*table)->emphRules[i][lenPhraseOffset] = compileNumber(file)))
					ok = 1;
				break;
			case CTO_EmphModeChars: {
				if (!getRuleCharsText(file, &ruleChars)) break;
				widechar *emphmodechars = (*table)->emphModeChars[i];
				int len;
				for (len = 0; len < EMPHMODECHARSSIZE && emphmodechars[len]; len++)
					;
				if (len + ruleChars.length > EMPHMODECHARSSIZE) {
					compileError(file, "More than %d characters", EMPHMODECHARSSIZE);
					break;
				}
				ok = 1;
				for (int k = 0; k < ruleChars.length; k++) {
					if (!getChar(ruleChars.chars[k], *table, NULL)) {
						compileError(file, "Emphasis mode character undefined");
						ok = 0;
						break;
					}
					emphmodechars[len++] = ruleChars.chars[k];
				}
				break;
			}
			case CTO_NoEmphChars: {
				if (!getRuleCharsText(file, &ruleChars)) break;
				widechar *noemphchars = (*table)->noEmphChars[i];
				int len;
				for (len = 0; len < NOEMPHCHARSSIZE && noemphchars[len]; len++)
					;
				if (len + ruleChars.length > NOEMPHCHARSSIZE) {
					compileError(file, "More than %d characters", NOEMPHCHARSSIZE);
					break;
				}
				ok = 1;
				for (int k = 0; k < ruleChars.length; k++) {
					if (!getChar(ruleChars.chars[k], *table, NULL)) {
						compileError(file, "Character undefined");
						ok = 0;
						break;
					}
					noemphchars[len++] = ruleChars.chars[k];
				}
				break;
			}
			default:
				break;
			}
			free(s);
			return ok;
		}
		case CTO_LetterSign: {
			// not passing pointer because compileBrailleIndicator may reallocate table
			TranslationTableOffset ruleOffset = (*table)->letterSign;
			if (!compileBrailleIndicator(file, "letter sign", CTO_LetterRule, &ruleOffset,
						noback, nofor, table))
				return 0;
			(*table)->letterSign = ruleOffset;
			return 1;
		}
		case CTO_NoLetsignBefore:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			if (((*table)->noLetsignBeforeCount + ruleChars.length) > LETSIGNBEFORESIZE) {
				compileError(file, "More than %d characters", LETSIGNBEFORESIZE);
				return 0;
			}
			for (int k = 0; k < ruleChars.length; k++)
				(*table)->noLetsignBefore[(*table)->noLetsignBeforeCount++] =
						ruleChars.chars[k];
			return 1;
		case CTO_NoLetsign:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			if (((*table)->noLetsignCount + ruleChars.length) > LETSIGNSIZE) {
				compileError(file, "More than %d characters", LETSIGNSIZE);
				return 0;
			}
			for (int k = 0; k < ruleChars.length; k++)
				(*table)->noLetsign[(*table)->noLetsignCount++] = ruleChars.chars[k];
			return 1;
		case CTO_NoLetsignAfter:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			if (((*table)->noLetsignAfterCount + ruleChars.length) > LETSIGNAFTERSIZE) {
				compileError(file, "More than %d characters", LETSIGNAFTERSIZE);
				return 0;
			}
			for (int k = 0; k < ruleChars.length; k++)
				(*table)->noLetsignAfter[(*table)->noLetsignAfterCount++] =
						ruleChars.chars[k];
			return 1;
		case CTO_NumberSign: {
			// not passing pointer because compileBrailleIndicator may reallocate table
			TranslationTableOffset ruleOffset = (*table)->numberSign;
			if (!compileBrailleIndicator(file, "number sign", CTO_NumberRule, &ruleOffset,
						noback, nofor, table))
				return 0;
			(*table)->numberSign = ruleOffset;
			return 1;
		}

		case CTO_NumericModeChars:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			for (int k = 0; k < ruleChars.length; k++) {
				TranslationTableCharacter *c = getChar(ruleChars.chars[k], *table, NULL);
				if (!c) {
					compileError(file, "Numeric mode character undefined: %s",
							_lou_showString(&ruleChars.chars[k], 1, 0));
					return 0;
				}
				c->attributes |= CTC_NumericMode;
				(*table)->usesNumericMode = 1;
			}
			return 1;

		case CTO_MidEndNumericModeChars:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			for (int k = 0; k < ruleChars.length; k++) {
				TranslationTableCharacter *c = getChar(ruleChars.chars[k], *table, NULL);
				if (!c) {
					compileError(file, "Midendnumeric mode character undefined");
					return 0;
				}
				c->attributes |= CTC_MidEndNumericMode;
				(*table)->usesNumericMode = 1;
			}
			return 1;

		case CTO_NumericNoContractChars:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			for (int k = 0; k < ruleChars.length; k++) {
				TranslationTableCharacter *c = getChar(ruleChars.chars[k], *table, NULL);
				if (!c) {
					compileError(file, "Numeric no contraction character undefined");
					return 0;
				}
				c->attributes |= CTC_NumericNoContract;
				(*table)->usesNumericMode = 1;
			}
			return 1;

		case CTO_NoContractSign: {
			// not passing pointer because compileBrailleIndicator may reallocate table
			TranslationTableOffset ruleOffset = (*table)->noContractSign;
			if (!compileBrailleIndicator(file, "no contractions sign", CTO_NoContractRule,
						&ruleOffset, noback, nofor, table))
				return 0;
			(*table)->noContractSign = ruleOffset;
			return 1;
		}
		case CTO_SeqDelimiter:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			for (int k = 0; k < ruleChars.length; k++) {
				TranslationTableCharacter *c = getChar(ruleChars.chars[k], *table, NULL);
				if (!c) {
					compileError(file, "Sequence delimiter character undefined");
					return 0;
				}
				c->attributes |= CTC_SeqDelimiter;
				(*table)->usesSequences = 1;
			}
			return 1;

		case CTO_SeqBeforeChars:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			for (int k = 0; k < ruleChars.length; k++) {
				TranslationTableCharacter *c = getChar(ruleChars.chars[k], *table, NULL);
				if (!c) {
					compileError(file, "Sequence before character undefined");
					return 0;
				}
				c->attributes |= CTC_SeqBefore;
			}
			return 1;

		case CTO_SeqAfterChars:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			for (int k = 0; k < ruleChars.length; k++) {
				TranslationTableCharacter *c = getChar(ruleChars.chars[k], *table, NULL);
				if (!c) {
					compileError(file, "Sequence after character undefined");
					return 0;
				}
				c->attributes |= CTC_SeqAfter;
			}
			return 1;

		case CTO_SeqAfterPattern:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			if (((*table)->seqPatternsCount + ruleChars.length + 1) > SEQPATTERNSIZE) {
				compileError(file, "More than %d characters", SEQPATTERNSIZE);
				return 0;
			}
			for (int k = 0; k < ruleChars.length; k++)
				(*table)->seqPatterns[(*table)->seqPatternsCount++] = ruleChars.chars[k];
			(*table)->seqPatterns[(*table)->seqPatternsCount++] = 0;
			return 1;

		case CTO_SeqAfterExpression:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			for ((*table)->seqAfterExpressionLength = 0;
					(*table)->seqAfterExpressionLength < ruleChars.length;
					(*table)->seqAfterExpressionLength++)
				(*table)->seqAfterExpression[(*table)->seqAfterExpressionLength] =
						ruleChars.chars[(*table)->seqAfterExpressionLength];
			(*table)->seqAfterExpression[(*table)->seqAfterExpressionLength] = 0;
			return 1;

		case CTO_CapsModeChars:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			for (int k = 0; k < ruleChars.length; k++) {
				TranslationTableCharacter *c = getChar(ruleChars.chars[k], *table, NULL);
				if (!c) {
					compileError(file, "Capital mode character undefined");
					return 0;
				}
				c->attributes |= CTC_CapsMode;
				(*table)->hasCapsModeChars = 1;
			}
			return 1;

		case CTO_BegComp: {
			// not passing pointer because compileBrailleIndicator may reallocate table
			TranslationTableOffset ruleOffset = (*table)->begComp;
			if (!compileBrailleIndicator(file, "begin computer braille", CTO_BegCompRule,
						&ruleOffset, noback, nofor, table))
				return 0;
			(*table)->begComp = ruleOffset;
			return 1;
		}
		case CTO_EndComp: {
			// not passing pointer because compileBrailleIndicator may reallocate table
			TranslationTableOffset ruleOffset = (*table)->endComp;
			if (!compileBrailleIndicator(file, "end computer braslle", CTO_EndCompRule,
						&ruleOffset, noback, nofor, table))
				return 0;
			(*table)->endComp = ruleOffset;
			return 1;
		}
		case CTO_NoCross:
			if (nocross) {
				compileError(
						file, "%s already specified.", _lou_findOpcodeName(CTO_NoCross));
				return 0;
			}
			nocross = 1;
			goto doOpcode;
		case CTO_Syllable:
			(*table)->syllables = 1;
		case CTO_Always:
		case CTO_LargeSign:
		case CTO_WholeWord:
		case CTO_PartWord:
		case CTO_JoinNum:
		case CTO_JoinableWord:
		case CTO_LowWord:
		case CTO_SuffixableWord:
		case CTO_PrefixableWord:
		case CTO_BegWord:
		case CTO_BegMidWord:
		case CTO_MidWord:
		case CTO_MidEndWord:
		case CTO_EndWord:
		case CTO_PrePunc:
		case CTO_PostPunc:
		case CTO_BegNum:
		case CTO_MidNum:
		case CTO_EndNum:
		case CTO_Repeated:
		case CTO_RepWord:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			if (!getRuleDotsPattern(file, &ruleDots)) return 0;
			if (ruleDots.length == 0)
				// check that all characters in a rule with `=` as second operand are
				// defined (or based on another character)
				for (int k = 0; k < ruleChars.length; k++) {
					TranslationTableCharacter *c =
							getChar(ruleChars.chars[k], *table, NULL);
					if (!(c && (c->definitionRule || c->basechar))) {
						compileError(file, "Character %s is not defined",
								_lou_showString(&ruleChars.chars[k], 1, 0));
						return 0;
					}
				}
			TranslationTableRule *r;
			if (!addRule(file, opcode, &ruleChars, &ruleDots, after, before, NULL, &r,
						noback, nofor, table))
				return 0;
			if (nocross) r->nocross = 1;
			return 1;
			// if (opcode == CTO_MidNum)
			// {
			//   TranslationTableCharacter *c = getChar(ruleChars.chars[0]);
			//   if(c)
			//     c->attributes |= CTC_NumericMode;
			// }
		case CTO_RepEndWord:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			CharsString dots;
			if (!getToken(file, &dots, "dots,dots operand")) return 0;
			int len = dots.length;
			for (int k = 0; k < len - 1; k++) {
				if (dots.chars[k] == ',') {
					dots.length = k;
					if (!parseDots(file, &ruleDots, &dots)) return 0;
					ruleDots.chars[ruleDots.length++] = ',';
					k++;
					if (k == len - 1 && dots.chars[k] == '=') {
						// check that all characters are defined (or based on another
						// character)
						for (int l = 0; l < ruleChars.length; l++) {
							TranslationTableCharacter *c =
									getChar(ruleChars.chars[l], *table, NULL);
							if (!(c && (c->definitionRule || c->basechar))) {
								compileError(file, "Character %s is not defined",
										_lou_showString(&ruleChars.chars[l], 1, 0));
								return 0;
							}
						}
					} else {
						CharsString x, y;
						x.length = 0;
						while (k < len) x.chars[x.length++] = dots.chars[k++];
						if (parseDots(file, &y, &x))
							for (int l = 0; l < y.length; l++)
								ruleDots.chars[ruleDots.length++] = y.chars[l];
					}
					return addRule(file, opcode, &ruleChars, &ruleDots, after, before,
							NULL, NULL, noback, nofor, table);
				}
			}
			return 0;
		case CTO_CompDots:
		case CTO_Comp6: {
			TranslationTableOffset ruleOffset;
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			if (ruleChars.length != 1) {
				compileError(file, "first operand must be 1 character");
				return 0;
			}
			if (nofor || noback) {
				compileWarning(file, "nofor and noback not allowed on comp6 rules");
			}
			if (!getRuleDotsPattern(file, &ruleDots)) return 0;
			if (!addRule(file, opcode, &ruleChars, &ruleDots, after, before, &ruleOffset,
						NULL, noback, nofor, table))
				return 0;
			return 1;
		}
		case CTO_ExactDots:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			if (ruleChars.chars[0] != '@') {
				compileError(file, "The operand must begin with an at sign (@)");
				return 0;
			}
			for (int k = 1; k < ruleChars.length; k++)
				scratchPad.chars[k - 1] = ruleChars.chars[k];
			scratchPad.length = ruleChars.length - 1;
			if (!parseDots(file, &ruleDots, &scratchPad)) return 0;
			return addRule(file, opcode, &ruleChars, &ruleDots, before, after, NULL, NULL,
					noback, nofor, table);
		case CTO_CapsNoCont: {
			TranslationTableOffset ruleOffset;
			ruleChars.length = 1;
			ruleChars.chars[0] = 'a';
			if (!addRule(file, CTO_CapsNoContRule, &ruleChars, NULL, after, before,
						&ruleOffset, NULL, noback, nofor, table))
				return 0;
			(*table)->capsNoCont = ruleOffset;
			return 1;
		}
		case CTO_Replace:
			if (getRuleCharsText(file, &ruleChars)) {
				if (atEndOfLine(file))
					ruleDots.length = ruleDots.chars[0] = 0;
				else {
					getRuleDotsText(file, &ruleDots);
					if (ruleDots.chars[0] == '#')
						ruleDots.length = ruleDots.chars[0] = 0;
					else if (ruleDots.chars[0] == '\\' && ruleDots.chars[1] == '#')
						memmove(&ruleDots.chars[0], &ruleDots.chars[1],
								ruleDots.length-- * CHARSIZE);
				}
			}
			for (int k = 0; k < ruleChars.length; k++)
				putChar(file, ruleChars.chars[k], table, NULL);
			for (int k = 0; k < ruleDots.length; k++)
				putChar(file, ruleDots.chars[k], table, NULL);
			return addRule(file, opcode, &ruleChars, &ruleDots, after, before, NULL, NULL,
					noback, nofor, table);
		case CTO_Correct:
			(*table)->corrections = 1;
			goto doPass;
		case CTO_Pass2:
			if ((*table)->numPasses < 2) (*table)->numPasses = 2;
			goto doPass;
		case CTO_Pass3:
			if ((*table)->numPasses < 3) (*table)->numPasses = 3;
			goto doPass;
		case CTO_Pass4:
			if ((*table)->numPasses < 4) (*table)->numPasses = 4;
		doPass:
		case CTO_Context:
			if (!(nofor || noback)) {
				compileError(file, "%s or %s must be specified.",
						_lou_findOpcodeName(CTO_NoFor), _lou_findOpcodeName(CTO_NoBack));
				return 0;
			}
			return compilePassOpcode(file, opcode, noback, nofor, table);
		case CTO_Contraction:
		case CTO_NoCont:
		case CTO_CompBrl:
		case CTO_Literal:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			// check that all characters in a compbrl, contraction,
			// nocont or literal rule are defined (or based on another
			// character)
			for (int k = 0; k < ruleChars.length; k++) {
				TranslationTableCharacter *c = getChar(ruleChars.chars[k], *table, NULL);
				if (!(c && (c->definitionRule || c->basechar))) {
					compileError(file, "Character %s is not defined",
							_lou_showString(&ruleChars.chars[k], 1, 0));
					return 0;
				}
			}
			return addRule(file, opcode, &ruleChars, NULL, after, before, NULL, NULL,
					noback, nofor, table);
		case CTO_MultInd: {
			ruleChars.length = 0;
			if (!getToken(file, &token, "multiple braille indicators") ||
					!parseDots(file, &cells, &token))
				return 0;
			while (getToken(file, &token, "multind opcodes")) {
				opcode = getOpcode(file, &token);
				if (opcode == CTO_None) {
					compileError(file, "opcode %s not defined.",
							_lou_showString(token.chars, token.length, 0));
					return 0;
				}
				if (!(opcode >= CTO_CapsLetter && opcode < CTO_MultInd)) {
					compileError(file, "Not a braille indicator opcode.");
					return 0;
				}
				ruleChars.chars[ruleChars.length++] = (widechar)opcode;
				if (atEndOfLine(file)) break;
			}
			return addRule(file, CTO_MultInd, &ruleChars, &cells, after, before, NULL,
					NULL, noback, nofor, table);
		}

		case CTO_Class:
			compileWarning(file, "class is deprecated, use attribute instead");
		case CTO_Attribute: {
			if (nofor || noback) {
				compileWarning(
						file, "nofor and noback not allowed before class/attribute");
			}
			if ((opcode == CTO_Class && (*table)->usesAttributeOrClass == 1) ||
					(opcode == CTO_Attribute && (*table)->usesAttributeOrClass == 2)) {
				compileError(file,
						"attribute and class rules must not be both present in a table");
				return 0;
			}
			if (opcode == CTO_Class)
				(*table)->usesAttributeOrClass = 2;
			else
				(*table)->usesAttributeOrClass = 1;
			if (!getToken(file, &token, "attribute name")) {
				compileError(file, "Expected %s", "attribute name");
				return 0;
			}
			if (!(*table)->characterClasses && !allocateCharacterClasses(*table)) {
				return 0;
			}

			TranslationTableCharacterAttributes attribute = 0;
			{
				int attrNumber = -1;
				switch (token.chars[0]) {
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					attrNumber = token.chars[0] - '0';
					break;
				}
				if (attrNumber >= 0) {
					if (opcode == CTO_Class) {
						compileError(file,
								"Invalid class name: may not contain digits, use "
								"attribute instead of class");
						return 0;
					}
					if (token.length > 1 || attrNumber > 7) {
						compileError(file,
								"Invalid attribute name: must be a digit between 0 and 7 "
								"or a word containing only letters");
						return 0;
					}
					if (!(*table)->numberedAttributes[attrNumber])
						// attribute not used before yet: assign it a value
						(*table)->numberedAttributes[attrNumber] =
								getNextNumberedAttribute(*table);
					attribute = (*table)->numberedAttributes[attrNumber];
				} else {
					const CharacterClass *namedAttr = findCharacterClass(&token, *table);
					if (!namedAttr) {
						// no class with that name: create one
						namedAttr = addCharacterClass(
								file, &token.chars[0], token.length, *table, 1);
						if (!namedAttr) return 0;
					}
					// there is a class with that name or a new class was successfully
					// created
					attribute = namedAttr->attribute;
					if (attribute == CTC_UpperCase || attribute == CTC_LowerCase)
						attribute |= CTC_Letter;
				}
			}
			CharsString characters;
			if (!getCharacters(file, &characters)) return 0;
			for (int i = 0; i < characters.length; i++) {
				// get the character from the table, or if it is not defined yet,
				// define it
				TranslationTableCharacter *character =
						putChar(file, characters.chars[i], table, NULL);
				// set the attribute
				character->attributes |= attribute;
				// also set the attribute on the associated dots (if any)
				if (character->basechar)
					character = (TranslationTableCharacter *)&(*table)
										->ruleArea[character->basechar];
				if (character->definitionRule) {
					TranslationTableRule *defRule =
							(TranslationTableRule *)&(*table)
									->ruleArea[character->definitionRule];
					if (defRule->dotslen == 1) {
						TranslationTableCharacter *dots =
								getDots(defRule->charsdots[defRule->charslen], *table);
						if (dots) dots->attributes |= attribute;
					}
				}
			}
			return 1;
		}

			{
				TranslationTableCharacterAttributes *attributes;
				const CharacterClass *class;
			case CTO_After:
				attributes = &after;
				goto doBeforeAfter;
			case CTO_Before:
				attributes = &before;
			doBeforeAfter:
				if (!(*table)->characterClasses) {
					if (!allocateCharacterClasses(*table)) return 0;
				}
				if (!getToken(file, &token, "attribute name")) return 0;
				if (!(class = findCharacterClass(&token, *table))) {
					compileError(file, "attribute not defined");
					return 0;
				}
				*attributes |= class->attribute;
				goto doOpcode;
			}
		case CTO_Base:
			if (nofor || noback) {
				compileWarning(file, "nofor and noback not allowed before base");
			}
			if (!getToken(file, &token, "attribute name")) {
				compileError(
						file, "base opcode must be followed by a valid attribute name.");
				return 0;
			}
			if (!(*table)->characterClasses && !allocateCharacterClasses(*table)) {
				return 0;
			}
			const CharacterClass *mode = findCharacterClass(&token, *table);
			if (!mode) {
				mode = addCharacterClass(file, token.chars, token.length, *table, 1);
				if (!mode) return 0;
			}
			if (!(mode->attribute == CTC_UpperCase || mode->attribute == CTC_Digit) &&
					mode->attribute >= CTC_Space && mode->attribute <= CTC_LitDigit) {
				compileError(file,
						"base opcode must be followed by \"uppercase\", \"digit\", or a "
						"custom attribute name.");
				return 0;
			}
			if (!getRuleCharsText(file, &token)) return 0;
			if (token.length != 1) {
				compileError(file,
						"Exactly one character followed by one base character is "
						"required.");
				return 0;
			}
			TranslationTableOffset characterOffset;
			TranslationTableCharacter *character =
					putChar(file, token.chars[0], table, &characterOffset);
			if (!getRuleCharsText(file, &token)) return 0;
			if (token.length != 1) {
				compileError(file, "Exactly one base character is required.");
				return 0;
			}
			if (character->definitionRule) {
				TranslationTableRule *prevRule =
						(TranslationTableRule *)&(*table)
								->ruleArea[character->definitionRule];
				_lou_logMessage(LOU_LOG_DEBUG,
						"%s:%d: Character already defined (%s). The base rule will take "
						"precedence.",
						file->fileName, file->lineNumber,
						printSource(file, prevRule->sourceFile, prevRule->sourceLine));
				character->definitionRule = 0;
			}
			TranslationTableOffset basechar;
			putChar(file, token.chars[0], table, &basechar);
			// putChar may have moved table, so make sure character is still valid
			character = (TranslationTableCharacter *)&(*table)->ruleArea[characterOffset];
			if (character->basechar) {
				if (character->basechar == basechar &&
						character->mode == mode->attribute) {
					_lou_logMessage(LOU_LOG_DEBUG, "%s:%d: Duplicate base rule.",
							file->fileName, file->lineNumber);
				} else {
					_lou_logMessage(LOU_LOG_DEBUG,
							"%s:%d: A different base rule already exists for this "
							"character (%s). The new rule will take precedence.",
							file->fileName, file->lineNumber,
							printSource(
									file, character->sourceFile, character->sourceLine));
				}
			}
			character->basechar = basechar;
			character->mode = mode->attribute;
			character->sourceFile = file->sourceFile;
			character->sourceLine = file->lineNumber;
			/* some other processing is done at the end of the compilation, in
			 * finalizeTable() */
			return 1;
		case CTO_EmpMatchBefore:
			before |= CTC_EmpMatch;
			goto doOpcode;
		case CTO_EmpMatchAfter:
			after |= CTC_EmpMatch;
			goto doOpcode;

		case CTO_SwapCc:
		case CTO_SwapCd:
		case CTO_SwapDd:
			return compileSwap(file, opcode, noback, nofor, table);
		case CTO_Hyphen:
		case CTO_DecPoint:
			//	case CTO_Apostrophe:
			//	case CTO_Initial:
			if (!getRuleCharsText(file, &ruleChars)) return 0;
			if (!getRuleDotsPattern(file, &ruleDots)) return 0;
			if (ruleChars.length != 1 || ruleDots.length < 1) {
				compileError(file,
						"One Unicode character and at least one cell are "
						"required.");
				return 0;
			}
			return addRule(file, opcode, &ruleChars, &ruleDots, after, before, NULL, NULL,
					noback, nofor, table);
			// if (opcode == CTO_DecPoint)
			// {
			//   TranslationTableCharacter *c =
			//   getChar(ruleChars.chars[0]);
			//   if(c)
			//     c->attributes |= CTC_NumericMode;
			// }
		default:
			compileError(file, "unimplemented opcode.");
			return 0;
		}
	}
	return 0;
}