xmlBufAttrSerializeTxtContent(xmlBufPtr buf, xmlDocPtr doc,
                              xmlAttrPtr attr, const xmlChar * string)
{
    xmlChar *base, *cur;

    if (string == NULL)
        return;
    base = cur = (xmlChar *) string;
    while (*cur != 0) {
        if (*cur == '\n') {
            if (base != cur)
                xmlBufAdd(buf, base, cur - base);
            xmlBufAdd(buf, BAD_CAST "&#10;", 5);
            cur++;
            base = cur;
        } else if (*cur == '\r') {
            if (base != cur)
                xmlBufAdd(buf, base, cur - base);
            xmlBufAdd(buf, BAD_CAST "&#13;", 5);
            cur++;
            base = cur;
        } else if (*cur == '\t') {
            if (base != cur)
                xmlBufAdd(buf, base, cur - base);
            xmlBufAdd(buf, BAD_CAST "&#9;", 4);
            cur++;
            base = cur;
        } else if (*cur == '"') {
            if (base != cur)
                xmlBufAdd(buf, base, cur - base);
            xmlBufAdd(buf, BAD_CAST "&quot;", 6);
            cur++;
            base = cur;
        } else if (*cur == '<') {
            if (base != cur)
                xmlBufAdd(buf, base, cur - base);
            xmlBufAdd(buf, BAD_CAST "&lt;", 4);
            cur++;
            base = cur;
        } else if (*cur == '>') {
            if (base != cur)
                xmlBufAdd(buf, base, cur - base);
            xmlBufAdd(buf, BAD_CAST "&gt;", 4);
            cur++;
            base = cur;
        } else if (*cur == '&') {
            if (base != cur)
                xmlBufAdd(buf, base, cur - base);
            xmlBufAdd(buf, BAD_CAST "&amp;", 5);
            cur++;
            base = cur;
        } else if ((*cur >= 0x80) && ((doc == NULL) ||
                                      (doc->encoding == NULL))) {
            /*
             * We assume we have UTF-8 content.
             */
            unsigned char tmp[12];
            int val = 0, l = 1;

            if (base != cur)
                xmlBufAdd(buf, base, cur - base);
            if (*cur < 0xC0) {
                xmlSaveErr(XML_SAVE_NOT_UTF8, (xmlNodePtr) attr, NULL);
                if (doc != NULL)
                    doc->encoding = xmlStrdup(BAD_CAST "ISO-8859-1");
		xmlSerializeHexCharRef(tmp, *cur);
                xmlBufAdd(buf, (xmlChar *) tmp, -1);
                cur++;
                base = cur;
                continue;
            } else if (*cur < 0xE0) {
                val = (cur[0]) & 0x1F;
                val <<= 6;
                val |= (cur[1]) & 0x3F;
                l = 2;
            } else if (*cur < 0xF0) {
                val = (cur[0]) & 0x0F;
                val <<= 6;
                val |= (cur[1]) & 0x3F;
                val <<= 6;
                val |= (cur[2]) & 0x3F;
                l = 3;
            } else if (*cur < 0xF8) {
                val = (cur[0]) & 0x07;
                val <<= 6;
                val |= (cur[1]) & 0x3F;
                val <<= 6;
                val |= (cur[2]) & 0x3F;
                val <<= 6;
                val |= (cur[3]) & 0x3F;
                l = 4;
            }
            if ((l == 1) || (!IS_CHAR(val))) {
                xmlSaveErr(XML_SAVE_CHAR_INVALID, (xmlNodePtr) attr, NULL);
                if (doc != NULL)
                    doc->encoding = xmlStrdup(BAD_CAST "ISO-8859-1");

		xmlSerializeHexCharRef(tmp, *cur);
                xmlBufAdd(buf, (xmlChar *) tmp, -1);
                cur++;
                base = cur;
                continue;
            }
            /*
             * We could do multiple things here. Just save
             * as a char ref
             */
	    xmlSerializeHexCharRef(tmp, val);
            xmlBufAdd(buf, (xmlChar *) tmp, -1);
            cur += l;
            base = cur;
        } else {
            cur++;
        }
    }
    if (base != cur)
        xmlBufAdd(buf, base, cur - base);
}