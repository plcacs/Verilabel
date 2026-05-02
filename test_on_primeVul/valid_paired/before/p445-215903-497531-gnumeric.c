ms_escher_get_data (MSEscherState *state,
		    gint offset,	/* bytes from logical start of the stream */
		    gint num_bytes,	/*how many bytes we want, NOT incl prefix */
		    gboolean * needs_free)
{
	BiffQuery *q = state->q;
	guint8    *res;

	g_return_val_if_fail (offset >= state->start_offset, NULL);

	/* find the 1st containing record */
	while (offset >= state->end_offset) {
		if (!ms_biff_query_next (q)) {
			g_warning ("unexpected end of stream;");
			return NULL;
		}

		if (q->opcode != BIFF_MS_O_DRAWING &&
		    q->opcode != BIFF_MS_O_DRAWING_GROUP &&
		    q->opcode != BIFF_MS_O_DRAWING_SELECTION &&
		    q->opcode != BIFF_CHART_gelframe &&
		    q->opcode != BIFF_CONTINUE) {
		  g_warning ("Unexpected record type 0x%x len=0x%x @ 0x%lx;", q->opcode, q->length, (long)q->streamPos);
			return NULL;
		}

		d (1, g_printerr ("Target is 0x%x bytes at 0x%x, current = 0x%x..0x%x;\n"
			      "Adding biff-0x%x of length 0x%x;\n",
			      num_bytes, offset,
			      state->start_offset,
			      state->end_offset,
			      q->opcode, q->length););

		state->start_offset = state->end_offset;
		state->end_offset += q->length;
		state->segment_len = q->length;
	}

	g_return_val_if_fail (offset >= state->start_offset, NULL);
	g_return_val_if_fail ((size_t)(offset - state->start_offset) < q->length, NULL);

	res = q->data + offset - state->start_offset;
	if ((*needs_free = ((offset + num_bytes) > state->end_offset))) {
		guint8 *buffer = g_malloc (num_bytes);
		guint8 *tmp = buffer;

		/* Setup front stub */
		int len = q->length - (res - q->data);
		int counter = 0;

		d (1, g_printerr ("MERGE needed (%d) which is >= %d + %d;\n",
			      num_bytes, offset, state->end_offset););

		do {
			d (1, g_printerr ("record %d) add %d bytes;\n", ++counter, len););
			/* copy necessary portion of current record */
			memcpy (tmp, res, len);
			tmp += len;

			/* Get next record */
			if (!ms_biff_query_next (q)) {
				g_warning ("unexpected end of stream;");
				return NULL;
			}

			/* We should only see DRAW records now */
			if (q->opcode != BIFF_MS_O_DRAWING &&
			    q->opcode != BIFF_MS_O_DRAWING_GROUP &&
			    q->opcode != BIFF_MS_O_DRAWING_SELECTION &&
			    q->opcode != BIFF_CHART_gelframe &&
			    q->opcode != BIFF_CONTINUE) {
			  g_warning ("Unexpected record type 0x%x @ 0x%lx;", q->opcode, (long)q->streamPos);
				return NULL;
			}

			state->start_offset = state->end_offset;
			state->end_offset += q->length;
			state->segment_len = q->length;

			res = q->data;
			len = q->length;

		} while ((num_bytes - (tmp - buffer)) > len);

		/* Copy back stub */
		memcpy (tmp, res, num_bytes - (tmp-buffer));
		d (1, g_printerr ("record %d) add %d bytes;\n",
			      ++counter,
			      num_bytes - (int)(tmp-buffer)););
		return buffer;
	}

	return res;
}