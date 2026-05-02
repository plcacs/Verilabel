static bool MR_primality_test(UnsignedBigInteger n, const Vector<UnsignedBigInteger, 256>& tests)
{
    // Written using Wikipedia:
    // https://en.wikipedia.org/wiki/Miller%E2%80%93Rabin_primality_test#Miller%E2%80%93Rabin_test
    ASSERT(!(n < 4));
    auto predecessor = n.minus({ 1 });
    auto d = predecessor;
    size_t r = 0;

    {
        auto div_result = d.divided_by(2);
        while (div_result.remainder == 0) {
            d = div_result.quotient;
            div_result = d.divided_by(2);
            ++r;
        }
    }
    if (r == 0) {
        // n - 1 is odd, so n was even. But there is only one even prime:
        return n == 2;
    }

    for (auto a : tests) {
        // Technically: ASSERT(2 <= a && a <= n - 2)
        ASSERT(a < n);
        auto x = ModularPower(a, d, n);
        if (x == 1 || x == predecessor)
            continue;
        bool skip_this_witness = false;
        // r − 1 iterations.
        for (size_t i = 0; i < r - 1; ++i) {
            x = ModularPower(x, 2, n);
            if (x == predecessor) {
                skip_this_witness = true;
                break;
            }
        }
        if (skip_this_witness)
            continue;
        return false; // "composite"
    }

    return true; // "probably prime"
}