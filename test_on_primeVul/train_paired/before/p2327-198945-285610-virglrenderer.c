int main()
{
   initialize_environment();

   test_format_wrong_size();
   test_blit_info_format_check();
   test_blit_info_format_check_null_format();
   test_format_is_plain_nullptr_deref_trigger();
   test_format_util_format_is_rgb_nullptr_deref_trigger_illegal_resource();
   test_format_util_format_is_rgb_nullptr_deref_trigger();
   test_double_free_in_vrend_renderer_blit_int_trigger_invalid_formats();
   test_double_free_in_vrend_renderer_blit_int_trigger();
   test_format_is_has_alpha_nullptr_deref_trigger_original();
   test_format_is_has_alpha_nullptr_deref_trigger_legal_resource();

   test_heap_overflow_vrend_renderer_transfer_write_iov();

   virgl_renderer_context_destroy(ctx_id);
   virgl_renderer_cleanup(&cookie);
   virgl_egl_destroy(test_egl);

   return 0;
}