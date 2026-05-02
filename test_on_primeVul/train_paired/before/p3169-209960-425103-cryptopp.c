bool Inflator::DecodeBody()
{
	bool blockEnd = false;
	switch (m_blockType)
	{
	case 0:	// stored
		CRYPTOPP_ASSERT(m_reader.BitsBuffered() == 0);
		while (!m_inQueue.IsEmpty() && !blockEnd)
		{
			size_t size;
			const byte *block = m_inQueue.Spy(size);
			size = UnsignedMin(m_storedLen, size);
			CRYPTOPP_ASSERT(size <= 0xffff);

			OutputString(block, size);
			m_inQueue.Skip(size);
			m_storedLen = m_storedLen - (word16)size;
			if (m_storedLen == 0)
				blockEnd = true;
		}
		break;
	case 1:	// fixed codes
	case 2:	// dynamic codes
		static const unsigned int lengthStarts[] = {
			3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
			35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
		static const unsigned int lengthExtraBits[] = {
			0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
			3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
		static const unsigned int distanceStarts[] = {
			1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
			257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
			8193, 12289, 16385, 24577};
		static const unsigned int distanceExtraBits[] = {
			0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
			7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
			12, 12, 13, 13};

		const HuffmanDecoder& literalDecoder = GetLiteralDecoder();
		const HuffmanDecoder& distanceDecoder = GetDistanceDecoder();

		switch (m_nextDecode)
		{
		case LITERAL:
			while (true)
			{
				if (!literalDecoder.Decode(m_reader, m_literal))
				{
					m_nextDecode = LITERAL;
					break;
				}
				if (m_literal < 256)
					OutputByte((byte)m_literal);
				else if (m_literal == 256)	// end of block
				{
					blockEnd = true;
					break;
				}
				else
				{
					if (m_literal > 285)
						throw BadBlockErr();
					unsigned int bits;
		case LENGTH_BITS:
					bits = lengthExtraBits[m_literal-257];
					if (!m_reader.FillBuffer(bits))
					{
						m_nextDecode = LENGTH_BITS;
						break;
					}
					m_literal = m_reader.GetBits(bits) + lengthStarts[m_literal-257];
		case DISTANCE:
					if (!distanceDecoder.Decode(m_reader, m_distance))
					{
						m_nextDecode = DISTANCE;
						break;
					}
		case DISTANCE_BITS:
					// TODO: this surfaced during fuzzing. What do we do???
					CRYPTOPP_ASSERT(m_distance < COUNTOF(distanceExtraBits));
					bits = (m_distance >= COUNTOF(distanceExtraBits)) ? distanceExtraBits[29] : distanceExtraBits[m_distance];
					if (!m_reader.FillBuffer(bits))
					{
						m_nextDecode = DISTANCE_BITS;
						break;
					}
					m_distance = m_reader.GetBits(bits) + distanceStarts[m_distance];
					OutputPast(m_literal, m_distance);
				}
			}
			break;
		default:
			CRYPTOPP_ASSERT(0);
		}
	}
	if (blockEnd)
	{
		if (m_eof)
		{
			FlushOutput();
			m_reader.SkipBits(m_reader.BitsBuffered()%8);
			if (m_reader.BitsBuffered())
			{
				// undo too much lookahead
				SecBlockWithHint<byte, 4> buffer(m_reader.BitsBuffered() / 8);
				for (unsigned int i=0; i<buffer.size(); i++)
					buffer[i] = (byte)m_reader.GetBits(8);
				m_inQueue.Unget(buffer, buffer.size());
			}
			m_state = POST_STREAM;
		}
		else
			m_state = WAIT_HEADER;
	}
	return blockEnd;
}