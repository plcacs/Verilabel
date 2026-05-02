test_bson_append_code_with_scope (void)
{
   const uint8_t *scope_buf = NULL;
   uint32_t scopelen = 0;
   uint32_t len = 0;
   bson_iter_t iter;
   bool r;
   const char *code = NULL;
   bson_t *b;
   bson_t *b2;
   bson_t *scope;

   /* Test with NULL bson, which converts to just CODE type. */
   b = bson_new ();
   BSON_ASSERT (
      bson_append_code_with_scope (b, "code", -1, "var a = {};", NULL));
   b2 = get_bson ("test30.bson");
   BSON_ASSERT_BSON_EQUAL (b, b2);
   r = bson_iter_init_find (&iter, b, "code");
   BSON_ASSERT (r);
   BSON_ASSERT (BSON_ITER_HOLDS_CODE (&iter)); /* Not codewscope */
   bson_destroy (b);
   bson_destroy (b2);

   /* Empty scope is still CODEWSCOPE. */
   b = bson_new ();
   scope = bson_new ();
   BSON_ASSERT (
      bson_append_code_with_scope (b, "code", -1, "var a = {};", scope));
   b2 = get_bson ("code_w_empty_scope.bson");
   BSON_ASSERT_BSON_EQUAL (b, b2);
   r = bson_iter_init_find (&iter, b, "code");
   BSON_ASSERT (r);
   BSON_ASSERT (BSON_ITER_HOLDS_CODEWSCOPE (&iter));
   bson_destroy (b);
   bson_destroy (b2);
   bson_destroy (scope);

   /* Test with non-empty scope */
   b = bson_new ();
   scope = bson_new ();
   BSON_ASSERT (bson_append_utf8 (scope, "foo", -1, "bar", -1));
   BSON_ASSERT (
      bson_append_code_with_scope (b, "code", -1, "var a = {};", scope));
   b2 = get_bson ("test31.bson");
   BSON_ASSERT_BSON_EQUAL (b, b2);
   r = bson_iter_init_find (&iter, b, "code");
   BSON_ASSERT (r);
   BSON_ASSERT (BSON_ITER_HOLDS_CODEWSCOPE (&iter));
   code = bson_iter_codewscope (&iter, &len, &scopelen, &scope_buf);
   BSON_ASSERT (len == 11);
   BSON_ASSERT (scopelen == scope->len);
   BSON_ASSERT (!strcmp (code, "var a = {};"));
   bson_destroy (b);
   bson_destroy (b2);
   bson_destroy (scope);

   /* CDRIVER-2269 Test with a malformed zero length code string  */
   b = bson_new ();

   uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00, // length of doc (set below)
      0x0F, // code_w_s type
      0x00, // empty key
      0x10,
      0x00,
      0x00,
      0x00, // code_w_s length (needs to be > 14 for initial
            // validation so give a non-empty scope doc)
      0x00,
      0x00,
      0x00,
      0x00, // invalid string length (must have trailing \0)
      0x08,
      0x00,
      0x00,
      0x00, // scope doc length
      0x08,
      0x00,
      0x00, // "" : false
      0x00, // end of scope doc
      0x00  // end of doc
   };
   data[0] = (uint8_t) sizeof (data);
   b = bson_new_from_data (data, sizeof (data));

   BSON_ASSERT (b);
   BSON_ASSERT (bson_iter_init (&iter, b));
   BSON_ASSERT (!bson_iter_next (&iter));
   bson_destroy (b);

   /* CDRIVER-2269 Test with malformed BSON from ticket */
   bson_error_t err;
   bool eof;
   bson_reader_t *reader =
      bson_reader_new_from_file (BINARY_DIR "/cdriver2269.bson", &err);
   BSON_ASSERT (reader);
   const bson_t *ticketBSON = bson_reader_read (reader, &eof);
   BSON_ASSERT (ticketBSON);
   BSON_ASSERT (bson_iter_init (&iter, ticketBSON));
   BSON_ASSERT (!bson_iter_next (&iter));
   bson_reader_destroy (reader);
}