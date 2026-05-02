  void Inspect::operator()(Complex_Selector_Ptr c)
  {
    Compound_Selector_Obj      head = c->head();
    Complex_Selector_Obj            tail = c->tail();
    Complex_Selector::Combinator comb = c->combinator();

    if (comb == Complex_Selector::ANCESTOR_OF && (!head || head->empty())) {
      if (tail) tail->perform(this);
      return;
    }

    if (c->has_line_feed()) {
      if (!(c->has_parent_ref())) {
        append_optional_linefeed();
        append_indentation();
      }
    }

    if (head && head->length() != 0) head->perform(this);
    bool is_empty = !head || head->length() == 0 || head->is_empty_reference();
    bool is_tail = head && !head->is_empty_reference() && tail;
    if (output_style() == COMPRESSED && comb != Complex_Selector::ANCESTOR_OF) scheduled_space = 0;

    switch (comb) {
      case Complex_Selector::ANCESTOR_OF:
        if (is_tail) append_mandatory_space();
      break;
      case Complex_Selector::PARENT_OF:
        append_optional_space();
        append_string(">");
        append_optional_space();
      break;
      case Complex_Selector::ADJACENT_TO:
        append_optional_space();
        append_string("+");
        append_optional_space();
      break;
      case Complex_Selector::REFERENCE:
        append_mandatory_space();
        append_string("/");
        c->reference()->perform(this);
        append_string("/");
        append_mandatory_space();
      break;
      case Complex_Selector::PRECEDES:
        if (is_empty) append_optional_space();
        else append_mandatory_space();
        append_string("~");
        if (tail) append_mandatory_space();
        else append_optional_space();
      break;
      default: break;
    }
    if (tail && comb != Complex_Selector::ANCESTOR_OF) {
      if (c->has_line_break()) append_optional_linefeed();
    }
    if (tail) tail->perform(this);
    if (!tail && c->has_line_break()) {
      if (output_style() == COMPACT) {
        append_mandatory_space();
      }
    }
  }