expand_case_fold_string(Node* node, regex_t* reg)
{
#define THRESHOLD_CASE_FOLD_ALT_FOR_EXPANSION  8

  int r, n, len, alt_num;
  UChar *start, *end, *p;
  Node *top_root, *root, *snode, *prev_node;
  OnigCaseFoldCodeItem items[ONIGENC_GET_CASE_FOLD_CODES_MAX_NUM];
  StrNode* sn = STR_(node);

  if (NODE_STRING_IS_AMBIG(node)) return 0;

  start = sn->s;
  end   = sn->end;
  if (start >= end) return 0;

  r = 0;
  top_root = root = prev_node = snode = NULL_NODE;
  alt_num = 1;
  p = start;
  while (p < end) {
    n = ONIGENC_GET_CASE_FOLD_CODES_BY_STR(reg->enc, reg->case_fold_flag, p, end,
                                           items);
    if (n < 0) {
      r = n;
      goto err;
    }

    len = enclen(reg->enc, p);

    if (n == 0) {
      if (IS_NULL(snode)) {
        if (IS_NULL(root) && IS_NOT_NULL(prev_node)) {
          top_root = root = onig_node_list_add(NULL_NODE, prev_node);
          if (IS_NULL(root)) {
            onig_node_free(prev_node);
            goto mem_err;
          }
        }

        prev_node = snode = onig_node_new_str(NULL, NULL);
        if (IS_NULL(snode)) goto mem_err;
        if (IS_NOT_NULL(root)) {
          if (IS_NULL(onig_node_list_add(root, snode))) {
            onig_node_free(snode);
            goto mem_err;
          }
        }
      }

      r = onig_node_str_cat(snode, p, p + len);
      if (r != 0) goto err;
    }
    else {
      alt_num *= (n + 1);
      if (alt_num > THRESHOLD_CASE_FOLD_ALT_FOR_EXPANSION) break;

      if (IS_NULL(root) && IS_NOT_NULL(prev_node)) {
        top_root = root = onig_node_list_add(NULL_NODE, prev_node);
        if (IS_NULL(root)) {
          onig_node_free(prev_node);
          goto mem_err;
        }
      }

      r = expand_case_fold_string_alt(n, items, p, len, end, reg, &prev_node);
      if (r < 0) goto mem_err;
      if (r == 1) {
        if (IS_NULL(root)) {
          top_root = prev_node;
        }
        else {
          if (IS_NULL(onig_node_list_add(root, prev_node))) {
            onig_node_free(prev_node);
            goto mem_err;
          }
        }

        root = NODE_CAR(prev_node);
      }
      else { /* r == 0 */
        if (IS_NOT_NULL(root)) {
          if (IS_NULL(onig_node_list_add(root, prev_node))) {
            onig_node_free(prev_node);
            goto mem_err;
          }
        }
      }

      snode = NULL_NODE;
    }

    p += len;
  }

  if (p < end) {
    Node *srem;

    r = expand_case_fold_make_rem_string(&srem, p, end, reg);
    if (r != 0) goto mem_err;

    if (IS_NOT_NULL(prev_node) && IS_NULL(root)) {
      top_root = root = onig_node_list_add(NULL_NODE, prev_node);
      if (IS_NULL(root)) {
        onig_node_free(srem);
        onig_node_free(prev_node);
        goto mem_err;
      }
    }

    if (IS_NULL(root)) {
      prev_node = srem;
    }
    else {
      if (IS_NULL(onig_node_list_add(root, srem))) {
        onig_node_free(srem);
        goto mem_err;
      }
    }
  }

  /* ending */
  top_root = (IS_NOT_NULL(top_root) ? top_root : prev_node);
  swap_node(node, top_root);
  onig_node_free(top_root);
  return 0;

 mem_err:
  r = ONIGERR_MEMORY;

 err:
  onig_node_free(top_root);
  return r;
}