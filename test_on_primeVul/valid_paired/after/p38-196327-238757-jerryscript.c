ecma_op_internal_buffer_append (ecma_collection_t *container_p, /**< internal container pointer */
                                ecma_value_t key_arg, /**< key argument */
                                ecma_value_t value_arg, /**< value argument */
                                lit_magic_string_id_t lit_id) /**< class id */
{
  JERRY_ASSERT (container_p != NULL);

  if (lit_id == LIT_MAGIC_STRING_WEAKMAP_UL || lit_id == LIT_MAGIC_STRING_MAP_UL)
  {
    ecma_value_t values[] = { ecma_copy_value_if_not_object (key_arg), ecma_copy_value_if_not_object (value_arg) };
    ecma_collection_append (container_p, values, 2);
  }
  else
  {
    ecma_collection_push_back (container_p, ecma_copy_value_if_not_object (key_arg));
  }

  ECMA_CONTAINER_SET_SIZE (container_p, ECMA_CONTAINER_GET_SIZE (container_p) + 1);
} /* ecma_op_internal_buffer_append */