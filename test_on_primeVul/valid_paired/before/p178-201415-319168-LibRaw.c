static x3f_huffnode_t *new_node(x3f_hufftree_t *tree)
{
  x3f_huffnode_t *t = &tree->nodes[tree->free_node_index];

  t->branch[0] = NULL;
  t->branch[1] = NULL;
  t->leaf = UNDEFINED_LEAF;

  tree->free_node_index++;

  return t;
}