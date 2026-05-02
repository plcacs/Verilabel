bool CPlayListASX::LoadData(std::istream& stream)
{
  CLog::Log(LOGINFO, "Parsing ASX");

  if(stream.peek() == '[')
  {
    return LoadAsxIniInfo(stream);
  }
  else
  {
    std::string asxstream(std::istreambuf_iterator<char>(stream), {});
    CXBMCTinyXML xmlDoc;
    xmlDoc.Parse(asxstream, TIXML_DEFAULT_ENCODING);

    if (xmlDoc.Error())
    {
      CLog::Log(LOGERROR, "Unable to parse ASX info Error: {}", xmlDoc.ErrorDesc());
      return false;
    }

    TiXmlElement *pRootElement = xmlDoc.RootElement();

    if (!pRootElement)
      return false;

    // lowercase every element
    TiXmlNode *pNode = pRootElement;
    TiXmlNode *pChild = NULL;
    std::string value;
    value = pNode->Value();
    StringUtils::ToLower(value);
    pNode->SetValue(value);
    while(pNode)
    {
      pChild = pNode->IterateChildren(pChild);
      if(pChild)
      {
        if (pChild->Type() == TiXmlNode::TINYXML_ELEMENT)
        {
          value = pChild->Value();
          StringUtils::ToLower(value);
          pChild->SetValue(value);

          TiXmlAttribute* pAttr = pChild->ToElement()->FirstAttribute();
          while(pAttr)
          {
            value = pAttr->Name();
            StringUtils::ToLower(value);
            pAttr->SetName(value);
            pAttr = pAttr->Next();
          }
        }

        pNode = pChild;
        pChild = NULL;
        continue;
      }

      pChild = pNode;
      pNode = pNode->Parent();
    }
    std::string roottitle;
    TiXmlElement *pElement = pRootElement->FirstChildElement();
    while (pElement)
    {
      value = pElement->Value();
      if (value == "title" && !pElement->NoChildren())
      {
        roottitle = pElement->FirstChild()->ValueStr();
      }
      else if (value == "entry")
      {
        std::string title(roottitle);

        TiXmlElement *pRef = pElement->FirstChildElement("ref");
        TiXmlElement *pTitle = pElement->FirstChildElement("title");

        if(pTitle && !pTitle->NoChildren())
          title = pTitle->FirstChild()->ValueStr();

        while (pRef)
        { // multiple references may appear for one entry
          // duration may exist on this level too
          value = XMLUtils::GetAttribute(pRef, "href");
          if (!value.empty())
          {
            if(title.empty())
              title = value;

            CLog::Log(LOGINFO, "Adding element {}, {}", title, value);
            CFileItemPtr newItem(new CFileItem(title));
            newItem->SetPath(value);
            Add(newItem);
          }
          pRef = pRef->NextSiblingElement("ref");
        }
      }
      else if (value == "entryref")
      {
        value = XMLUtils::GetAttribute(pElement, "href");
        if (!value.empty())
        { // found an entryref, let's try loading that url
          std::unique_ptr<CPlayList> playlist(CPlayListFactory::Create(value));
          if (nullptr != playlist)
            if (playlist->Load(value))
              Add(*playlist);
        }
      }
      pElement = pElement->NextSiblingElement();
    }
  }

  return true;
}