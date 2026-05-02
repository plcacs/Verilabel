rndr_underline(struct buf *ob, const struct buf *text, void *opaque)
{
	if (!text || !text->size)
		return 0;

	BUFPUTSL(ob, "<u>");
	bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</u>");

	return 1;
}