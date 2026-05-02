
    CImg<T>& _load_bmp(std::FILE *const file, const char *const filename) {
      if (!file && !filename)
        throw CImgArgumentException(_cimg_instance
                                    "load_bmp(): Specified filename is (null).",
                                    cimg_instance);

      std::FILE *const nfile = file?file:cimg::fopen(filename,"rb");
      CImg<ucharT> header(54);
      cimg::fread(header._data,54,nfile);
      if (*header!='B' || header[1]!='M') {
        if (!file) cimg::fclose(nfile);
        throw CImgIOException(_cimg_instance
                              "load_bmp(): Invalid BMP file '%s'.",
                              cimg_instance,
                              filename?filename:"(FILE*)");
      }

      // Read header and pixel buffer
      int
        file_size = header[0x02] + (header[0x03]<<8) + (header[0x04]<<16) + (header[0x05]<<24),
        offset = header[0x0A] + (header[0x0B]<<8) + (header[0x0C]<<16) + (header[0x0D]<<24),
        header_size = header[0x0E] + (header[0x0F]<<8) + (header[0x10]<<16) + (header[0x11]<<24),
        dx = header[0x12] + (header[0x13]<<8) + (header[0x14]<<16) + (header[0x15]<<24),
        dy = header[0x16] + (header[0x17]<<8) + (header[0x18]<<16) + (header[0x19]<<24),
        compression = header[0x1E] + (header[0x1F]<<8) + (header[0x20]<<16) + (header[0x21]<<24),
        nb_colors = header[0x2E] + (header[0x2F]<<8) + (header[0x30]<<16) + (header[0x31]<<24),
        bpp = header[0x1C] + (header[0x1D]<<8);

      if (!file_size || file_size==offset) {
        cimg::fseek(nfile,0,SEEK_END);
        file_size = (int)cimg::ftell(nfile);
        cimg::fseek(nfile,54,SEEK_SET);
      }
      if (header_size>40) cimg::fseek(nfile,header_size - 40,SEEK_CUR);

      const int
        dx_bytes = (bpp==1)?(dx/8 + (dx%8?1:0)):((bpp==4)?(dx/2 + (dx%2)):(int)((longT)dx*bpp/8)),
        align_bytes = (4 - dx_bytes%4)%4;
      const ulongT
        cimg_iobuffer = (ulongT)24*1024*1024,
        buf_size = std::min((ulongT)cimg::abs(dy)*(dx_bytes + align_bytes),(ulongT)file_size - offset);

      CImg<intT> colormap;
      if (bpp<16) { if (!nb_colors) nb_colors = 1<<bpp; } else nb_colors = 0;
      if (nb_colors) { colormap.assign(nb_colors); cimg::fread(colormap._data,nb_colors,nfile); }
      const int xoffset = offset - 14 - header_size - 4*nb_colors;
      if (xoffset>0) cimg::fseek(nfile,xoffset,SEEK_CUR);

      CImg<ucharT> buffer;
      if (buf_size<cimg_iobuffer) {
        buffer.assign(cimg::abs(dy)*(dx_bytes + align_bytes),1,1,1,0);
        cimg::fread(buffer._data,buf_size,nfile);
      } else buffer.assign(dx_bytes + align_bytes);
      unsigned char *ptrs = buffer;

      // Decompress buffer (if necessary)
      if (compression) {
        if (file)
          throw CImgIOException(_cimg_instance
                                "load_bmp(): Unable to load compressed data from '(*FILE)' inputs.",
                                cimg_instance);
        else {
          if (!file) cimg::fclose(nfile);
          return load_other(filename);
        }
      }

      // Read pixel data
      assign(dx,cimg::abs(dy),1,3,0);
      switch (bpp) {
      case 1 : { // Monochrome
        if (colormap._width>=2) for (int y = height() - 1; y>=0; --y) {
          if (buf_size>=cimg_iobuffer) {
            if (!cimg::fread(ptrs=buffer._data,dx_bytes,nfile)) break;
            cimg::fseek(nfile,align_bytes,SEEK_CUR);
          }
          unsigned char mask = 0x80, val = 0;
          cimg_forX(*this,x) {
            if (mask==0x80) val = *(ptrs++);
            const unsigned char *col = (unsigned char*)(colormap._data + (val&mask?1:0));
            (*this)(x,y,2) = (T)*(col++);
            (*this)(x,y,1) = (T)*(col++);
            (*this)(x,y,0) = (T)*(col++);
            mask = cimg::ror(mask);
          }
          ptrs+=align_bytes;
        }
      } break;
      case 4 : { // 16 colors
        if (colormap._width>=16) for (int y = height() - 1; y>=0; --y) {
          if (buf_size>=cimg_iobuffer) {
            if (!cimg::fread(ptrs=buffer._data,dx_bytes,nfile)) break;
            cimg::fseek(nfile,align_bytes,SEEK_CUR);
          }
          unsigned char mask = 0xF0, val = 0;
          cimg_forX(*this,x) {
            if (mask==0xF0) val = *(ptrs++);
            const unsigned char color = (unsigned char)((mask<16)?(val&mask):((val&mask)>>4));
            const unsigned char *col = (unsigned char*)(colormap._data + color);
            (*this)(x,y,2) = (T)*(col++);
            (*this)(x,y,1) = (T)*(col++);
            (*this)(x,y,0) = (T)*(col++);
            mask = cimg::ror(mask,4);
          }
          ptrs+=align_bytes;
        }
      } break;
      case 8 : { // 256 colors
        if (colormap._width>=256) for (int y = height() - 1; y>=0; --y) {
          if (buf_size>=cimg_iobuffer) {
            if (!cimg::fread(ptrs=buffer._data,dx_bytes,nfile)) break;
            cimg::fseek(nfile,align_bytes,SEEK_CUR);
          }
          cimg_forX(*this,x) {
            const unsigned char *col = (unsigned char*)(colormap._data + *(ptrs++));
            (*this)(x,y,2) = (T)*(col++);
            (*this)(x,y,1) = (T)*(col++);
            (*this)(x,y,0) = (T)*(col++);
          }
          ptrs+=align_bytes;
        }
      } break;
      case 16 : { // 16 bits colors
        for (int y = height() - 1; y>=0; --y) {
          if (buf_size>=cimg_iobuffer) {
            if (!cimg::fread(ptrs=buffer._data,dx_bytes,nfile)) break;
            cimg::fseek(nfile,align_bytes,SEEK_CUR);
          }
          cimg_forX(*this,x) {
            const unsigned char c1 = *(ptrs++), c2 = *(ptrs++);
            const unsigned short col = (unsigned short)(c1|(c2<<8));
            (*this)(x,y,2) = (T)(col&0x1F);
            (*this)(x,y,1) = (T)((col>>5)&0x1F);
            (*this)(x,y,0) = (T)((col>>10)&0x1F);
          }
          ptrs+=align_bytes;
        }
      } break;
      case 24 : { // 24 bits colors
        for (int y = height() - 1; y>=0; --y) {
          if (buf_size>=cimg_iobuffer) {
            if (!cimg::fread(ptrs=buffer._data,dx_bytes,nfile)) break;
            cimg::fseek(nfile,align_bytes,SEEK_CUR);
          }
          cimg_forX(*this,x) {
            (*this)(x,y,2) = (T)*(ptrs++);
            (*this)(x,y,1) = (T)*(ptrs++);
            (*this)(x,y,0) = (T)*(ptrs++);
          }
          ptrs+=align_bytes;
        }
      } break;
      case 32 : { // 32 bits colors
        for (int y = height() - 1; y>=0; --y) {
          if (buf_size>=cimg_iobuffer) {
            if (!cimg::fread(ptrs=buffer._data,dx_bytes,nfile)) break;
            cimg::fseek(nfile,align_bytes,SEEK_CUR);
          }
          cimg_forX(*this,x) {
            (*this)(x,y,2) = (T)*(ptrs++);
            (*this)(x,y,1) = (T)*(ptrs++);
            (*this)(x,y,0) = (T)*(ptrs++);
            ++ptrs;
          }
          ptrs+=align_bytes;
        }
      } break;
      }
      if (dy<0) mirror('y');
      if (!file) cimg::fclose(nfile);
      return *this;