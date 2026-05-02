asn1_read_value_type (asn1_node root, const char *name, void *ivalue,
		      int *len, unsigned int *etype)
{
  asn1_node node, p, p2;
  int len2, len3, result;
  int value_size = *len;
  unsigned char *value = ivalue;
  unsigned type;

  node = asn1_find_node (root, name);
  if (node == NULL)
    return ASN1_ELEMENT_NOT_FOUND;

  type = type_field (node->type);

  if ((type != ASN1_ETYPE_NULL) &&
      (type != ASN1_ETYPE_CHOICE) &&
      !(node->type & CONST_DEFAULT) && !(node->type & CONST_ASSIGN) &&
      (node->value == NULL))
    return ASN1_VALUE_NOT_FOUND;

  if (etype)
    *etype = type;
  switch (type)
    {
    case ASN1_ETYPE_NULL:
      PUT_STR_VALUE (value, value_size, "NULL");
      break;
    case ASN1_ETYPE_BOOLEAN:
      if ((node->type & CONST_DEFAULT) && (node->value == NULL))
	{
	  p = node->down;
	  while (type_field (p->type) != ASN1_ETYPE_DEFAULT)
	    p = p->right;
	  if (p->type & CONST_TRUE)
	    {
	      PUT_STR_VALUE (value, value_size, "TRUE");
	    }
	  else
	    {
	      PUT_STR_VALUE (value, value_size, "FALSE");
	    }
	}
      else if (node->value[0] == 'T')
	{
	  PUT_STR_VALUE (value, value_size, "TRUE");
	}
      else
	{
	  PUT_STR_VALUE (value, value_size, "FALSE");
	}
      break;
    case ASN1_ETYPE_INTEGER:
    case ASN1_ETYPE_ENUMERATED:
      if ((node->type & CONST_DEFAULT) && (node->value == NULL))
	{
	  p = node->down;
	  while (type_field (p->type) != ASN1_ETYPE_DEFAULT)
	    p = p->right;
	  if ((isdigit (p->value[0])) || (p->value[0] == '-')
	      || (p->value[0] == '+'))
	    {
	      result = _asn1_convert_integer
		  (p->value, value, value_size, len);
              if (result != ASN1_SUCCESS)
		return result;
	    }
	  else
	    {			/* is an identifier like v1 */
	      p2 = node->down;
	      while (p2)
		{
		  if (type_field (p2->type) == ASN1_ETYPE_CONSTANT)
		    {
		      if (!_asn1_strcmp (p2->name, p->value))
			{
			  result = _asn1_convert_integer
			      (p2->value, value, value_size,
			       len);
			  if (result != ASN1_SUCCESS)
			    return result;
			  break;
			}
		    }
		  p2 = p2->right;
		}
	    }
	}
      else
	{
	  len2 = -1;
	  result = asn1_get_octet_der
	      (node->value, node->value_len, &len2, value, value_size,
	       len);
          if (result != ASN1_SUCCESS)
	    return result;
	}
      break;
    case ASN1_ETYPE_OBJECT_ID:
      if (node->type & CONST_ASSIGN)
	{
	  if (value)
	  	value[0] = 0;
	  p = node->down;
	  while (p)
	    {
	      if (type_field (p->type) == ASN1_ETYPE_CONSTANT)
		{
		  ADD_STR_VALUE (value, value_size, p->value);
		  if (p->right)
		    {
		      ADD_STR_VALUE (value, value_size, ".");
		    }
		}
	      p = p->right;
	    }
	  *len = _asn1_strlen (value) + 1;
	}
      else if ((node->type & CONST_DEFAULT) && (node->value == NULL))
	{
	  p = node->down;
	  while (type_field (p->type) != ASN1_ETYPE_DEFAULT)
	    p = p->right;
	  PUT_STR_VALUE (value, value_size, p->value);
	}
      else
	{
	  PUT_STR_VALUE (value, value_size, node->value);
	}
      break;
    case ASN1_ETYPE_GENERALIZED_TIME:
    case ASN1_ETYPE_UTC_TIME:
      PUT_AS_STR_VALUE (value, value_size, node->value, node->value_len);
      break;
    case ASN1_ETYPE_OCTET_STRING:
    case ASN1_ETYPE_GENERALSTRING:
    case ASN1_ETYPE_NUMERIC_STRING:
    case ASN1_ETYPE_IA5_STRING:
    case ASN1_ETYPE_TELETEX_STRING:
    case ASN1_ETYPE_PRINTABLE_STRING:
    case ASN1_ETYPE_UNIVERSAL_STRING:
    case ASN1_ETYPE_BMP_STRING:
    case ASN1_ETYPE_UTF8_STRING:
    case ASN1_ETYPE_VISIBLE_STRING:
      len2 = -1;
      result = asn1_get_octet_der
	  (node->value, node->value_len, &len2, value, value_size,
	   len);
      if (result != ASN1_SUCCESS)
	return result;
      break;
    case ASN1_ETYPE_BIT_STRING:
      len2 = -1;
      result = asn1_get_bit_der
	  (node->value, node->value_len, &len2, value, value_size,
	   len);
      if (result != ASN1_SUCCESS)
	return result;
      break;
    case ASN1_ETYPE_CHOICE:
      PUT_STR_VALUE (value, value_size, node->down->name);
      break;
    case ASN1_ETYPE_ANY:
      len3 = -1;
      len2 = asn1_get_length_der (node->value, node->value_len, &len3);
      if (len2 < 0)
	return ASN1_DER_ERROR;
      PUT_VALUE (value, value_size, node->value + len3, len2);
      break;
    default:
      return ASN1_ELEMENT_NOT_FOUND;
      break;
    }
  return ASN1_SUCCESS;
}