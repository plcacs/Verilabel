HtmlOutputDev::HtmlOutputDev(Catalog *catalogA, const char *fileName, const char *title,
	const char *author, const char *keywords, const char *subject, const char *date,
	bool rawOrder, int firstPage, bool outline) 
{
  catalog = catalogA;
  fContentsFrame = nullptr;
  docTitle = new GooString(title);
  pages = nullptr;
  dumpJPEG=true;
  //write = true;
  this->rawOrder = rawOrder;
  this->doOutline = outline;
  ok = false;
  //this->firstPage = firstPage;
  //pageNum=firstPage;
  // open file
  needClose = false;
  pages = new HtmlPage(rawOrder);
  
  glMetaVars = new std::vector<HtmlMetaVar*>();
  glMetaVars->push_back(new HtmlMetaVar("generator", "pdftohtml 0.36"));
  if( author ) glMetaVars->push_back(new HtmlMetaVar("author", author));
  if( keywords ) glMetaVars->push_back(new HtmlMetaVar("keywords", keywords));
  if( date ) glMetaVars->push_back(new HtmlMetaVar("date", date));
  if( subject ) glMetaVars->push_back(new HtmlMetaVar("subject", subject));

  maxPageWidth = 0;
  maxPageHeight = 0;

  pages->setDocName(fileName);
  Docname=new GooString (fileName);

  // for non-xml output (complex or simple) with frames generate the left frame
  if(!xml && !noframes)
  {
     if (!singleHtml)
     {
         GooString* left=new GooString(fileName);
         left->append("_ind.html");

         doFrame(firstPage);

         if (!(fContentsFrame = fopen(left->c_str(), "w")))
         {
             error(errIO, -1, "Couldn't open html file '{0:t}'", left);
             delete left;
             return;
         }
         delete left;
         fputs(DOCTYPE, fContentsFrame);
         fputs("<html xmlns=\"http://www.w3.org/1999/xhtml\" lang=\"\" xml:lang=\"\">\n<head>\n<title></title>\n</head>\n<body>\n", fContentsFrame);

         if (doOutline)
         {
             fprintf(fContentsFrame, "<a href=\"%s%s\" target=\"contents\">Outline</a><br/>",
                 gbasename(Docname->c_str()).c_str(),
                 complexMode ? "-outline.html" : "s.html#outline");
         }
     }
	if (!complexMode)
	{	/* not in complex mode */
		
       GooString* right=new GooString(fileName);
       right->append("s.html");

       if (!(page=fopen(right->c_str(),"w"))){
        error(errIO, -1, "Couldn't open html file '{0:t}'", right);
        delete right;
		return;
       }
       delete right;
       fputs(DOCTYPE, page);
       fputs("<html>\n<head>\n<title></title>\n",page);
       printCSS(page);
       fputs("</head>\n<body>\n",page);
     }
  }

  if (noframes) {
    if (stout) page=stdout;
    else {
      GooString* right=new GooString(fileName);
      if (!xml) right->append(".html");
      if (xml) right->append(".xml");
      if (!(page=fopen(right->c_str(),"w"))){
	error(errIO, -1, "Couldn't open html file '{0:t}'", right);
	delete right;
	return;
      }  
      delete right;
    }

    GooString *htmlEncoding = mapEncodingToHtml(globalParams->getTextEncodingName()); 
    if (xml) 
    {
      fprintf(page, "<?xml version=\"1.0\" encoding=\"%s\"?>\n", htmlEncoding->c_str());
      fputs("<!DOCTYPE pdf2xml SYSTEM \"pdf2xml.dtd\">\n\n", page);
      fprintf(page,"<pdf2xml producer=\"%s\" version=\"%s\">\n", PACKAGE_NAME, PACKAGE_VERSION);
    } 
    else 
    {
      fprintf(page,"%s\n<html xmlns=\"http://www.w3.org/1999/xhtml\" lang=\"\" xml:lang=\"\">\n<head>\n<title>%s</title>\n", DOCTYPE, docTitle->c_str());
      
      fprintf(page, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=%s\"/>\n", htmlEncoding->c_str());
      
      dumpMetaVars(page);
      printCSS(page);
      fprintf(page,"</head>\n");
      fprintf(page,"<body bgcolor=\"#A0A0A0\" vlink=\"blue\" link=\"blue\">\n");
    }
    delete htmlEncoding;
  }
  ok = true; 
}