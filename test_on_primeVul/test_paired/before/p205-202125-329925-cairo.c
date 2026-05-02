_inplace_src_spans (void *abstract_renderer, int y, int h,
		    const cairo_half_open_span_t *spans,
		    unsigned num_spans)
{
    cairo_image_span_renderer_t *r = abstract_renderer;
    uint8_t *m;
    int x0;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    x0 = spans[0].x;
    m = r->_buf;
    do {
	int len = spans[1].x - spans[0].x;
	if (len >= r->u.composite.run_length && spans[0].coverage == 0xff) {
	    if (spans[0].x != x0) {
#if PIXMAN_HAS_OP_LERP
		pixman_image_composite32 (PIXMAN_OP_LERP_SRC,
					  r->src, r->mask, r->u.composite.dst,
					  x0 + r->u.composite.src_x,
					  y + r->u.composite.src_y,
					  0, 0,
					  x0, y,
					  spans[0].x - x0, h);
#else
		pixman_image_composite32 (PIXMAN_OP_OUT_REVERSE,
					  r->mask, NULL, r->u.composite.dst,
					  0, 0,
					  0, 0,
					  x0, y,
					  spans[0].x - x0, h);
		pixman_image_composite32 (PIXMAN_OP_ADD,
					  r->src, r->mask, r->u.composite.dst,
					  x0 + r->u.composite.src_x,
					  y + r->u.composite.src_y,
					  0, 0,
					  x0, y,
					  spans[0].x - x0, h);
#endif
	    }

	    pixman_image_composite32 (PIXMAN_OP_SRC,
				      r->src, NULL, r->u.composite.dst,
				      spans[0].x + r->u.composite.src_x,
				      y + r->u.composite.src_y,
				      0, 0,
				      spans[0].x, y,
				      spans[1].x - spans[0].x, h);

	    m = r->_buf;
	    x0 = spans[1].x;
	} else if (spans[0].coverage == 0x0) {
	    if (spans[0].x != x0) {
#if PIXMAN_HAS_OP_LERP
		pixman_image_composite32 (PIXMAN_OP_LERP_SRC,
					  r->src, r->mask, r->u.composite.dst,
					  x0 + r->u.composite.src_x,
					  y + r->u.composite.src_y,
					  0, 0,
					  x0, y,
					  spans[0].x - x0, h);
#else
		pixman_image_composite32 (PIXMAN_OP_OUT_REVERSE,
					  r->mask, NULL, r->u.composite.dst,
					  0, 0,
					  0, 0,
					  x0, y,
					  spans[0].x - x0, h);
		pixman_image_composite32 (PIXMAN_OP_ADD,
					  r->src, r->mask, r->u.composite.dst,
					  x0 + r->u.composite.src_x,
					  y + r->u.composite.src_y,
					  0, 0,
					  x0, y,
					  spans[0].x - x0, h);
#endif
	    }

	    m = r->_buf;
	    x0 = spans[1].x;
	} else {
	    *m++ = spans[0].coverage;
	    if (len > 1) {
		memset (m, spans[0].coverage, --len);
		m += len;
	    }
	}
	spans++;
    } while (--num_spans > 1);

    if (spans[0].x != x0) {
#if PIXMAN_HAS_OP_LERP
	pixman_image_composite32 (PIXMAN_OP_LERP_SRC,
				  r->src, r->mask, r->u.composite.dst,
				  x0 + r->u.composite.src_x,
				  y + r->u.composite.src_y,
				  0, 0,
				  x0, y,
				  spans[0].x - x0, h);
#else
	pixman_image_composite32 (PIXMAN_OP_OUT_REVERSE,
				  r->mask, NULL, r->u.composite.dst,
				  0, 0,
				  0, 0,
				  x0, y,
				  spans[0].x - x0, h);
	pixman_image_composite32 (PIXMAN_OP_ADD,
				  r->src, r->mask, r->u.composite.dst,
				  x0 + r->u.composite.src_x,
				  y + r->u.composite.src_y,
				  0, 0,
				  x0, y,
				  spans[0].x - x0, h);
#endif
    }

    return CAIRO_STATUS_SUCCESS;
}