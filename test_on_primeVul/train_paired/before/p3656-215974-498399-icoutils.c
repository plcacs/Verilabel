check_offset(char *memory, int total_size, char *name, void *offset, int size)
{
	ptrdiff_t need_size = (char *) offset - memory + size;

	/*debug("check_offset: size=%x vs %x offset=%x size=%x\n",
		need_size, total_size, (char *) offset - memory, size);*/

	if (need_size < 0 || need_size > total_size) {
		warn(_("%s: premature end"), name);
		return false;
	}

	return true;
}