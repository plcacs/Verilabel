p11_rpc_buffer_get_attribute (p11_buffer *buffer,
			      size_t *offset,
			      CK_ATTRIBUTE *attr)
{
	uint32_t type, length;
	unsigned char validity;
	p11_rpc_attribute_serializer *serializer;
	p11_rpc_value_type value_type;

	/* The attribute type */
	if (!p11_rpc_buffer_get_uint32 (buffer, offset, &type))
		return false;

	/* Attribute validity */
	if (!p11_rpc_buffer_get_byte (buffer, offset, &validity))
		return false;

	/* Not a valid attribute */
	if (!validity) {
		attr->ulValueLen = ((CK_ULONG)-1);
		attr->type = type;
		return true;
	}

	if (!p11_rpc_buffer_get_uint32 (buffer, offset, &length))
		return false;

	/* Decode the attribute value */
	value_type = map_attribute_to_value_type (type);
	assert (value_type < ELEMS (p11_rpc_attribute_serializers));
	serializer = &p11_rpc_attribute_serializers[value_type];
	assert (serializer != NULL);
	if (!serializer->decode (buffer, offset, attr->pValue, &attr->ulValueLen))
		return false;
	if (!attr->pValue)
		attr->ulValueLen = length;
	attr->type = type;
	return true;
}