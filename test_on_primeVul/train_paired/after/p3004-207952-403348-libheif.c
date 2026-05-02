Fraction::Fraction(int32_t num,int32_t den)
{
  // Reduce resolution of fraction until we are in a safe range.
  // We need this as adding fractions may lead to very large denominators
  // (e.g. 0x10000 * 0x10000 > 0x100000000 -> overflow, leading to integer 0)

  while (denominator > MAX_FRACTION_VALUE || denominator < -MAX_FRACTION_VALUE) {
    numerator /= 2;
    denominator /= 2;
  }

  while (numerator > MAX_FRACTION_VALUE || numerator < -MAX_FRACTION_VALUE) {
    numerator /= 2;
    denominator /= 2;
  }
}