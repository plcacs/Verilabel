static uint64_t unpack_timestamp(const struct efi_time *timestamp)
{
	uint64_t val = 0;
	uint16_t year = le32_to_cpu(timestamp->year);

	/* pad1, nanosecond, timezone, daylight and pad2 are meant to be zero */
	val |= ((uint64_t) timestamp->pad1 & 0xFF) << 0;
	val |= ((uint64_t) timestamp->second & 0xFF) << (1*8);
	val |= ((uint64_t) timestamp->minute & 0xFF) << (2*8);
	val |= ((uint64_t) timestamp->hour & 0xFF) << (3*8);
	val |= ((uint64_t) timestamp->day & 0xFF) << (4*8);
	val |= ((uint64_t) timestamp->month & 0xFF) << (5*8);
	val |= ((uint64_t) year) << (6*8);

	return val;
}