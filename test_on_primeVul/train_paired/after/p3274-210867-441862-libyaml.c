yaml_parser_save_simple_key(yaml_parser_t *parser)
{
    /*
     * A simple key is required at the current position if the scanner is in
     * the block context and the current column coincides with the indentation
     * level.
     */

    int required = (!parser->flow_level
            && parser->indent == (ptrdiff_t)parser->mark.column);

    /*
     * A simple key is required only when it is the first token in the current
     * line.  Therefore it is always allowed.  But we add a check anyway.
     */

    /* XXX This caused:
     * https://bitbucket.org/xi/libyaml/issue/10/wrapped-strings-cause-assert-failure
    assert(parser->simple_key_allowed || !required); */    /* Impossible. */

    /*
     * If the current position may start a simple key, save it.
     */

    if (parser->simple_key_allowed)
    {
        yaml_simple_key_t simple_key;
        simple_key.possible = 1;
        simple_key.required = required;
        simple_key.token_number = 
            parser->tokens_parsed + (parser->tokens.tail - parser->tokens.head);
        simple_key.mark = parser->mark;

        if (!yaml_parser_remove_simple_key(parser)) return 0;

        *(parser->simple_keys.top-1) = simple_key;
    }

    return 1;
}