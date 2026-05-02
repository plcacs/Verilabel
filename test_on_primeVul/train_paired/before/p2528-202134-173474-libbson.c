bson_iter_codewscope (const bson_iter_t *iter, /* IN */
                      uint32_t *length,        /* OUT */
                      uint32_t *scope_len,     /* OUT */
                      const uint8_t **scope)   /* OUT */
{
   uint32_t len;

   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_CODEWSCOPE) {
      if (length) {
         memcpy (&len, iter->raw + iter->d2, sizeof (len));
         *length = BSON_UINT32_FROM_LE (len) - 1;
      }

      memcpy (&len, iter->raw + iter->d4, sizeof (len));
      *scope_len = BSON_UINT32_FROM_LE (len);
      *scope = iter->raw + iter->d4;
      return (const char *) (iter->raw + iter->d3);
   }

   if (length) {
      *length = 0;
   }

   if (scope_len) {
      *scope_len = 0;
   }

   if (scope) {
      *scope = NULL;
   }

   return NULL;
}