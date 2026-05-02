R_API bool r_crbtree_insert(RRBTree *tree, void *data, RRBComparator cmp, void *user) {
	r_return_val_if_fail (tree && data && cmp, false);
	bool inserted = false;

	if (tree->root == NULL) {
		tree->root = _node_new (data, NULL);
		if (tree->root == NULL) {
			return false;
		}
		inserted = true;
		goto out_exit;
	}

	RRBNode head; /* Fake tree root */
	memset (&head, 0, sizeof (RRBNode));
	RRBNode *g = NULL, *parent = &head; /* Grandparent & parent */
	RRBNode *p = NULL, *q = tree->root; /* Iterator & parent */
	int dir = 0, last = 0; /* Directions */

	_set_link (parent, q, 1);

	for (;;) {
		if (!q) {
			/* Insert a node at first null link(also set its parent link) */
			q = _node_new (data, p);
			if (!q) {
				return false;
			}
			p->link[dir] = q;
			inserted = true;
		} else if (IS_RED (q->link[0]) && IS_RED (q->link[1])) {
			/* Simple red violation: color flip */
			q->red = 1;
			q->link[0]->red = 0;
			q->link[1]->red = 0;
		}

		if (IS_RED (q) && IS_RED (p)) {
#if 0
			// coverity error, parent is never null
			/* Hard red violation: rotate */
			if (!parent) {
				return false;
			}
#endif
			int dir2 = parent->link[1] == g;
			if (q == p->link[last]) {
				_set_link (parent, _rot_once (g, !last), dir2);
			} else {
				_set_link (parent, _rot_twice (g, !last), dir2);
			}
		}

		if (inserted) {
			break;
		}

		last = dir;
		dir = cmp (data, q->data, user) >= 0;

		if (g) {
			parent = g;
		}

		g = p;
		p = q;
		q = q->link[dir];
	}

	/* Update root(it may different due to root rotation) */
	tree->root = head.link[1];

out_exit:
	/* Invariant: root is black */
	tree->root->red = 0;
	tree->root->parent = NULL;
	if (inserted) {
		tree->size++;
	}

	return inserted;
}