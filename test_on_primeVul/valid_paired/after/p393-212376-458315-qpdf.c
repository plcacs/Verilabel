Pl_Count::write(unsigned char* buf, size_t len)
{
    if (len)
    {
	this->m->count += QIntC::to_offset(len);
	this->m->last_char = buf[len - 1];
	getNext()->write(buf, len);
    }
}