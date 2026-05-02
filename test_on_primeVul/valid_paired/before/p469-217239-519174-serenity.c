String UTF16BEDecoder::to_utf8(const StringView& input)
{
    StringBuilder builder(input.length() / 2);
    for (size_t i = 0; i < input.length(); i += 2) {
        u16 code_point = (input[i] << 8) | input[i + 1];
        builder.append_code_point(code_point);
    }
    return builder.to_string();
}