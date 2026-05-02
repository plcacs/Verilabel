static tmbstr ParseValue( TidyDocImpl* doc, ctmbstr name,
                          Bool foldCase, Bool *isempty, int *pdelim)
{
    Lexer* lexer = doc->lexer;
    int len = 0, start;
    Bool seen_gt = no;
    Bool munge = yes;
    uint c, lastc, delim, quotewarning;
    tmbstr value;

    delim = (tmbchar) 0;
    *pdelim = '"';

    /*
     Henry Zrepa reports that some folk are using the
     embed element with script attributes where newlines
     are significant and must be preserved
    */
    if ( cfgBool(doc, TidyLiteralAttribs) )
        munge = no;

 /* skip white space before the '=' */

    for (;;)
    {
        c = TY_(ReadChar)(doc->docIn);

        if (c == EndOfStream)
        {
            TY_(UngetChar)(c, doc->docIn);
            break;
        }

        if (!TY_(IsWhite)(c))
           break;
    }

/*
  c should be '=' if there is a value
  other legal possibilities are white
  space, '/' and '>'
*/

    if (c != '=' && c != '"' && c != '\'')
    {
        TY_(UngetChar)(c, doc->docIn);
        return NULL;
    }

 /* skip white space after '=' */

    for (;;)
    {
        c = TY_(ReadChar)(doc->docIn);

        if (c == EndOfStream)
        {
            TY_(UngetChar)(c, doc->docIn);
            break;
        }

        if (!TY_(IsWhite)(c))
           break;
    }

 /* check for quote marks */

    if (c == '"' || c == '\'')
        delim = c;
    else if (c == '<')
    {
        start = lexer->lexsize;
        TY_(AddCharToLexer)(lexer, c);
        *pdelim = ParseServerInstruction( doc );
        len = lexer->lexsize - start;
        lexer->lexsize = start;
        return (len > 0 ? TY_(tmbstrndup)(doc->allocator,
                                          lexer->lexbuf+start, len) : NULL);
    }
    else
        TY_(UngetChar)(c, doc->docIn);

 /*
   and read the value string
   check for quote mark if needed
 */

    quotewarning = 0;
    start = lexer->lexsize;
    c = '\0';

    for (;;)
    {
        lastc = c;  /* track last character */
        c = TY_(ReadChar)(doc->docIn);

        if (c == EndOfStream)
        {
            TY_(ReportAttrError)( doc, lexer->token, NULL, UNEXPECTED_END_OF_FILE_ATTR );
            TY_(UngetChar)(c, doc->docIn);
            break;
        }

        if (delim == (tmbchar)0)
        {
            if (c == '>')
            {
                TY_(UngetChar)(c, doc->docIn);
                break;
            }

            if (c == '"' || c == '\'')
            {
                uint q = c;

                TY_(ReportAttrError)( doc, lexer->token, NULL, UNEXPECTED_QUOTEMARK );

                /* handle <input onclick=s("btn1")> and <a title=foo""">...</a> */
                /* this doesn't handle <a title=foo"/> which browsers treat as  */
                /* 'foo"/' nor  <a title=foo" /> which browser treat as 'foo"'  */
                
                c = TY_(ReadChar)(doc->docIn);
                if (c == '>')
                {
                    TY_(AddCharToLexer)(lexer, q);
                    TY_(UngetChar)(c, doc->docIn);
                    break;
                }
                else
                {
                    TY_(UngetChar)(c, doc->docIn);
                    c = q;
                }
            }

            if (c == '<')
            {
                TY_(UngetChar)(c, doc->docIn);
                c = '>';
                TY_(UngetChar)(c, doc->docIn);
                TY_(ReportAttrError)( doc, lexer->token, NULL, UNEXPECTED_GT );
                break;
            }

            /*
             For cases like <br clear=all/> need to avoid treating /> as
             part of the attribute value, however care is needed to avoid
             so treating <a href=http://www.acme.com/> in this way, which
             would map the <a> tag to <a href="http://www.acme.com"/>
            */
            if (c == '/')
            {
                /* peek ahead in case of /> */
                c = TY_(ReadChar)(doc->docIn);

                if ( c == '>' && !TY_(IsUrl)(doc, name) )
                {
                    *isempty = yes;
                    TY_(UngetChar)(c, doc->docIn);
                    break;
                }

                /* unget peeked character */
                TY_(UngetChar)(c, doc->docIn);
                c = '/';
            }
        }
        else  /* delim is '\'' or '"' */
        {
            if (c == delim)
                break;

            if (c == '\n' || c == '<' || c == '>')
                ++quotewarning;

            if (c == '>')
                seen_gt = yes;
        }

        if (c == '&')
        {
            TY_(AddCharToLexer)(lexer, c);
            ParseEntity( doc, IgnoreWhitespace );
            if (lexer->lexbuf[lexer->lexsize - 1] == '\n' && munge)
                ChangeChar(lexer, ' ');
            continue;
        }

        /*
         kludge for JavaScript attribute values
         with line continuations in string literals
        */
        if (c == '\\')
        {
            c = TY_(ReadChar)(doc->docIn);

            if (c != '\n')
            {
                TY_(UngetChar)(c, doc->docIn);
                c = '\\';
            }
        }

        if (TY_(IsWhite)(c))
        {
            if ( delim == 0 )
                break;

            if (munge)
            {
                /* discard line breaks in quoted URLs */ 
                /* #438650 - fix by Randy Waki */
                if ( c == '\n' && TY_(IsUrl)(doc, name) )
                {
                    /* warn that we discard this newline */
                    TY_(ReportAttrError)( doc, lexer->token, NULL, NEWLINE_IN_URI);
                    continue;
                }
                
                c = ' ';

                if (lastc == ' ')
                {
                    if (TY_(IsUrl)(doc, name) )
                        TY_(ReportAttrError)( doc, lexer->token, NULL, WHITE_IN_URI);
                    continue;
                }
            }
        }
        else if (foldCase && TY_(IsUpper)(c))
            c = TY_(ToLower)(c);

        TY_(AddCharToLexer)(lexer, c);
    }

    if (quotewarning > 10 && seen_gt && munge)
    {
        /*
           there is almost certainly a missing trailing quote mark
           as we have see too many newlines, < or > characters.

           an exception is made for Javascript attributes and the
           javascript URL scheme which may legitimately include < and >,
           and for attributes starting with "<xml " as generated by
           Microsoft Office.
        */
        if ( !TY_(IsScript)(doc, name) &&
             !(TY_(IsUrl)(doc, name) && TY_(tmbstrncmp)(lexer->lexbuf+start, "javascript:", 11) == 0) &&
             !(TY_(tmbstrncmp)(lexer->lexbuf+start, "<xml ", 5) == 0)
           )
            TY_(ReportFatal)( doc, NULL, NULL, SUSPECTED_MISSING_QUOTE ); 
    }

    len = lexer->lexsize - start;
    lexer->lexsize = start;


    if (len > 0 || delim)
    {
        /* ignore leading and trailing white space for all but title, alt, value */
        /* and prompts attributes unless --literal-attributes is set to yes      */
        /* #994841 - Whitespace is removed from value attributes                 */

        if (munge &&
            TY_(tmbstrcasecmp)(name, "alt") &&
            TY_(tmbstrcasecmp)(name, "title") &&
            TY_(tmbstrcasecmp)(name, "value") &&
            TY_(tmbstrcasecmp)(name, "prompt"))
        {
            while (TY_(IsWhite)(lexer->lexbuf[start+len-1]))
                --len;

            while (TY_(IsWhite)(lexer->lexbuf[start]) && start < len)
            {
                ++start;
                --len;
            }
        }

        value = TY_(tmbstrndup)(doc->allocator, lexer->lexbuf + start, len);
    }
    else
        value = NULL;

    /* note delimiter if given */
    *pdelim = (delim ? delim : '"');

    return value;
}