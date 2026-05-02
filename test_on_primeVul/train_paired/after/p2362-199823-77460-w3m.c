formList_addInput(struct form_list *fl, struct parsed_tag *tag)
{
    struct form_item_list *item;
    char *p;
    int i;

    /* if not in <form>..</form> environment, just ignore <input> tag */
    if (fl == NULL)
	return NULL;

    item = New(struct form_item_list);
    item->type = FORM_UNKNOWN;
    item->size = -1;
    item->rows = 0;
    item->checked = item->init_checked = 0;
    item->accept = 0;
    item->name = NULL;
    item->value = item->init_value = NULL;
    item->readonly = 0;
    if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
	item->type = formtype(p);
	if (item->size < 0 &&
	    (item->type == FORM_INPUT_TEXT ||
	     item->type == FORM_INPUT_FILE ||
	     item->type == FORM_INPUT_PASSWORD))
	    item->size = FORM_I_TEXT_DEFAULT_SIZE;
    }
    if (parsedtag_get_value(tag, ATTR_NAME, &p))
	item->name = Strnew_charp(p);
    if (parsedtag_get_value(tag, ATTR_VALUE, &p))
	item->value = item->init_value = Strnew_charp(p);
    item->checked = item->init_checked = parsedtag_exists(tag, ATTR_CHECKED);
    item->accept = parsedtag_exists(tag, ATTR_ACCEPT);
    parsedtag_get_value(tag, ATTR_SIZE, &item->size);
    parsedtag_get_value(tag, ATTR_MAXLENGTH, &item->maxlength);
    item->readonly = parsedtag_exists(tag, ATTR_READONLY);
    if (parsedtag_get_value(tag, ATTR_TEXTAREANUMBER, &i)
	&& i >= 0 && i < max_textarea)
	item->value = item->init_value = textarea_str[i];
#ifdef MENU_SELECT
    if (parsedtag_get_value(tag, ATTR_SELECTNUMBER, &i)
	&& i >= 0 && i < max_select)
	item->select_option = select_option[i].first;
#endif				/* MENU_SELECT */
    if (parsedtag_get_value(tag, ATTR_ROWS, &p))
	item->rows = atoi(p);
    if (item->type == FORM_UNKNOWN) {
	/* type attribute is missing. Ignore the tag. */
	return NULL;
    }
#ifdef MENU_SELECT
    if (item->type == FORM_SELECT) {
	chooseSelectOption(item, item->select_option);
	item->init_selected = item->selected;
	item->init_value = item->value;
	item->init_label = item->label;
    }
#endif				/* MENU_SELECT */
    if (item->type == FORM_INPUT_FILE && item->value && item->value->length) {
	/* security hole ! */
	return NULL;
    }
    item->parent = fl;
    item->next = NULL;
    if (fl->item == NULL) {
	fl->item = fl->lastitem = item;
    }
    else {
	fl->lastitem->next = item;
	fl->lastitem = item;
    }
    if (item->type == FORM_INPUT_HIDDEN)
	return NULL;
    fl->nitems++;
    return item;
}
