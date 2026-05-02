QPDFObjectHandle::parseInternal(PointerHolder<InputSource> input,
                                std::string const& object_description,
                                QPDFTokenizer& tokenizer, bool& empty,
                                StringDecrypter* decrypter, QPDF* context,
                                bool content_stream)
{
    // This method must take care not to resolve any objects. Don't
    // check the type of any object without first ensuring that it is
    // a direct object. Otherwise, doing so may have the side effect
    // of reading the object and changing the file pointer.

    empty = false;

    QPDFObjectHandle object;

    std::vector<std::vector<QPDFObjectHandle> > olist_stack;
    olist_stack.push_back(std::vector<QPDFObjectHandle>());
    std::vector<parser_state_e> state_stack;
    state_stack.push_back(st_top);
    std::vector<qpdf_offset_t> offset_stack;
    qpdf_offset_t offset = input->tell();
    offset_stack.push_back(offset);
    bool done = false;
    while (! done)
    {
        std::vector<QPDFObjectHandle>& olist = olist_stack.back();
        parser_state_e state = state_stack.back();
        offset = offset_stack.back();

	object = QPDFObjectHandle();

	QPDFTokenizer::Token token =
            tokenizer.readToken(input, object_description, true);

	switch (token.getType())
	{
          case QPDFTokenizer::tt_eof:
            if (! content_stream)
            {
                QTC::TC("qpdf", "QPDFObjectHandle eof in parseInternal");
                warn(context,
                     QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                             object_description,
                             input->getLastOffset(),
                             "unexpected EOF"));
            }
            state = st_eof;
            break;

          case QPDFTokenizer::tt_bad:
	    QTC::TC("qpdf", "QPDFObjectHandle bad token in parse");
            warn(context,
                 QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                         object_description,
                         input->getLastOffset(),
                         token.getErrorMessage()));
            object = newNull();
	    break;

	  case QPDFTokenizer::tt_brace_open:
	  case QPDFTokenizer::tt_brace_close:
	    QTC::TC("qpdf", "QPDFObjectHandle bad brace");
            warn(context,
                 QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                         object_description,
                         input->getLastOffset(),
                         "treating unexpected brace token as null"));
            object = newNull();
	    break;

	  case QPDFTokenizer::tt_array_close:
	    if (state == st_array)
	    {
                state = st_stop;
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDFObjectHandle bad array close");
                warn(context,
                     QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                             object_description,
                             input->getLastOffset(),
                             "treating unexpected array close token as null"));
                object = newNull();
	    }
	    break;

	  case QPDFTokenizer::tt_dict_close:
	    if (state == st_dictionary)
	    {
                state = st_stop;
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDFObjectHandle bad dictionary close");
                warn(context,
                     QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                             object_description,
                             input->getLastOffset(),
                             "unexpected dictionary close token"));
                object = newNull();
	    }
	    break;

	  case QPDFTokenizer::tt_array_open:
	  case QPDFTokenizer::tt_dict_open:
            olist_stack.push_back(std::vector<QPDFObjectHandle>());
            state = st_start;
            offset_stack.push_back(input->tell());
            state_stack.push_back(
                (token.getType() == QPDFTokenizer::tt_array_open) ?
                st_array : st_dictionary);
	    break;

	  case QPDFTokenizer::tt_bool:
	    object = newBool((token.getValue() == "true"));
	    break;

	  case QPDFTokenizer::tt_null:
	    object = newNull();
	    break;

	  case QPDFTokenizer::tt_integer:
	    object = newInteger(QUtil::string_to_ll(token.getValue().c_str()));
	    break;

	  case QPDFTokenizer::tt_real:
	    object = newReal(token.getValue());
	    break;

	  case QPDFTokenizer::tt_name:
	    object = newName(token.getValue());
	    break;

	  case QPDFTokenizer::tt_word:
	    {
		std::string const& value = token.getValue();
                if (content_stream)
                {
                    object = QPDFObjectHandle::newOperator(value);
                }
		else if ((value == "R") && (state != st_top) &&
                         (olist.size() >= 2) &&
                         (! olist.at(olist.size() - 1).isIndirect()) &&
                         (olist.at(olist.size() - 1).isInteger()) &&
                         (! olist.at(olist.size() - 2).isIndirect()) &&
                         (olist.at(olist.size() - 2).isInteger()))
		{
                    if (context == 0)
                    {
                        QTC::TC("qpdf", "QPDFObjectHandle indirect without context");
                        throw std::logic_error(
                            "QPDFObjectHandle::parse called without context"
                            " on an object with indirect references");
                    }
		    // Try to resolve indirect objects
		    object = newIndirect(
			context,
			olist.at(olist.size() - 2).getIntValue(),
			olist.at(olist.size() - 1).getIntValue());
		    olist.pop_back();
		    olist.pop_back();
		}
		else if ((value == "endobj") && (state == st_top))
		{
		    // We just saw endobj without having read
		    // anything.  Treat this as a null and do not move
		    // the input source's offset.
		    object = newNull();
		    input->seek(input->getLastOffset(), SEEK_SET);
                    empty = true;
		}
		else
		{
                    QTC::TC("qpdf", "QPDFObjectHandle treat word as string");
                    warn(context,
                         QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                                 object_description,
                                 input->getLastOffset(),
                                 "unknown token while reading object;"
                                 " treating as string"));
                    object = newString(value);
		}
	    }
	    break;

	  case QPDFTokenizer::tt_string:
	    {
		std::string val = token.getValue();
                if (decrypter)
                {
                    decrypter->decryptString(val);
                }
		object = QPDFObjectHandle::newString(val);
	    }

	    break;

	  default:
            warn(context,
                 QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                         object_description,
                         input->getLastOffset(),
                         "treating unknown token type as null while "
                         "reading object"));
            object = newNull();
	    break;
	}

        if ((! object.isInitialized()) &&
            (! ((state == st_start) ||
                (state == st_stop) ||
                (state == st_eof))))
        {
            throw std::logic_error(
                "QPDFObjectHandle::parseInternal: "
                "unexpected uninitialized object");
            object = newNull();
        }

        switch (state)
        {
          case st_eof:
            if (state_stack.size() > 1)
            {
                warn(context,
                     QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                             object_description,
                             input->getLastOffset(),
                             "parse error while reading object"));
            }
            done = true;
            // In content stream mode, leave object uninitialized to
            // indicate EOF
            if (! content_stream)
            {
                object = newNull();
            }
            break;

          case st_dictionary:
          case st_array:
            setObjectDescriptionFromInput(
                object, context, object_description, input,
                input->getLastOffset());
            olist.push_back(object);
            break;

          case st_top:
            done = true;
            break;

          case st_start:
            break;

          case st_stop:
            if ((state_stack.size() < 2) || (olist_stack.size() < 2))
            {
                throw std::logic_error(
                    "QPDFObjectHandle::parseInternal: st_stop encountered"
                    " with insufficient elements in stack");
            }
            parser_state_e old_state = state_stack.back();
            state_stack.pop_back();
            if (old_state == st_array)
            {
                object = newArray(olist);
                setObjectDescriptionFromInput(
                    object, context, object_description, input, offset);
            }
            else if (old_state == st_dictionary)
            {
                // Convert list to map. Alternating elements are keys.
                // Attempt to recover more or less gracefully from
                // invalid dictionaries.
                std::set<std::string> names;
                for (std::vector<QPDFObjectHandle>::iterator iter =
                         olist.begin();
                     iter != olist.end(); ++iter)
                {
                    if ((! (*iter).isIndirect()) && (*iter).isName())
                    {
                        names.insert((*iter).getName());
                    }
                }

                std::map<std::string, QPDFObjectHandle> dict;
                int next_fake_key = 1;
                for (unsigned int i = 0; i < olist.size(); ++i)
                {
                    QPDFObjectHandle key_obj = olist.at(i);
                    QPDFObjectHandle val;
                    if (key_obj.isIndirect() || (! key_obj.isName()))
                    {
                        bool found_fake = false;
                        std::string candidate;
                        while (! found_fake)
                        {
                            candidate =
                                "/QPDFFake" +
                                QUtil::int_to_string(next_fake_key++);
                            found_fake = (names.count(candidate) == 0);
                            QTC::TC("qpdf", "QPDFObjectHandle found fake",
                                    (found_fake ? 0 : 1));
                        }
                        warn(context,
                             QPDFExc(
                                 qpdf_e_damaged_pdf,
                                 input->getName(), object_description, offset,
                                 "expected dictionary key but found"
                                 " non-name object; inserting key " +
                                 candidate));
                        val = key_obj;
                        key_obj = newName(candidate);
                    }
                    else if (i + 1 >= olist.size())
                    {
                        QTC::TC("qpdf", "QPDFObjectHandle no val for last key");
                        warn(context,
                             QPDFExc(
                                 qpdf_e_damaged_pdf,
                                 input->getName(), object_description, offset,
                                 "dictionary ended prematurely; "
                                 "using null as value for last key"));
                        val = newNull();
                        setObjectDescriptionFromInput(
                            val, context, object_description, input, offset);
                    }
                    else
                    {
                        val = olist.at(++i);
                    }
                    dict[key_obj.getName()] = val;
                }
                object = newDictionary(dict);
                setObjectDescriptionFromInput(
                    object, context, object_description, input, offset);
            }
            olist_stack.pop_back();
            offset_stack.pop_back();
            if (state_stack.back() == st_top)
            {
                done = true;
            }
            else
            {
                olist_stack.back().push_back(object);
            }
        }
    }

    setObjectDescriptionFromInput(
        object, context, object_description, input, offset);
    return object;
}