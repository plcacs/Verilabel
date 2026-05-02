int ossl_rsaz_mod_exp_avx512_x2(BN_ULONG *res1,
                                const BN_ULONG *base1,
                                const BN_ULONG *exp1,
                                const BN_ULONG *m1,
                                const BN_ULONG *rr1,
                                BN_ULONG k0_1,
                                BN_ULONG *res2,
                                const BN_ULONG *base2,
                                const BN_ULONG *exp2,
                                const BN_ULONG *m2,
                                const BN_ULONG *rr2,
                                BN_ULONG k0_2,
                                int factor_size)
{
    typedef void (*AMM)(BN_ULONG *res, const BN_ULONG *a,
                        const BN_ULONG *b, const BN_ULONG *m, BN_ULONG k0);
    int ret = 0;

    /*
     * Number of word-size (BN_ULONG) digits to store exponent in redundant
     * representation.
     */
    int exp_digits = number_of_digits(factor_size + 2, DIGIT_SIZE);
    int coeff_pow = 4 * (DIGIT_SIZE * exp_digits - factor_size);

    /*  Number of YMM registers required to store exponent's digits */
    int ymm_regs_num = NUMBER_OF_REGISTERS(exp_digits, 256 /* ymm bit size */);
    /* Capacity of the register set (in qwords) to store exponent */
    int regs_capacity = ymm_regs_num * 4;

    BN_ULONG *base1_red, *m1_red, *rr1_red;
    BN_ULONG *base2_red, *m2_red, *rr2_red;
    BN_ULONG *coeff_red;
    BN_ULONG *storage = NULL;
    BN_ULONG *storage_aligned = NULL;
    int storage_len_bytes = 7 * regs_capacity * sizeof(BN_ULONG)
                           + 64 /* alignment */;

    const BN_ULONG *exp[2] = {0};
    BN_ULONG k0[2] = {0};
    /* AMM = Almost Montgomery Multiplication */
    AMM amm = NULL;

    switch (factor_size) {
    case 1024:
        amm = ossl_rsaz_amm52x20_x1_ifma256;
        break;
    case 1536:
        amm = ossl_rsaz_amm52x30_x1_ifma256;
        break;
    case 2048:
        amm = ossl_rsaz_amm52x40_x1_ifma256;
        break;
    default:
        goto err;
    }

    storage = (BN_ULONG *)OPENSSL_malloc(storage_len_bytes);
    if (storage == NULL)
        goto err;
    storage_aligned = (BN_ULONG *)ALIGN_OF(storage, 64);

    /* Memory layout for red(undant) representations */
    base1_red = storage_aligned;
    base2_red = storage_aligned + 1 * regs_capacity;
    m1_red    = storage_aligned + 2 * regs_capacity;
    m2_red    = storage_aligned + 3 * regs_capacity;
    rr1_red   = storage_aligned + 4 * regs_capacity;
    rr2_red   = storage_aligned + 5 * regs_capacity;
    coeff_red = storage_aligned + 6 * regs_capacity;

    /* Convert base_i, m_i, rr_i, from regular to 52-bit radix */
    to_words52(base1_red, regs_capacity, base1, factor_size);
    to_words52(base2_red, regs_capacity, base2, factor_size);
    to_words52(m1_red,    regs_capacity, m1,    factor_size);
    to_words52(m2_red,    regs_capacity, m2,    factor_size);
    to_words52(rr1_red,   regs_capacity, rr1,   factor_size);
    to_words52(rr2_red,   regs_capacity, rr2,   factor_size);

    /*
     * Compute target domain Montgomery converters RR' for each modulus
     * based on precomputed original domain's RR.
     *
     * RR -> RR' transformation steps:
     *  (1) coeff = 2^k
     *  (2) t = AMM(RR,RR) = RR^2 / R' mod m
     *  (3) RR' = AMM(t, coeff) = RR^2 * 2^k / R'^2 mod m
     * where
     *  k = 4 * (52 * digits52 - modlen)
     *  R  = 2^(64 * ceil(modlen/64)) mod m
     *  RR = R^2 mod m
     *  R' = 2^(52 * ceil(modlen/52)) mod m
     *
     *  EX/ modlen = 1024: k = 64, RR = 2^2048 mod m, RR' = 2^2080 mod m
     */
    memset(coeff_red, 0, exp_digits * sizeof(BN_ULONG));
    /* (1) in reduced domain representation */
    set_bit(coeff_red, 64 * (int)(coeff_pow / 52) + coeff_pow % 52);

    amm(rr1_red, rr1_red, rr1_red, m1_red, k0_1);     /* (2) for m1 */
    amm(rr1_red, rr1_red, coeff_red, m1_red, k0_1);   /* (3) for m1 */

    amm(rr2_red, rr2_red, rr2_red, m2_red, k0_2);     /* (2) for m2 */
    amm(rr2_red, rr2_red, coeff_red, m2_red, k0_2);   /* (3) for m2 */

    exp[0] = exp1;
    exp[1] = exp2;

    k0[0] = k0_1;
    k0[1] = k0_2;

    /* Dual (2-exps in parallel) exponentiation */
    ret = RSAZ_mod_exp_x2_ifma256(rr1_red, base1_red, exp, m1_red, rr1_red,
                                  k0, factor_size);
    if (!ret)
        goto err;

    /* Convert rr_i back to regular radix */
    from_words52(res1, factor_size, rr1_red);
    from_words52(res2, factor_size, rr2_red);

    bn_reduce_once_in_place(res1, /*carry=*/0, m1, storage, factor_size);
    bn_reduce_once_in_place(res2, /*carry=*/0, m2, storage, factor_size);

err:
    if (storage != NULL) {
        OPENSSL_cleanse(storage, storage_len_bytes);
        OPENSSL_free(storage);
    }
    return ret;
}