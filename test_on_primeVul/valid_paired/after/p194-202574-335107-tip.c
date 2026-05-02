bool timerqueue_add(struct timerqueue_head *head, struct timerqueue_node *node)
{
	struct rb_node **p = &head->rb_root.rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct timerqueue_node *ptr;
	bool leftmost = true;

	/* Make sure we don't add nodes that are already added */
	WARN_ON_ONCE(!RB_EMPTY_NODE(&node->node));

	while (*p) {
		parent = *p;
		ptr = rb_entry(parent, struct timerqueue_node, node);
		if (node->expires < ptr->expires) {
			p = &(*p)->rb_left;
		} else {
			p = &(*p)->rb_right;
			leftmost = false;
		}
	}
	rb_link_node(&node->node, parent, p);
	rb_insert_color_cached(&node->node, &head->rb_root, leftmost);

	return leftmost;
}