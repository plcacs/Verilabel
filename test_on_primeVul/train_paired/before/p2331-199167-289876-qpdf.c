QPDFObjectHandle::parseInternal(PointerHolder<InputSource> input,
                                std::string const& object_description,
                                QPDFTokenizer& tokenizer, bool& empty,
                                StringDecrypter* decrypter, QPDF* context,
                                bool in_array, bool in_dictionary,
                                bool content_stream)
{
    // This method must take care not to resolve any objects. Don't
    // check the type of any object without first ensuring that it is
    // a direct object. Otherwise, doing so may have the side effect
    // of reading the object and changing the file pointer.

    empty = false;
    if (in_dictionary && in_array)
    {
	// Although dictionaries and arrays arbitrarily nest, these
	// variables indicate what is at the top of the stack right
	// now, so they can, by definition, never both be true.
	throw std::logic_error(
	    "INTERNAL ERROR: parseInternal: in_dict && in_array");
    }

    QPDFObjectHandle object;

    qpdf_offset_t offset = input->tell();
    std::vector<QPDFObjectHandle> olist;
    bool done = false;
    while (! done)
    {
	object = QPDFObjectHandle();

	QPDFTokenizer::Token token =
            tokenizer.readToken(input, object_description);

	switch (token.getType())
	{
          case QPDFTokenizer::tt_eof:
            if (content_stream)
            {
                // Return uninitialized object to indicate EOF
                return object;
            }
            else
            {
                // When not in content stream mode, EOF is tt_bad and
                // throws an exception before we get here.
                throw std::logic_error(
                    "EOF received while not in content stream mode");
            }
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
	    if (in_array)
	    {
		done = true;
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
	    if (in_dictionary)
	    {
		done = true;
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
	    object = parseInternal(
		input, object_description, tokenizer, empty,
                decrypter, context, true, false, content_stream);
	    break;

	  case QPDFTokenizer::tt_dict_open:
	    object = parseInternal(
		input, object_description, tokenizer, empty,
                decrypter, context, false, true, content_stream);
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
		else if ((value == "R") && (in_array || in_dictionary) &&
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
		else if ((value == "endobj") &&
			 (! (in_array || in_dictionary)))
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

	if (in_dictionary || in_array)
	{
	    if (! done)
	    {
		olist.push_back(object);
	    }
	}
	else if (! object.isInitialized())
	{
            warn(context,
                 QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                         object_description,
                         input->getLastOffset(),
                         "parse error while reading object"));
            object = newNull();
	}
	else
	{
	    done = true;
	}
    }

    if (in_array)
    {
	object = newArray(olist);
    }
    else if (in_dictionary)
    {
        // Convert list to map. Alternating elements are keys. Attempt
        // to recover more or less gracefully from invalid
        // dictionaries.
        std::set<std::string> names;
        for (std::vector<QPDFObjectHandle>::iterator iter = olist.begin();
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
                        "/QPDFFake" + QUtil::int_to_string(next_fake_key++);
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
                         "dictionary ended prematurely; using null as value"
                         " for last key"));
                val = newNull();
            }
            else
            {
                val = olist.at(++i);
            }
            dict[key_obj.getName()] = val;
        }
        object = newDictionary(dict);
    }

    return object;
}