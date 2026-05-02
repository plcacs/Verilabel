int pure_strcmp(const char * const s1, const char * const s2)
{
    return pure_memcmp(s1, s2, strlen(s1) + 1U);
}