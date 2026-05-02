string sanitize_proprietary_tags(string input_string)
{
  unsigned int i;
  bool strip = false;
  bool tag_open = false;
  int tag_open_idx = 0; //Are we within < > ?
  bool closing_tag_open = false; //Are we within </ > ?
  int orig_tag_open_idx = 0;
  bool proprietary_tag = false; //Are we within a proprietary element?
  bool proprietary_closing_tag = false;
  int crop_end_idx = 0;
  char buffer[READ_BUFFER_SIZE] = "";
  char tagname[READ_BUFFER_SIZE] = "";
  int tagname_idx = 0;
  char close_tagname[READ_BUFFER_SIZE] = "";

  for (i = 0; i < READ_BUFFER_SIZE; i++)
  {
    buffer[i] = 0;
    tagname[i] = 0;
    close_tagname[i] = 0;
  }

  size_t input_string_size = input_string.size();

  // Minimum workaround to prevent buffer overflow: Stop iterating
  // once the (fixed!) size of the output buffers is reached. In
  // response to
  // https://www.talosintelligence.com/vulnerability_reports/TALOS-2017-0317
  //
  // However, this code is a huge mess anyway and is in no way
  // anything like up-to-date C++ code. Please, anyone, replace it
  // with something more modern. Thanks. - cstim, 2017-09-17.
  for (i = 0; i < std::min(input_string_size, size_t(READ_BUFFER_SIZE)); i++)
  {
    if (input_string.c_str()[i] == '<')
    {
      tag_open = true;
      tag_open_idx = i;
      if (proprietary_tag == true && input_string.c_str()[i+1] == '/')
      {
        //We are now in a closing tag
        closing_tag_open = true;
        //cout<<"Comparaison: "<<tagname<<"|"<<&(input_string.c_str()[i+2])<<"|"<<strlen(tagname)<<endl;
        if (strncmp(tagname, &(input_string.c_str()[i+2]), strlen(tagname)) != 0)
        {
          //If it is the begining of an other tag
          //cout<<"DIFFERENT!"<<endl;
          crop_end_idx = i - 1;
          strip = true;
        }
        else
        {
          //Otherwise, it is the start of the closing tag of the proprietary tag
          proprietary_closing_tag = true;
        }
      }
      else if (proprietary_tag == true)
      {
        //It is the start of a new tag, following a proprietary tag
        crop_end_idx = i - 1;
        strip = true;
      }
    }
    else if (input_string.c_str()[i] == '>')
    {
      tag_open = false;
      closing_tag_open = false;
      tagname[tagname_idx] = 0;
      tagname_idx = 0;
      if (proprietary_closing_tag == true)
      {
        crop_end_idx = i;
        strip = true;
      }
    }
    else if (tag_open == true && closing_tag_open == false)
    {
      if (input_string.c_str()[i] == '.')
      {
        if (proprietary_tag != true)
        {
          orig_tag_open_idx = tag_open_idx;
          proprietary_tag = true;
        }
      }
      tagname[tagname_idx] = input_string.c_str()[i];
      tagname_idx++;
    }
    //cerr <<i<<endl;
    if (strip == true && orig_tag_open_idx < input_string.size())
    {
      input_string.copy(buffer, (crop_end_idx - orig_tag_open_idx) + 1, orig_tag_open_idx);
      message_out(INFO, "sanitize_proprietary_tags() (end tag or new tag) removed: " + string(buffer));
      input_string.erase(orig_tag_open_idx, (crop_end_idx - orig_tag_open_idx) + 1);
      i = orig_tag_open_idx - 1;
      proprietary_tag = false;
      proprietary_closing_tag = false;
      closing_tag_open = false;
      tag_open = false;
      strip = false;

      input_string_size = input_string.size();
    }

  }//end for
  if (proprietary_tag == true && orig_tag_open_idx < input_string.size())
  {
    if (crop_end_idx == 0)   //no closing tag
    {
      crop_end_idx = input_string.size() - 1;
    }
    input_string.copy(buffer, (crop_end_idx - orig_tag_open_idx) + 1, orig_tag_open_idx);
    message_out(INFO, "sanitize_proprietary_tags() (end of line) removed: " + string(buffer));
    input_string.erase(orig_tag_open_idx, (crop_end_idx - orig_tag_open_idx) + 1);
    input_string_size = input_string.size();
  }
  return input_string;
}