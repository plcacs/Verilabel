check_offset(char *memory, int total_size, char *name, void *offset, int size)
{
	char* memory_end = memory + total_size;
	char* block = (char*)offset;
	char* block_end = offset + size;

	/*debug("check_offset: size=%x vs %x offset=%x size=%x\n",
		need_size, total_size, (char *) offset - memory, size);*/

	if (((memory <= memory_end) && (block <= block_end))
		&& ((block < memory) || (block >= memory_end) || (block_end > memory_end))) {
		warn(_("%s: premature end"), name);
		return false;
	}

	return true;
}