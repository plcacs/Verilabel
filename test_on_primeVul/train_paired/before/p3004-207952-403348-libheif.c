Fraction::Fraction(int32_t num,int32_t den)
{
  int32_t g = gcd(num, den);

  // these strange tests are for catching the case that we divide -2147483648 by -1,
  // which would exceed the maximum positive value by one.

  if (num == std::numeric_limits<int32_t>::min() && g == -1) {
    num++;
  }

  if (den == std::numeric_limits<int32_t>::min() && g == -1) {
    den++;
  }


  numerator = num / g;
  denominator = den / g;


  // Reduce resolution of fraction until we are in a safe range.
  // We need this as adding fractions may lead to very large denominators
  // (e.g. 0x10000 * 0x10000 > 0x100000000 -> overflow, leading to integer 0)

  while (denominator > MAX_FRACTION_DENOMINATOR) {
    numerator >>= 1;
    denominator >>= 1;
  }
}