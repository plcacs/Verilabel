Value ExpressionPow::evaluate(const Document& root) const {
    Value baseVal = vpOperand[0]->evaluate(root);
    Value expVal = vpOperand[1]->evaluate(root);
    if (baseVal.nullish() || expVal.nullish())
        return Value(BSONNULL);

    BSONType baseType = baseVal.getType();
    BSONType expType = expVal.getType();

    uassert(28762,
            str::stream() << "$pow's base must be numeric, not " << typeName(baseType),
            baseVal.numeric());
    uassert(28763,
            str::stream() << "$pow's exponent must be numeric, not " << typeName(expType),
            expVal.numeric());

    auto checkNonZeroAndNeg = [](bool isZeroAndNeg) {
        uassert(28764, "$pow cannot take a base of 0 and a negative exponent", !isZeroAndNeg);
    };

    // If either argument is decimal, return a decimal.
    if (baseType == NumberDecimal || expType == NumberDecimal) {
        Decimal128 baseDecimal = baseVal.coerceToDecimal();
        Decimal128 expDecimal = expVal.coerceToDecimal();
        checkNonZeroAndNeg(baseDecimal.isZero() && expDecimal.isNegative());
        return Value(baseDecimal.power(expDecimal));
    }

    // pow() will cast args to doubles.
    double baseDouble = baseVal.coerceToDouble();
    double expDouble = expVal.coerceToDouble();
    checkNonZeroAndNeg(baseDouble == 0 && expDouble < 0);

    // If either argument is a double, return a double.
    if (baseType == NumberDouble || expType == NumberDouble) {
        return Value(std::pow(baseDouble, expDouble));
    }

    // base and exp are both integers.

    auto representableAsLong = [](long long base, long long exp) {
        // If exp is greater than 63 and base is not -1, 0, or 1, the result will overflow.
        // If exp is negative and the base is not -1 or 1, the result will be fractional.
        if (exp < 0 || exp > 63) {
            return std::abs(base) == 1 || base == 0;
        }

        struct MinMax {
            long long min;
            long long max;
        };

        // Array indices correspond to exponents 0 through 63. The values in each index are the min
        // and max bases, respectively, that can be raised to that exponent without overflowing a
        // 64-bit int. For max bases, this was computed by solving for b in
        // b = (2^63-1)^(1/exp) for exp = [0, 63] and truncating b. To calculate min bases, for even
        // exps the equation  used was b = (2^63-1)^(1/exp), and for odd exps the equation used was
        // b = (-2^63)^(1/exp). Since the magnitude of long min is greater than long max, the
        // magnitude of some of the min bases raised to odd exps is greater than the corresponding
        // max bases raised to the same exponents.

        static const MinMax kBaseLimits[] = {
            {std::numeric_limits<long long>::min(), std::numeric_limits<long long>::max()},  // 0
            {std::numeric_limits<long long>::min(), std::numeric_limits<long long>::max()},
            {-3037000499LL, 3037000499LL},
            {-2097152, 2097151},
            {-55108, 55108},
            {-6208, 6208},
            {-1448, 1448},
            {-512, 511},
            {-234, 234},
            {-128, 127},
            {-78, 78},  // 10
            {-52, 52},
            {-38, 38},
            {-28, 28},
            {-22, 22},
            {-18, 18},
            {-15, 15},
            {-13, 13},
            {-11, 11},
            {-9, 9},
            {-8, 8},  // 20
            {-8, 7},
            {-7, 7},
            {-6, 6},
            {-6, 6},
            {-5, 5},
            {-5, 5},
            {-5, 5},
            {-4, 4},
            {-4, 4},
            {-4, 4},  // 30
            {-4, 4},
            {-3, 3},
            {-3, 3},
            {-3, 3},
            {-3, 3},
            {-3, 3},
            {-3, 3},
            {-3, 3},
            {-3, 3},
            {-2, 2},  // 40
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},  // 50
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},
            {-2, 2},  // 60
            {-2, 2},
            {-2, 2},
            {-2, 1}};

        return base >= kBaseLimits[exp].min && base <= kBaseLimits[exp].max;
    };

    long long baseLong = baseVal.getLong();
    long long expLong = expVal.getLong();

    // If the result cannot be represented as a long, return a double. Otherwise if either number
    // is a long, return a long. If both numbers are ints, then return an int if the result fits or
    // a long if it is too big.
    if (!representableAsLong(baseLong, expLong)) {
        return Value(std::pow(baseLong, expLong));
    }

    long long result = 1;
    // Use repeated multiplication, since pow() casts args to doubles which could result in loss of
    // precision if arguments are very large.
    for (int i = 0; i < expLong; i++) {
        result *= baseLong;
    }

    if (baseType == NumberLong || expType == NumberLong) {
        return Value(result);
    }
    return Value::createIntOrLong(result);
}