end_element(PInfo pi, const char *ename) {
    if (TRACE <= pi->options->trace) {
	char	indent[128];
	
	if (DEBUG <= pi->options->trace) {
	    char    buf[1024];

	    printf("===== end element stack(%d) =====\n", helper_stack_depth(&pi->helpers));
	    snprintf(buf, sizeof(buf) - 1, "</%s>", ename);
	    debug_stack(pi, buf);
	} else {
	    fill_indent(pi, indent, sizeof(indent));
	    printf("%s</%s>\n", indent, ename);
	}
    }
    if (!helper_stack_empty(&pi->helpers)) {
	Helper	h = helper_stack_pop(&pi->helpers);
	Helper	ph = helper_stack_peek(&pi->helpers);

	if (ox_empty_string == h->obj) {
	    /* special catch for empty strings */
	    h->obj = rb_str_new2("");
	}
	pi->obj = h->obj;
	if (0 != ph) {
	    switch (ph->type) {
	    case ArrayCode:
		rb_ary_push(ph->obj, h->obj);
		break;
	    case ExceptionCode:
	    case ObjectCode:
		if (Qnil != ph->obj) {
		    rb_ivar_set(ph->obj, h->var, h->obj);
		}
		break;
	    case StructCode:
#if HAS_RSTRUCT
		rb_struct_aset(ph->obj, h->var, h->obj);
#else
		set_error(&pi->err, "Ruby structs not supported with this verion of Ruby", pi->str, pi->s);
		return;
#endif
		break;
	    case HashCode:
		// put back h
		helper_stack_push(&pi->helpers, h->var, h->obj, KeyCode);
		break;
	    case RangeCode:
#if HAS_RSTRUCT
		if (ox_beg_id == h->var) {
		    RSTRUCT_SET(ph->obj, 0, h->obj);
		} else if (ox_end_id == h->var) {
		    RSTRUCT_SET(ph->obj, 1, h->obj);
		} else if (ox_excl_id == h->var) {
		    RSTRUCT_SET(ph->obj, 2, h->obj);
		} else {
		    set_error(&pi->err, "Invalid range attribute", pi->str, pi->s);
		    return;
		}
#else
		set_error(&pi->err, "Ruby structs not supported with this verion of Ruby", pi->str, pi->s);
		return;
#endif
		break;
	    case KeyCode:
		{
		    Helper	gh;

		    helper_stack_pop(&pi->helpers);
		    if (NULL == (gh = helper_stack_peek(&pi->helpers))) {
			set_error(&pi->err, "Corrupt parse stack, container is wrong type", pi->str, pi->s);
			return;
		    }
		    rb_hash_aset(gh->obj, ph->obj, h->obj);
		}
		break;
	    case ComplexCode:
#ifdef T_COMPLEX
		if (Qundef == ph->obj) {
		    ph->obj = h->obj;
		} else {
		    ph->obj = rb_complex_new(ph->obj, h->obj);
		}
#else
		set_error(&pi->err, "Complex Objects not implemented in Ruby 1.8.7", pi->str, pi->s);
		return;
#endif
		break;
	    case RationalCode:
#ifdef T_RATIONAL
		if (Qundef == ph->obj) {
		    ph->obj = h->obj;
		} else {
#ifdef RUBINIUS_RUBY
		    ph->obj = rb_Rational(ph->obj, h->obj);
#else
		    ph->obj = rb_rational_new(ph->obj, h->obj);
#endif
		}
#else
		set_error(&pi->err, "Rational Objects not implemented in Ruby 1.8.7", pi->str, pi->s);
		return;
#endif
		break;
	    default:
		set_error(&pi->err, "Corrupt parse stack, container is wrong type", pi->str, pi->s);
		return;
		break;
	    }
	}
    }
    if (0 != pi->circ_array && helper_stack_empty(&pi->helpers)) {
	circ_array_free(pi->circ_array);
	pi->circ_array = 0;
    }
    if (DEBUG <= pi->options->trace) {
	debug_stack(pi, "   ----------");
    }
}