void fli_read_brun(FILE *f, s_fli_header *fli_header, unsigned char *framebuf)
{
	unsigned short yc;
	unsigned char *pos;
	for (yc=0; yc < fli_header->height; yc++) {
		unsigned short pc, pcnt;
		size_t n, xc;
		pc=fli_read_char(f);
		xc=0;
		pos=framebuf+(fli_header->width * yc);
		n=(size_t)fli_header->width * (fli_header->height-yc);
		for (pcnt=pc; pcnt>0; pcnt--) {
			unsigned short ps;
			ps=fli_read_char(f);
			if (ps & 0x80) {
				unsigned short len;
				for (len=-(signed char)ps; len>0 && xc<n; len--) {
					pos[xc++]=fli_read_char(f);
				}
			} else {
				unsigned char val;
				size_t len;
				len=MIN(n-xc,ps);
				val=fli_read_char(f);
				memset(&(pos[xc]), val, len);
				xc+=len;
			}
		}
	}
}