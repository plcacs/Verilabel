int64_t GmfOpenMesh(const char *FilNam, int mod, ...)
{
   int      KwdCod, res, *PtrVer, *PtrDim, err;
   int64_t  MshIdx;
   char     str[ GmfStrSiz ];
   va_list  VarArg;
   GmfMshSct *msh;

   /*---------------------*/
   /* MESH STRUCTURE INIT */
   /*---------------------*/

   if(!(msh = calloc(1, sizeof(GmfMshSct))))
      return(0);

   MshIdx = (int64_t)msh;

   // Save the current stack environment for longjmp
   if( (err = setjmp(msh->err)) != 0)
   {
#ifdef GMFDEBUG
      printf("libMeshb : mesh %p : error %d\n", msh, err);
#endif
      if(msh->hdl != NULL)
         fclose(msh->hdl);

      if(msh->FilDes != 0)
#ifdef GMF_WINDOWS
         _close(msh->FilDes);
#else
         close(msh->FilDes);
#endif

      free(msh);
      return(0);
   }

   // Copy the FilNam into the structure
   if(strlen(FilNam) + 7 >= GmfStrSiz)
      longjmp(msh->err, -4);

   strcpy(msh->FilNam, FilNam);

   // Store the opening mod (read or write) and guess
   // the filetype (binary or ascii) depending on the extension
   msh->mod = mod;
   msh->buf = (void *)msh->DblBuf;
   msh->FltBuf = (void *)msh->DblBuf;
   msh->IntBuf = (void *)msh->DblBuf;

   if(strstr(msh->FilNam, ".meshb"))
      msh->typ |= (Bin | MshFil);
   else if(strstr(msh->FilNam, ".mesh"))
      msh->typ |= (Asc | MshFil);
   else if(strstr(msh->FilNam, ".solb"))
      msh->typ |= (Bin | SolFil);
   else if(strstr(msh->FilNam, ".sol"))
      msh->typ |= (Asc | SolFil);
   else
      longjmp(msh->err, -5);

   // Open the file in the required mod and initialize the mesh structure
   if(msh->mod == GmfRead)
   {

      /*-----------------------*/
      /* OPEN FILE FOR READING */
      /*-----------------------*/

      va_start(VarArg, mod);
      PtrVer = va_arg(VarArg, int *);
      PtrDim = va_arg(VarArg, int *);
      va_end(VarArg);

      // Read the endian coding tag, the mesh version
      // and the mesh dimension (mandatory kwd)
      if(msh->typ & Bin)
      {
         // Create the name string and open the file
#ifdef WITH_GMF_AIO
         // [Bruno] added binary flag (necessary under Windows)
         msh->FilDes = open(msh->FilNam, OPEN_READ_FLAGS, OPEN_READ_MODE);

         if(msh->FilDes <= 0)
            longjmp(msh->err, -6);

         // Read the endian coding tag
         if(read(msh->FilDes, &msh->cod, WrdSiz) != WrdSiz)
            longjmp(msh->err, -7);
#else
         // [Bruno] added binary flag (necessary under Windows)
         if(!(msh->hdl = fopen(msh->FilNam, "rb")))
            longjmp(msh->err, -8);

         // Read the endian coding tag
         safe_fread(&msh->cod, WrdSiz, 1, msh->hdl, msh->err);
#endif

         // Read the mesh version and the mesh dimension (mandatory kwd)
         if( (msh->cod != 1) && (msh->cod != 16777216) )
            longjmp(msh->err, -9);

         ScaWrd(msh, (unsigned char *)&msh->ver);

         if( (msh->ver < 1) || (msh->ver > 4) )
            longjmp(msh->err, -10);

         if( (msh->ver >= 3) && (sizeof(int64_t) != 8) )
            longjmp(msh->err, -11);

         ScaWrd(msh, (unsigned char *)&KwdCod);

         if(KwdCod != GmfDimension)
            longjmp(msh->err, -12);

         GetPos(msh);
         ScaWrd(msh, (unsigned char *)&msh->dim);
      }
      else
      {
         // Create the name string and open the file
         if(!(msh->hdl = fopen(msh->FilNam, "rb")))
            longjmp(msh->err, -13);

         do
         {
            res = fscanf(msh->hdl, "%s", str);
         }while( (res != EOF) && strcmp(str, "MeshVersionFormatted") );

         if(res == EOF)
            longjmp(msh->err, -14);

         safe_fscanf(msh->hdl, "%d", &msh->ver, msh->err);

         if( (msh->ver < 1) || (msh->ver > 4) )
            longjmp(msh->err, -15);

         do
         {
            res = fscanf(msh->hdl, "%s", str);
         }while( (res != EOF) && strcmp(str, "Dimension") );

         if(res == EOF)
            longjmp(msh->err, -16);

         safe_fscanf(msh->hdl, "%d", &msh->dim, msh->err);
      }

      if( (msh->dim != 2) && (msh->dim != 3) )
         longjmp(msh->err, -17);

      (*PtrVer) = msh->ver;
      (*PtrDim) = msh->dim;

      // Set default real numbers size
      if(msh->ver == 1)
         msh->FltSiz = 32;
      else
         msh->FltSiz = 64;

      /*------------*/
      /* KW READING */
      /*------------*/

      // Read the list of kw present in the file
      if(!ScaKwdTab(msh))
         return(0);

      return(MshIdx);
   }
   else if(msh->mod == GmfWrite)
   {

      /*-----------------------*/
      /* OPEN FILE FOR WRITING */
      /*-----------------------*/

      msh->cod = 1;

      // Check if the user provided a valid version number and dimension
      va_start(VarArg, mod);
      msh->ver = va_arg(VarArg, int);
      msh->dim = va_arg(VarArg, int);
      va_end(VarArg);

      if( (msh->ver < 1) || (msh->ver > 4) )
         longjmp(msh->err, -18);

      if( (msh->ver >= 3) && (sizeof(int64_t) != 8) )
         longjmp(msh->err, -19);

      if( (msh->dim != 2) && (msh->dim != 3) )
         longjmp(msh->err, -20);

      // Set default real numbers size
      if(msh->ver == 1)
         msh->FltSiz = 32;
      else
         msh->FltSiz = 64;

      // Create the mesh file
      if(msh->typ & Bin) 
      {
         /* 
          * [Bruno] replaced previous call to creat():
          * with a call to open(), because Windows needs the
          * binary flag to be specified.
          */
#ifdef WITH_GMF_AIO
         msh->FilDes = open(msh->FilNam, OPEN_WRITE_FLAGS, OPEN_WRITE_MODE);

         if(msh->FilDes <= 0)
            longjmp(msh->err, -21);
#else
         if(!(msh->hdl = fopen(msh->FilNam, "wb")))
            longjmp(msh->err, -22);
#endif
      }
      else if(!(msh->hdl = fopen(msh->FilNam, "wb")))
         longjmp(msh->err, -23);


      /*------------*/
      /* KW WRITING */
      /*------------*/

      // Write the mesh version and dimension
      if(msh->typ & Asc)
      {
         fprintf(msh->hdl, "%s %d\n\n",
               GmfKwdFmt[ GmfVersionFormatted ][0], msh->ver);
         fprintf(msh->hdl, "%s %d\n",
               GmfKwdFmt[ GmfDimension ][0], msh->dim);
      }
      else
      {
         RecWrd(msh, (unsigned char *)&msh->cod);
         RecWrd(msh, (unsigned char *)&msh->ver);
         GmfSetKwd(MshIdx, GmfDimension, 0);
         RecWrd(msh, (unsigned char *)&msh->dim);
      }

      return(MshIdx);
   }
   else
   {
      free(msh);
      return(0);
   }
}