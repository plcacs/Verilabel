decode_bits_in_field(const guint bit_offset, const gint no_of_bits, const guint64 value)
{
	guint64 mask = 0,tmp;
	char *str;
	int bit, str_p = 0;
	int i;

	mask = 1;
	mask = mask << (no_of_bits-1);

	/* Prepare the string, 256 pos for the bits and zero termination, + 64 for the spaces */
	str=(char *)wmem_alloc0(wmem_packet_scope(), 256+64);
	for(bit=0;bit<((int)(bit_offset&0x07));bit++){
		if(bit&&(!(bit%4))){
			str[str_p] = ' ';
			str_p++;
		}
		str[str_p] = '.';
		str_p++;
	}

	/* read the bits for the int */
	for(i=0;i<no_of_bits;i++){
		if(bit&&(!(bit%4))){
			str[str_p] = ' ';
			str_p++;
		}
		if(bit&&(!(bit%8))){
			str[str_p] = ' ';
			str_p++;
		}
		bit++;
		tmp = value & mask;
		if(tmp != 0){
			str[str_p] = '1';
			str_p++;
		} else {
			str[str_p] = '0';
			str_p++;
		}
		mask = mask>>1;
	}

	for(;bit%8;bit++){
		if(bit&&(!(bit%4))){
			str[str_p] = ' ';
			str_p++;
		}
		str[str_p] = '.';
		str_p++;
	}
	return str;
}