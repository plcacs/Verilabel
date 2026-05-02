bool Item_equal::create_pushable_equalities(THD *thd,
                                            List<Item> *equalities,
                                            Pushdown_checker checker,
                                            uchar *arg,
                                            bool clone_const)
{
  Item *item;
  Item *left_item= NULL;
  Item *right_item = get_const();
  Item_equal_fields_iterator it(*this);

  while ((item=it++))
  {
    left_item= item;
    if (checker && !((item->*checker) (arg)))
      continue;
    break;
  }

  if (!left_item)
    return false;

  if (right_item)
  {
    Item_func_eq *eq= 0;
    Item *left_item_clone= left_item->build_clone(thd);
    Item *right_item_clone= !clone_const ?
                            right_item : right_item->build_clone(thd);
    if (!left_item_clone || !right_item_clone)
      return true;
    eq= new (thd->mem_root) Item_func_eq(thd,
                                         left_item_clone,
                                         right_item_clone);
    if (!eq ||  equalities->push_back(eq, thd->mem_root))
      return true;
    if (!clone_const)
      right_item->set_extraction_flag(IMMUTABLE_FL);
  }

  while ((item=it++))
  {
    if (checker && !((item->*checker) (arg)))
      continue;
    Item_func_eq *eq= 0;
    Item *left_item_clone= left_item->build_clone(thd);
    Item *right_item_clone= item->build_clone(thd);
    if (!(left_item_clone && right_item_clone))
      return true;
    left_item_clone->set_item_equal(NULL);
    right_item_clone->set_item_equal(NULL);
    eq= new (thd->mem_root) Item_func_eq(thd,
                                         right_item_clone,
                                         left_item_clone);
    if (!eq || equalities->push_back(eq, thd->mem_root))
      return true;
  }
  return false;
}