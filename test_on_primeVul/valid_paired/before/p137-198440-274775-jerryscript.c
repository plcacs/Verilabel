scanner_scan_all (parser_context_t *context_p, /**< context */
                  const uint8_t *arg_list_p, /**< function argument list */
                  const uint8_t *arg_list_end_p, /**< end of argument list */
                  const uint8_t *source_p, /**< valid UTF-8 source code */
                  const uint8_t *source_end_p) /**< end of source code */
{
  scanner_context_t scanner_context;

#if ENABLED (JERRY_PARSER_DUMP_BYTE_CODE)
  if (context_p->is_show_opcodes)
  {
    JERRY_DEBUG_MSG ("\n--- Scanning start ---\n\n");
  }
#endif /* ENABLED (JERRY_PARSER_DUMP_BYTE_CODE) */

  scanner_context.context_status_flags = context_p->status_flags;
  scanner_context.status_flags = SCANNER_CONTEXT_NO_FLAGS;
#if ENABLED (JERRY_DEBUGGER)
  if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
  {
    scanner_context.status_flags |= SCANNER_CONTEXT_DEBUGGER_ENABLED;
  }
#endif /* ENABLED (JERRY_DEBUGGER) */
#if ENABLED (JERRY_ES2015)
  scanner_context.binding_type = SCANNER_BINDING_NONE;
  scanner_context.active_binding_list_p = NULL;
#endif /* ENABLED (JERRY_ES2015) */
  scanner_context.active_literal_pool_p = NULL;
  scanner_context.active_switch_statement.last_case_p = NULL;
  scanner_context.end_arguments_p = NULL;
#if ENABLED (JERRY_ES2015)
  scanner_context.async_source_p = NULL;
#endif /* ENABLED (JERRY_ES2015) */

  /* This assignment must be here because of Apple compilers. */
  context_p->u.scanner_context_p = &scanner_context;

  parser_stack_init (context_p);

  PARSER_TRY (context_p->try_buffer)
  {
    context_p->line = 1;
    context_p->column = 1;

    if (arg_list_p == NULL)
    {
      context_p->source_p = source_p;
      context_p->source_end_p = source_end_p;

      uint16_t status_flags = SCANNER_LITERAL_POOL_FUNCTION_WITHOUT_ARGUMENTS | SCANNER_LITERAL_POOL_CAN_EVAL;

      if (context_p->status_flags & PARSER_IS_STRICT)
      {
        status_flags |= SCANNER_LITERAL_POOL_IS_STRICT;
      }

      scanner_literal_pool_t *literal_pool_p = scanner_push_literal_pool (context_p, &scanner_context, status_flags);
      literal_pool_p->source_p = source_p;

      parser_stack_push_uint8 (context_p, SCAN_STACK_SCRIPT);

      lexer_next_token (context_p);
      scanner_check_directives (context_p, &scanner_context);
    }
    else
    {
      context_p->source_p = arg_list_p;
      context_p->source_end_p = arg_list_end_p;

      uint16_t status_flags = SCANNER_LITERAL_POOL_FUNCTION;

      if (context_p->status_flags & PARSER_IS_STRICT)
      {
        status_flags |= SCANNER_LITERAL_POOL_IS_STRICT;
      }

#if ENABLED (JERRY_ES2015)
      if (context_p->status_flags & PARSER_IS_GENERATOR_FUNCTION)
      {
        status_flags |= SCANNER_LITERAL_POOL_GENERATOR;
      }
#endif /* ENABLED (JERRY_ES2015) */

      scanner_push_literal_pool (context_p, &scanner_context, status_flags);
      scanner_context.mode = SCAN_MODE_FUNCTION_ARGUMENTS;
      parser_stack_push_uint8 (context_p, SCAN_STACK_SCRIPT_FUNCTION);

      /* Faking the first token. */
      context_p->token.type = LEXER_LEFT_PAREN;
    }

    while (true)
    {
      lexer_token_type_t type = (lexer_token_type_t) context_p->token.type;
      scan_stack_modes_t stack_top = (scan_stack_modes_t) context_p->stack_top_uint8;

      switch (scanner_context.mode)
      {
        case SCAN_MODE_PRIMARY_EXPRESSION:
        {
          if (type == LEXER_ADD
              || type == LEXER_SUBTRACT
              || LEXER_IS_UNARY_OP_TOKEN (type))
          {
            break;
          }
          /* FALLTHRU */
        }
        case SCAN_MODE_PRIMARY_EXPRESSION_AFTER_NEW:
        {
          if (scanner_scan_primary_expression (context_p, &scanner_context, type, stack_top) != SCAN_NEXT_TOKEN)
          {
            continue;
          }
          break;
        }
#if ENABLED (JERRY_ES2015)
        case SCAN_MODE_CLASS_DECLARATION:
        {
          if (context_p->token.type == LEXER_KEYW_EXTENDS)
          {
            parser_stack_push_uint8 (context_p, SCAN_STACK_CLASS_EXTENDS);
            scanner_context.mode = SCAN_MODE_PRIMARY_EXPRESSION;
            break;
          }
          else if (context_p->token.type != LEXER_LEFT_BRACE)
          {
            scanner_raise_error (context_p);
          }

          scanner_context.mode = SCAN_MODE_CLASS_METHOD;
          /* FALLTHRU */
        }
        case SCAN_MODE_CLASS_METHOD:
        {
          JERRY_ASSERT (stack_top == SCAN_STACK_IMPLICIT_CLASS_CONSTRUCTOR
                        || stack_top == SCAN_STACK_EXPLICIT_CLASS_CONSTRUCTOR);

          lexer_skip_empty_statements (context_p);

          lexer_scan_identifier (context_p);

          if (context_p->token.type == LEXER_RIGHT_BRACE)
          {
            scanner_source_start_t source_start;

            parser_stack_pop_uint8 (context_p);

            if (stack_top == SCAN_STACK_IMPLICIT_CLASS_CONSTRUCTOR)
            {
              parser_stack_pop (context_p, &source_start, sizeof (scanner_source_start_t));
            }

            stack_top = context_p->stack_top_uint8;

            JERRY_ASSERT (stack_top == SCAN_STACK_CLASS_STATEMENT || stack_top == SCAN_STACK_CLASS_EXPRESSION);

            if (stack_top == SCAN_STACK_CLASS_STATEMENT)
            {
              /* The token is kept to disallow consuming a semicolon after it. */
              scanner_context.mode = SCAN_MODE_STATEMENT_END;
              continue;
            }

            scanner_context.mode = SCAN_MODE_POST_PRIMARY_EXPRESSION;
            parser_stack_pop_uint8 (context_p);
            break;
          }

          if (context_p->token.type == LEXER_LITERAL
              && LEXER_IS_IDENT_OR_STRING (context_p->token.lit_location.type)
              && lexer_compare_literal_to_string (context_p, "constructor", 11))
          {
            if (stack_top == SCAN_STACK_IMPLICIT_CLASS_CONSTRUCTOR)
            {
              scanner_source_start_t source_start;
              parser_stack_pop_uint8 (context_p);
              parser_stack_pop (context_p, &source_start, sizeof (scanner_source_start_t));

              scanner_info_t *info_p = scanner_insert_info (context_p, source_start.source_p, sizeof (scanner_info_t));
              info_p->type = SCANNER_TYPE_CLASS_CONSTRUCTOR;
              parser_stack_push_uint8 (context_p, SCAN_STACK_EXPLICIT_CLASS_CONSTRUCTOR);
            }
          }

          if (lexer_token_is_identifier (context_p, "static", 6))
          {
            lexer_scan_identifier (context_p);
          }

          parser_stack_push_uint8 (context_p, SCAN_STACK_FUNCTION_PROPERTY);
          scanner_context.mode = SCAN_MODE_FUNCTION_ARGUMENTS;

          uint16_t literal_pool_flags = SCANNER_LITERAL_POOL_FUNCTION;

          if (lexer_token_is_identifier (context_p, "get", 3)
              || lexer_token_is_identifier (context_p, "set", 3))
          {
            lexer_scan_identifier (context_p);

            if (context_p->token.type == LEXER_LEFT_PAREN)
            {
              scanner_push_literal_pool (context_p, &scanner_context, SCANNER_LITERAL_POOL_FUNCTION);
              continue;
            }
          }
          else if (lexer_token_is_identifier (context_p, "async", 5))
          {
            lexer_scan_identifier (context_p);

            if (context_p->token.type == LEXER_LEFT_PAREN)
            {
              scanner_push_literal_pool (context_p, &scanner_context, SCANNER_LITERAL_POOL_FUNCTION);
              continue;
            }

            literal_pool_flags |= SCANNER_LITERAL_POOL_ASYNC;

            if (context_p->token.type == LEXER_MULTIPLY)
            {
              lexer_scan_identifier (context_p);
              literal_pool_flags |= SCANNER_LITERAL_POOL_GENERATOR;
            }
          }
          else if (context_p->token.type == LEXER_MULTIPLY)
          {
            lexer_scan_identifier (context_p);
            literal_pool_flags |= SCANNER_LITERAL_POOL_GENERATOR;
          }

          if (context_p->token.type == LEXER_LEFT_SQUARE)
          {
            parser_stack_push_uint8 (context_p, SCANNER_FROM_LITERAL_POOL_TO_COMPUTED (literal_pool_flags));
            scanner_context.mode = SCAN_MODE_PRIMARY_EXPRESSION;
            break;
          }

          if (context_p->token.type != LEXER_LITERAL)
          {
            scanner_raise_error (context_p);
          }

          if (literal_pool_flags & SCANNER_LITERAL_POOL_GENERATOR)
          {
            context_p->status_flags |= PARSER_IS_GENERATOR_FUNCTION;
          }

          scanner_push_literal_pool (context_p, &scanner_context, literal_pool_flags);
          lexer_next_token (context_p);
          continue;
        }
#endif /* ENABLED (JERRY_ES2015) */
        case SCAN_MODE_POST_PRIMARY_EXPRESSION:
        {
          if (scanner_scan_post_primary_expression (context_p, &scanner_context, type, stack_top))
          {
            break;
          }
          type = (lexer_token_type_t) context_p->token.type;
          /* FALLTHRU */
        }
        case SCAN_MODE_PRIMARY_EXPRESSION_END:
        {
          if (scanner_scan_primary_expression_end (context_p, &scanner_context, type, stack_top) != SCAN_NEXT_TOKEN)
          {
            continue;
          }
          break;
        }
        case SCAN_MODE_STATEMENT_OR_TERMINATOR:
        {
          if (type == LEXER_RIGHT_BRACE || type == LEXER_EOS)
          {
            scanner_context.mode = SCAN_MODE_STATEMENT_END;
            continue;
          }
          /* FALLTHRU */
        }
        case SCAN_MODE_STATEMENT:
        {
          if (scanner_scan_statement (context_p, &scanner_context, type, stack_top) != SCAN_NEXT_TOKEN)
          {
            continue;
          }
          break;
        }
        case SCAN_MODE_STATEMENT_END:
        {
          if (scanner_scan_statement_end (context_p, &scanner_context, type) != SCAN_NEXT_TOKEN)
          {
            continue;
          }

          if (context_p->token.type == LEXER_EOS)
          {
            goto scan_completed;
          }

          break;
        }
        case SCAN_MODE_VAR_STATEMENT:
        {
#if ENABLED (JERRY_ES2015)
          if (type == LEXER_LEFT_SQUARE || type == LEXER_LEFT_BRACE)
          {
            uint8_t binding_type = SCANNER_BINDING_VAR;

            if (stack_top == SCAN_STACK_LET || stack_top == SCAN_STACK_FOR_LET_START)
            {
              binding_type = SCANNER_BINDING_LET;
            }
            else if (stack_top == SCAN_STACK_CONST || stack_top == SCAN_STACK_FOR_CONST_START)
            {
              binding_type = SCANNER_BINDING_CONST;
            }

            scanner_push_destructuring_pattern (context_p, &scanner_context, binding_type, false);

            if (type == LEXER_LEFT_SQUARE)
            {
              parser_stack_push_uint8 (context_p, SCAN_STACK_ARRAY_LITERAL);
              scanner_context.mode = SCAN_MODE_BINDING;
              break;
            }

            parser_stack_push_uint8 (context_p, SCAN_STACK_OBJECT_LITERAL);
            scanner_context.mode = SCAN_MODE_PROPERTY_NAME;
            continue;
          }
#endif /* ENABLED (JERRY_ES2015) */

          if (type != LEXER_LITERAL
              || context_p->token.lit_location.type != LEXER_IDENT_LITERAL)
          {
            scanner_raise_error (context_p);
          }

          lexer_lit_location_t *literal_p = scanner_add_literal (context_p, &scanner_context);

#if ENABLED (JERRY_ES2015)
          if (stack_top != SCAN_STACK_VAR && stack_top != SCAN_STACK_FOR_VAR_START)
          {
            scanner_detect_invalid_let (context_p, literal_p);

            if (stack_top == SCAN_STACK_LET || stack_top == SCAN_STACK_FOR_LET_START)
            {
              literal_p->type |= SCANNER_LITERAL_IS_LET;
            }
            else
            {
              JERRY_ASSERT (stack_top == SCAN_STACK_CONST || stack_top == SCAN_STACK_FOR_CONST_START);
              literal_p->type |= SCANNER_LITERAL_IS_CONST;
            }

            lexer_next_token (context_p);

            if (literal_p->type & SCANNER_LITERAL_IS_USED)
            {
              literal_p->type |= SCANNER_LITERAL_EARLY_CREATE;
            }
            else if (context_p->token.type == LEXER_ASSIGN)
            {
              scanner_binding_literal_t binding_literal;
              binding_literal.literal_p = literal_p;

              parser_stack_push (context_p, &binding_literal, sizeof (scanner_binding_literal_t));
              parser_stack_push_uint8 (context_p, SCAN_STACK_BINDING_INIT);
            }
          }
          else
          {
            if (!(literal_p->type & SCANNER_LITERAL_IS_VAR))
            {
              scanner_detect_invalid_var (context_p, &scanner_context, literal_p);
              literal_p->type |= SCANNER_LITERAL_IS_VAR;

              if (scanner_context.active_literal_pool_p->status_flags & SCANNER_LITERAL_POOL_IN_WITH)
              {
                literal_p->type |= SCANNER_LITERAL_NO_REG;
              }
            }

            lexer_next_token (context_p);
          }
#else /* !ENABLED (JERRY_ES2015) */
          literal_p->type |= SCANNER_LITERAL_IS_VAR;

          if (scanner_context.active_literal_pool_p->status_flags & SCANNER_LITERAL_POOL_IN_WITH)
          {
            literal_p->type |= SCANNER_LITERAL_NO_REG;
          }

          lexer_next_token (context_p);
#endif /* ENABLED (JERRY_ES2015) */

#if ENABLED (JERRY_ES2015_MODULE_SYSTEM)
          if (scanner_context.active_literal_pool_p->status_flags & SCANNER_LITERAL_POOL_IN_EXPORT)
          {
            literal_p->type |= SCANNER_LITERAL_NO_REG;
          }
#endif /* ENABLED (JERRY_ES2015_MODULE_SYSTEM) */

          switch (context_p->token.type)
          {
            case LEXER_ASSIGN:
            {
              scanner_context.mode = SCAN_MODE_PRIMARY_EXPRESSION;
              /* FALLTHRU */
            }
            case LEXER_COMMA:
            {
              lexer_next_token (context_p);
              continue;
            }
          }

          if (SCANNER_IS_FOR_START (stack_top))
          {
#if ENABLED (JERRY_ES2015_MODULE_SYSTEM)
            JERRY_ASSERT (!(scanner_context.active_literal_pool_p->status_flags & SCANNER_LITERAL_POOL_IN_EXPORT));
#endif /* ENABLED (JERRY_ES2015_MODULE_SYSTEM) */

            if (context_p->token.type != LEXER_SEMICOLON
                && context_p->token.type != LEXER_KEYW_IN
                && !SCANNER_IDENTIFIER_IS_OF ())
            {
              scanner_raise_error (context_p);
            }

            scanner_context.mode = SCAN_MODE_PRIMARY_EXPRESSION_END;
            continue;
          }

#if ENABLED (JERRY_ES2015)
          JERRY_ASSERT (stack_top == SCAN_STACK_VAR || stack_top == SCAN_STACK_LET || stack_top == SCAN_STACK_CONST);
#else /* !ENABLED (JERRY_ES2015) */
          JERRY_ASSERT (stack_top == SCAN_STACK_VAR);
#endif /* ENABLED (JERRY_ES2015) */

#if ENABLED (JERRY_ES2015_MODULE_SYSTEM)
          scanner_context.active_literal_pool_p->status_flags &= (uint16_t) ~SCANNER_LITERAL_POOL_IN_EXPORT;
#endif /* ENABLED (JERRY_ES2015_MODULE_SYSTEM) */

          scanner_context.mode = SCAN_MODE_STATEMENT_END;
          parser_stack_pop_uint8 (context_p);
          continue;
        }
        case SCAN_MODE_FUNCTION_ARGUMENTS:
        {
          JERRY_ASSERT (stack_top == SCAN_STACK_SCRIPT_FUNCTION
                        || stack_top == SCAN_STACK_FUNCTION_STATEMENT
                        || stack_top == SCAN_STACK_FUNCTION_EXPRESSION
                        || stack_top == SCAN_STACK_FUNCTION_PROPERTY);

          scanner_literal_pool_t *literal_pool_p = scanner_context.active_literal_pool_p;

          JERRY_ASSERT (literal_pool_p != NULL && (literal_pool_p->status_flags & SCANNER_LITERAL_POOL_FUNCTION));

          literal_pool_p->source_p = context_p->source_p;

#if ENABLED (JERRY_ES2015)
          if (JERRY_UNLIKELY (scanner_context.async_source_p != NULL))
          {
            literal_pool_p->status_flags |= SCANNER_LITERAL_POOL_ASYNC;
            literal_pool_p->source_p = scanner_context.async_source_p;
            scanner_context.async_source_p = NULL;
          }
#endif /* ENABLED (JERRY_ES2015) */

          if (type != LEXER_LEFT_PAREN)
          {
            scanner_raise_error (context_p);
          }
          lexer_next_token (context_p);

#if ENABLED (JERRY_ES2015)
          /* FALLTHRU */
        }
        case SCAN_MODE_CONTINUE_FUNCTION_ARGUMENTS:
        {
#endif /* ENABLED (JERRY_ES2015) */
          if (context_p->token.type != LEXER_RIGHT_PAREN && context_p->token.type != LEXER_EOS)
          {
#if ENABLED (JERRY_ES2015)
            lexer_lit_location_t *argument_literal_p;
#endif /* ENABLED (JERRY_ES2015) */

            while (true)
            {
#if ENABLED (JERRY_ES2015)
              if (context_p->token.type == LEXER_THREE_DOTS)
              {
                scanner_context.active_literal_pool_p->status_flags |= SCANNER_LITERAL_POOL_ARGUMENTS_UNMAPPED;

                lexer_next_token (context_p);
              }

              if (context_p->token.type == LEXER_LEFT_SQUARE || context_p->token.type == LEXER_LEFT_BRACE)
              {
                argument_literal_p = NULL;
                break;
              }
#endif /* ENABLED (JERRY_ES2015) */

              if (context_p->token.type != LEXER_LITERAL
                  || context_p->token.lit_location.type != LEXER_IDENT_LITERAL)
              {
                scanner_raise_error (context_p);
              }

#if ENABLED (JERRY_ES2015)
              argument_literal_p = scanner_append_argument (context_p, &scanner_context);
#else /* !ENABLED (JERRY_ES2015) */
              scanner_append_argument (context_p, &scanner_context);
#endif /* ENABLED (JERRY_ES2015) */

              lexer_next_token (context_p);

              if (context_p->token.type != LEXER_COMMA)
              {
                break;
              }
              lexer_next_token (context_p);
            }

#if ENABLED (JERRY_ES2015)
            if (argument_literal_p == NULL)
            {
              scanner_context.active_literal_pool_p->status_flags |= SCANNER_LITERAL_POOL_ARGUMENTS_UNMAPPED;

              parser_stack_push_uint8 (context_p, SCAN_STACK_FUNCTION_PARAMETERS);
              scanner_append_hole (context_p, &scanner_context);
              scanner_push_destructuring_pattern (context_p, &scanner_context, SCANNER_BINDING_ARG, false);

              if (context_p->token.type == LEXER_LEFT_SQUARE)
              {
                parser_stack_push_uint8 (context_p, SCAN_STACK_ARRAY_LITERAL);
                scanner_context.mode = SCAN_MODE_BINDING;
                break;
              }

              parser_stack_push_uint8 (context_p, SCAN_STACK_OBJECT_LITERAL);
              scanner_context.mode = SCAN_MODE_PROPERTY_NAME;
              continue;
            }

            if (context_p->token.type == LEXER_ASSIGN)
            {
              scanner_context.active_literal_pool_p->status_flags |= SCANNER_LITERAL_POOL_ARGUMENTS_UNMAPPED;

              parser_stack_push_uint8 (context_p, SCAN_STACK_FUNCTION_PARAMETERS);
              scanner_context.mode = SCAN_MODE_PRIMARY_EXPRESSION;

              if (argument_literal_p->type & SCANNER_LITERAL_IS_USED)
              {
                JERRY_ASSERT (argument_literal_p->type & SCANNER_LITERAL_EARLY_CREATE);
                break;
              }

              scanner_binding_literal_t binding_literal;
              binding_literal.literal_p = argument_literal_p;

              parser_stack_push (context_p, &binding_literal, sizeof (scanner_binding_literal_t));
              parser_stack_push_uint8 (context_p, SCAN_STACK_BINDING_INIT);
              break;
            }
#endif /* ENABLED (JERRY_ES2015) */
          }

          if (context_p->token.type == LEXER_EOS && stack_top == SCAN_STACK_SCRIPT_FUNCTION)
          {
            /* End of argument parsing. */
            scanner_info_t *scanner_info_p = (scanner_info_t *) scanner_malloc (context_p, sizeof (scanner_info_t));
            scanner_info_p->next_p = context_p->next_scanner_info_p;
            scanner_info_p->source_p = NULL;
            scanner_info_p->type = SCANNER_TYPE_END_ARGUMENTS;
            scanner_context.end_arguments_p = scanner_info_p;

            context_p->next_scanner_info_p = scanner_info_p;
            context_p->source_p = source_p;
            context_p->source_end_p = source_end_p;
            context_p->line = 1;
            context_p->column = 1;

            scanner_filter_arguments (context_p, &scanner_context);
            lexer_next_token (context_p);
            scanner_check_directives (context_p, &scanner_context);
            continue;
          }

          if (context_p->token.type != LEXER_RIGHT_PAREN)
          {
            scanner_raise_error (context_p);
          }

          lexer_next_token (context_p);

          if (context_p->token.type != LEXER_LEFT_BRACE)
          {
            scanner_raise_error (context_p);
          }

          scanner_filter_arguments (context_p, &scanner_context);
          lexer_next_token (context_p);
          scanner_check_directives (context_p, &scanner_context);
          continue;
        }
        case SCAN_MODE_PROPERTY_NAME:
        {
          JERRY_ASSERT (stack_top == SCAN_STACK_OBJECT_LITERAL);

          if (lexer_scan_identifier (context_p))
          {
            lexer_check_property_modifier (context_p);
          }

#if ENABLED (JERRY_ES2015)
          if (context_p->token.type == LEXER_LEFT_SQUARE)
          {
            parser_stack_push_uint8 (context_p, SCAN_STACK_COMPUTED_PROPERTY);
            scanner_context.mode = SCAN_MODE_PRIMARY_EXPRESSION;
            break;
          }
#endif /* ENABLED (JERRY_ES2015) */

          if (context_p->token.type == LEXER_RIGHT_BRACE)
          {
            scanner_context.mode = SCAN_MODE_PRIMARY_EXPRESSION_END;
            continue;
          }

          if (context_p->token.type == LEXER_PROPERTY_GETTER
#if ENABLED (JERRY_ES2015)
              || context_p->token.type == LEXER_KEYW_ASYNC
              || context_p->token.type == LEXER_MULTIPLY
#endif /* ENABLED (JERRY_ES2015) */
              || context_p->token.type == LEXER_PROPERTY_SETTER)
          {
            uint16_t literal_pool_flags = SCANNER_LITERAL_POOL_FUNCTION;

#if ENABLED (JERRY_ES2015)
            if (context_p->token.type == LEXER_MULTIPLY)
            {
              literal_pool_flags |= SCANNER_LITERAL_POOL_GENERATOR;
            }
            else if (context_p->token.type == LEXER_KEYW_ASYNC)
            {
              literal_pool_flags |= SCANNER_LITERAL_POOL_ASYNC;

              if (lexer_consume_generator (context_p))
              {
                literal_pool_flags |= SCANNER_LITERAL_POOL_GENERATOR;
              }
            }
#endif /* ENABLED (JERRY_ES2015) */

            parser_stack_push_uint8 (context_p, SCAN_STACK_FUNCTION_PROPERTY);
            lexer_scan_identifier (context_p);

#if ENABLED (JERRY_ES2015)
            if (context_p->token.type == LEXER_LEFT_SQUARE)
            {
              parser_stack_push_uint8 (context_p, SCANNER_FROM_LITERAL_POOL_TO_COMPUTED (literal_pool_flags));
              scanner_context.mode = SCAN_MODE_PRIMARY_EXPRESSION;
              break;
            }
#endif /* ENABLED (JERRY_ES2015) */

            if (context_p->token.type != LEXER_LITERAL)
            {
              scanner_raise_error (context_p);
            }

            scanner_push_literal_pool (context_p, &scanner_context, literal_pool_flags);
            scanner_context.mode = SCAN_MODE_FUNCTION_ARGUMENTS;
            break;
          }

          if (context_p->token.type != LEXER_LITERAL)
          {
            scanner_raise_error (context_p);
          }

#if ENABLED (JERRY_ES2015)
          parser_line_counter_t start_line = context_p->token.line;
          parser_line_counter_t start_column = context_p->token.column;
          bool is_ident = (context_p->token.lit_location.type == LEXER_IDENT_LITERAL);
#endif /* ENABLED (JERRY_ES2015) */

          lexer_next_token (context_p);

#if ENABLED (JERRY_ES2015)
          if (context_p->token.type == LEXER_LEFT_PAREN)
          {
            scanner_push_literal_pool (context_p, &scanner_context, SCANNER_LITERAL_POOL_FUNCTION);

            parser_stack_push_uint8 (context_p, SCAN_STACK_FUNCTION_PROPERTY);
            scanner_context.mode = SCAN_MODE_FUNCTION_ARGUMENTS;
            continue;
          }

          if (is_ident
              && (context_p->token.type == LEXER_COMMA
                  || context_p->token.type == LEXER_RIGHT_BRACE
                  || context_p->token.type == LEXER_ASSIGN))
          {
            context_p->source_p = context_p->token.lit_location.char_p;
            context_p->line = start_line;
            context_p->column = start_column;

            lexer_next_token (context_p);

            JERRY_ASSERT (context_p->token.type != LEXER_LITERAL
                          || context_p->token.lit_location.type == LEXER_IDENT_LITERAL);

            if (context_p->token.type != LEXER_LITERAL)
            {
              scanner_raise_error (context_p);
            }

            if (scanner_context.binding_type != SCANNER_BINDING_NONE)
            {
              scanner_context.mode = SCAN_MODE_BINDING;
              continue;
            }

            scanner_add_reference (context_p, &scanner_context);

            lexer_next_token (context_p);

            if (context_p->token.type == LEXER_ASSIGN)
            {
              scanner_context.mode = SCAN_MODE_PRIMARY_EXPRESSION;
              break;
            }

            scanner_context.mode = SCAN_MODE_PRIMARY_EXPRESSION_END;
            continue;
          }
#endif /* ENABLED (JERRY_ES2015) */

          if (context_p->token.type != LEXER_COLON)
          {
            scanner_raise_error (context_p);
          }

          scanner_context.mode = SCAN_MODE_PRIMARY_EXPRESSION;

#if ENABLED (JERRY_ES2015)
          if (scanner_context.binding_type != SCANNER_BINDING_NONE)
          {
            scanner_context.mode = SCAN_MODE_BINDING;
          }
#endif /* ENABLED (JERRY_ES2015) */
          break;
        }
#if ENABLED (JERRY_ES2015)
        case SCAN_MODE_BINDING:
        {
          JERRY_ASSERT (scanner_context.binding_type == SCANNER_BINDING_VAR
                        || scanner_context.binding_type == SCANNER_BINDING_LET
                        || scanner_context.binding_type == SCANNER_BINDING_CATCH
                        || scanner_context.binding_type == SCANNER_BINDING_CONST
                        || scanner_context.binding_type == SCANNER_BINDING_ARG
                        || scanner_context.binding_type == SCANNER_BINDING_ARROW_ARG);

          if (type == LEXER_THREE_DOTS)
          {
            lexer_next_token (context_p);
            type = (lexer_token_type_t) context_p->token.type;
          }

          if (type == LEXER_LEFT_SQUARE || type == LEXER_LEFT_BRACE)
          {
            scanner_push_destructuring_pattern (context_p, &scanner_context, scanner_context.binding_type, true);

            if (type == LEXER_LEFT_SQUARE)
            {
              parser_stack_push_uint8 (context_p, SCAN_STACK_ARRAY_LITERAL);
              break;
            }

            parser_stack_push_uint8 (context_p, SCAN_STACK_OBJECT_LITERAL);
            scanner_context.mode = SCAN_MODE_PROPERTY_NAME;
            continue;
          }

          if (type != LEXER_LITERAL || context_p->token.lit_location.type != LEXER_IDENT_LITERAL)
          {
            scanner_context.mode = SCAN_MODE_PRIMARY_EXPRESSION;
            continue;
          }

          lexer_lit_location_t *literal_p = scanner_add_literal (context_p, &scanner_context);

          scanner_context.mode = SCAN_MODE_POST_PRIMARY_EXPRESSION;

          if (scanner_context.binding_type == SCANNER_BINDING_VAR)
          {
            if (!(literal_p->type & SCANNER_LITERAL_IS_VAR))
            {
              scanner_detect_invalid_var (context_p, &scanner_context, literal_p);
              literal_p->type |= SCANNER_LITERAL_IS_VAR;

              if (scanner_context.active_literal_pool_p->status_flags & SCANNER_LITERAL_POOL_IN_WITH)
              {
                literal_p->type |= SCANNER_LITERAL_NO_REG;
              }
            }
            break;
          }

          if (scanner_context.binding_type == SCANNER_BINDING_ARROW_ARG)
          {
            literal_p->type |= SCANNER_LITERAL_IS_ARG | SCANNER_LITERAL_IS_ARROW_DESTRUCTURED_ARG;

            if (literal_p->type & SCANNER_LITERAL_IS_USED)
            {
              literal_p->type |= SCANNER_LITERAL_EARLY_CREATE;
              break;
            }
          }
          else
          {
            scanner_detect_invalid_let (context_p, literal_p);

            if (scanner_context.binding_type <= SCANNER_BINDING_CATCH)
            {
              JERRY_ASSERT ((scanner_context.binding_type == SCANNER_BINDING_LET)
                            || (scanner_context.binding_type == SCANNER_BINDING_CATCH));

              literal_p->type |= SCANNER_LITERAL_IS_LET;
            }
            else
            {
              literal_p->type |= SCANNER_LITERAL_IS_CONST;

              if (scanner_context.binding_type == SCANNER_BINDING_ARG)
              {
                literal_p->type |= SCANNER_LITERAL_IS_ARG;

                if (literal_p->type & SCANNER_LITERAL_IS_USED)
                {
                  literal_p->type |= SCANNER_LITERAL_EARLY_CREATE;
                  break;
                }
              }
            }

            if (literal_p->type & SCANNER_LITERAL_IS_USED)
            {
              literal_p->type |= SCANNER_LITERAL_EARLY_CREATE;
              break;
            }
          }

          scanner_binding_item_t *binding_item_p;
          binding_item_p = (scanner_binding_item_t *) scanner_malloc (context_p, sizeof (scanner_binding_item_t));

          binding_item_p->next_p = scanner_context.active_binding_list_p->items_p;
          binding_item_p->literal_p = literal_p;

          scanner_context.active_binding_list_p->items_p = binding_item_p;

          lexer_next_token (context_p);
          if (context_p->token.type != LEXER_ASSIGN)
          {
            continue;
          }

          scanner_binding_literal_t binding_literal;
          binding_literal.literal_p = literal_p;

          parser_stack_push (context_p, &binding_literal, sizeof (scanner_binding_literal_t));
          parser_stack_push_uint8 (context_p, SCAN_STACK_BINDING_INIT);

          scanner_context.mode = SCAN_MODE_PRIMARY_EXPRESSION;
          break;
        }
#endif /* ENABLED (JERRY_ES2015) */
      }

      lexer_next_token (context_p);
    }

scan_completed:
    if (context_p->stack_top_uint8 != SCAN_STACK_SCRIPT
        && context_p->stack_top_uint8 != SCAN_STACK_SCRIPT_FUNCTION)
    {
      scanner_raise_error (context_p);
    }

    scanner_pop_literal_pool (context_p, &scanner_context);

#if ENABLED (JERRY_ES2015)
    JERRY_ASSERT (scanner_context.active_binding_list_p == NULL);
#endif /* ENABLED (JERRY_ES2015) */
    JERRY_ASSERT (scanner_context.active_literal_pool_p == NULL);

#ifndef JERRY_NDEBUG
    scanner_context.context_status_flags |= PARSER_SCANNING_SUCCESSFUL;
#endif /* !JERRY_NDEBUG */
  }
  PARSER_CATCH
  {
    /* Ignore the errors thrown by the lexer. */
    if (context_p->error != PARSER_ERR_OUT_OF_MEMORY)
    {
      context_p->error = PARSER_ERR_NO_ERROR;
    }

#if ENABLED (JERRY_ES2015)
    while (scanner_context.active_binding_list_p != NULL)
    {
      scanner_pop_binding_list (&scanner_context);
    }
#endif /* ENABLED (JERRY_ES2015) */

    /* The following code may allocate memory, so it is enclosed in a try/catch. */
    PARSER_TRY (context_p->try_buffer)
    {
#if ENABLED (JERRY_ES2015)
      if (scanner_context.status_flags & SCANNER_CONTEXT_THROW_ERR_ASYNC_FUNCTION)
      {
        JERRY_ASSERT (scanner_context.async_source_p != NULL);

        scanner_info_t *info_p;
        info_p = scanner_insert_info (context_p, scanner_context.async_source_p, sizeof (scanner_info_t));
        info_p->type = SCANNER_TYPE_ERR_ASYNC_FUNCTION;
      }
#endif /* ENABLED (JERRY_ES2015) */

      while (scanner_context.active_literal_pool_p != NULL)
      {
        scanner_pop_literal_pool (context_p, &scanner_context);
      }
    }
    PARSER_CATCH
    {
      JERRY_ASSERT (context_p->error == PARSER_ERR_NO_ERROR);

      while (scanner_context.active_literal_pool_p != NULL)
      {
        scanner_literal_pool_t *literal_pool_p = scanner_context.active_literal_pool_p;

        scanner_context.active_literal_pool_p = literal_pool_p->prev_p;

        parser_list_free (&literal_pool_p->literal_pool);
        scanner_free (literal_pool_p, sizeof (scanner_literal_pool_t));
      }
    }
    PARSER_TRY_END

#if ENABLED (JERRY_ES2015)
    context_p->status_flags &= (uint32_t) ~PARSER_IS_GENERATOR_FUNCTION;
#endif /* ENABLED (JERRY_ES2015) */
  }
  PARSER_TRY_END

  context_p->status_flags = scanner_context.context_status_flags;
  scanner_reverse_info_list (context_p);

#if ENABLED (JERRY_PARSER_DUMP_BYTE_CODE)
  if (context_p->is_show_opcodes)
  {
    scanner_info_t *info_p = context_p->next_scanner_info_p;
    const uint8_t *source_start_p = (arg_list_p == NULL) ? source_p : arg_list_p;

    while (info_p->type != SCANNER_TYPE_END)
    {
      const char *name_p = NULL;
      bool print_location = false;

      switch (info_p->type)
      {
        case SCANNER_TYPE_END_ARGUMENTS:
        {
          JERRY_DEBUG_MSG ("  END_ARGUMENTS\n");
          source_start_p = source_p;
          break;
        }
        case SCANNER_TYPE_FUNCTION:
        case SCANNER_TYPE_BLOCK:
        {
          const uint8_t *prev_source_p = info_p->source_p - 1;
          const uint8_t *data_p;

          if (info_p->type == SCANNER_TYPE_FUNCTION)
          {
            data_p = (const uint8_t *) (info_p + 1);

            JERRY_DEBUG_MSG ("  FUNCTION: flags: 0x%x declarations: %d",
                             (int) info_p->u8_arg,
                             (int) info_p->u16_arg);
          }
          else
          {
            data_p = (const uint8_t *) (info_p + 1);

            JERRY_DEBUG_MSG ("  BLOCK:");
          }

          JERRY_DEBUG_MSG (" source:%d\n", (int) (info_p->source_p - source_start_p));

          while (data_p[0] != SCANNER_STREAM_TYPE_END)
          {
            switch (data_p[0] & SCANNER_STREAM_TYPE_MASK)
            {
              case SCANNER_STREAM_TYPE_VAR:
              {
                JERRY_DEBUG_MSG ("    VAR ");
                break;
              }
#if ENABLED (JERRY_ES2015)
              case SCANNER_STREAM_TYPE_LET:
              {
                JERRY_DEBUG_MSG ("    LET ");
                break;
              }
              case SCANNER_STREAM_TYPE_CONST:
              {
                JERRY_DEBUG_MSG ("    CONST ");
                break;
              }
              case SCANNER_STREAM_TYPE_LOCAL:
              {
                JERRY_DEBUG_MSG ("    LOCAL ");
                break;
              }
#endif /* ENABLED (JERRY_ES2015) */
#if ENABLED (JERRY_ES2015_MODULE_SYSTEM)
              case SCANNER_STREAM_TYPE_IMPORT:
              {
                JERRY_DEBUG_MSG ("    IMPORT ");
                break;
              }
#endif /* ENABLED (JERRY_ES2015_MODULE_SYSTEM) */
              case SCANNER_STREAM_TYPE_ARG:
              {
                JERRY_DEBUG_MSG ("    ARG ");
                break;
              }
#if ENABLED (JERRY_ES2015)
              case SCANNER_STREAM_TYPE_DESTRUCTURED_ARG:
              {
                JERRY_DEBUG_MSG ("    DESTRUCTURED_ARG ");
                break;
              }
#endif /* ENABLED (JERRY_ES2015) */
              case SCANNER_STREAM_TYPE_ARG_FUNC:
              {
                JERRY_DEBUG_MSG ("    ARG_FUNC ");
                break;
              }
#if ENABLED (JERRY_ES2015)
              case SCANNER_STREAM_TYPE_DESTRUCTURED_ARG_FUNC:
              {
                JERRY_DEBUG_MSG ("    DESTRUCTURED_ARG_FUNC ");
                break;
              }
#endif /* ENABLED (JERRY_ES2015) */
              case SCANNER_STREAM_TYPE_FUNC:
              {
                JERRY_DEBUG_MSG ("    FUNC ");
                break;
              }
              default:
              {
                JERRY_ASSERT ((data_p[0] & SCANNER_STREAM_TYPE_MASK) == SCANNER_STREAM_TYPE_HOLE);
                JERRY_DEBUG_MSG ("    HOLE\n");
                data_p++;
                continue;
              }
            }

            size_t length;

            if (!(data_p[0] & SCANNER_STREAM_UINT16_DIFF))
            {
              if (data_p[2] != 0)
              {
                prev_source_p += data_p[2];
                length = 2 + 1;
              }
              else
              {
                memcpy (&prev_source_p, data_p + 2 + 1, sizeof (const uint8_t *));
                length = 2 + 1 + sizeof (const uint8_t *);
              }
            }
            else
            {
              int32_t diff = ((int32_t) data_p[2]) | ((int32_t) data_p[3]) << 8;

              if (diff <= UINT8_MAX)
              {
                diff = -diff;
              }

              prev_source_p += diff;
              length = 2 + 2;
            }

#if ENABLED (JERRY_ES2015)
            if (data_p[0] & SCANNER_STREAM_EARLY_CREATE)
            {
              JERRY_ASSERT (data_p[0] & SCANNER_STREAM_NO_REG);
              JERRY_DEBUG_MSG ("*");
            }
#endif /* ENABLED (JERRY_ES2015) */

            if (data_p[0] & SCANNER_STREAM_NO_REG)
            {
              JERRY_DEBUG_MSG ("* ");
            }

            JERRY_DEBUG_MSG ("'%.*s'\n", data_p[1], (char *) prev_source_p);
            prev_source_p += data_p[1];
            data_p += length;
          }
          break;
        }
        case SCANNER_TYPE_WHILE:
        {
          name_p = "WHILE";
          print_location = true;
          break;
        }
        case SCANNER_TYPE_FOR:
        {
          scanner_for_info_t *for_info_p = (scanner_for_info_t *) info_p;
          JERRY_DEBUG_MSG ("  FOR: source:%d expression:%d[%d:%d] end:%d[%d:%d]\n",
                           (int) (for_info_p->info.source_p - source_start_p),
                           (int) (for_info_p->expression_location.source_p - source_start_p),
                           (int) for_info_p->expression_location.line,
                           (int) for_info_p->expression_location.column,
                           (int) (for_info_p->end_location.source_p - source_start_p),
                           (int) for_info_p->end_location.line,
                           (int) for_info_p->end_location.column);
          break;
        }
        case SCANNER_TYPE_FOR_IN:
        {
          name_p = "FOR-IN";
          print_location = true;
          break;
        }
#if ENABLED (JERRY_ES2015)
        case SCANNER_TYPE_FOR_OF:
        {
          name_p = "FOR-OF";
          print_location = true;
          break;
        }
#endif /* ENABLED (JERRY_ES2015) */
        case SCANNER_TYPE_SWITCH:
        {
          JERRY_DEBUG_MSG ("  SWITCH: source:%d\n",
                           (int) (info_p->source_p - source_start_p));

          scanner_case_info_t *current_case_p = ((scanner_switch_info_t *) info_p)->case_p;

          while (current_case_p != NULL)
          {
            JERRY_DEBUG_MSG ("    CASE: location:%d[%d:%d]\n",
                             (int) (current_case_p->location.source_p - source_start_p),
                             (int) current_case_p->location.line,
                             (int) current_case_p->location.column);

            current_case_p = current_case_p->next_p;
          }
          break;
        }
        case SCANNER_TYPE_CASE:
        {
          name_p = "CASE";
          print_location = true;
          break;
        }
#if ENABLED (JERRY_ES2015)
        case SCANNER_TYPE_INITIALIZER:
        {
          name_p = "INITIALIZER";
          print_location = true;
          break;
        }
        case SCANNER_TYPE_CLASS_CONSTRUCTOR:
        {
          JERRY_DEBUG_MSG ("  CLASS-CONSTRUCTOR: source:%d\n",
                           (int) (info_p->source_p - source_start_p));
          print_location = false;
          break;
        }
        case SCANNER_TYPE_LET_EXPRESSION:
        {
          JERRY_DEBUG_MSG ("  LET_EXPRESSION: source:%d\n",
                           (int) (info_p->source_p - source_start_p));
          break;
        }
        case SCANNER_TYPE_ERR_REDECLARED:
        {
          JERRY_DEBUG_MSG ("  ERR_REDECLARED: source:%d\n",
                           (int) (info_p->source_p - source_start_p));
          break;
        }
        case SCANNER_TYPE_ERR_ASYNC_FUNCTION:
        {
          JERRY_DEBUG_MSG ("  ERR_ASYNC_FUNCTION: source:%d\n",
                           (int) (info_p->source_p - source_start_p));
          break;
        }
#endif /* ENABLED (JERRY_ES2015) */
      }

      if (print_location)
      {
        scanner_location_info_t *location_info_p = (scanner_location_info_t *) info_p;
        JERRY_DEBUG_MSG ("  %s: source:%d location:%d[%d:%d]\n",
                         name_p,
                         (int) (location_info_p->info.source_p - source_start_p),
                         (int) (location_info_p->location.source_p - source_start_p),
                         (int) location_info_p->location.line,
                         (int) location_info_p->location.column);
      }

      info_p = info_p->next_p;
    }

    JERRY_DEBUG_MSG ("\n--- Scanning end ---\n\n");
  }
#endif /* ENABLED (JERRY_PARSER_DUMP_BYTE_CODE) */

  parser_stack_free (context_p);
} /* scanner_scan_all */