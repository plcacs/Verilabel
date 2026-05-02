nm_wildcard_match_check(const char *str, const char *const *patterns, guint num_patterns)
{
    gboolean has_optional     = FALSE;
    gboolean has_any_optional = FALSE;
    guint    i;

    for (i = 0; i < num_patterns; i++) {
        gboolean    is_inverted;
        gboolean    is_mandatory;
        gboolean    match;
        const char *p;

        _pattern_parse(patterns[i], &p, &is_inverted, &is_mandatory);

        match = (fnmatch(p, str, 0) == 0);
        if (is_inverted)
            match = !match;

        if (is_mandatory) {
            if (!match)
                return FALSE;
        } else {
            has_any_optional = TRUE;
            if (match)
                has_optional = TRUE;
        }
    }

    return has_optional || !has_any_optional;
}