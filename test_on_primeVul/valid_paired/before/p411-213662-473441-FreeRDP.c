static int crypto_rsa_common(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus,
                             const BYTE* exponent, int exponent_size, BYTE* output)
{
	BN_CTX* ctx;
	int output_length = -1;
	BYTE* input_reverse;
	BYTE* modulus_reverse;
	BYTE* exponent_reverse;
	BIGNUM *mod, *exp, *x, *y;
	input_reverse = (BYTE*)malloc(2 * key_length + exponent_size);

	if (!input_reverse)
		return -1;

	modulus_reverse = input_reverse + key_length;
	exponent_reverse = modulus_reverse + key_length;
	memcpy(modulus_reverse, modulus, key_length);
	crypto_reverse(modulus_reverse, key_length);
	memcpy(exponent_reverse, exponent, exponent_size);
	crypto_reverse(exponent_reverse, exponent_size);
	memcpy(input_reverse, input, length);
	crypto_reverse(input_reverse, length);

	if (!(ctx = BN_CTX_new()))
		goto fail_bn_ctx;

	if (!(mod = BN_new()))
		goto fail_bn_mod;

	if (!(exp = BN_new()))
		goto fail_bn_exp;

	if (!(x = BN_new()))
		goto fail_bn_x;

	if (!(y = BN_new()))
		goto fail_bn_y;

	BN_bin2bn(modulus_reverse, key_length, mod);
	BN_bin2bn(exponent_reverse, exponent_size, exp);
	BN_bin2bn(input_reverse, length, x);
	BN_mod_exp(y, x, exp, mod, ctx);
	output_length = BN_bn2bin(y, output);
	crypto_reverse(output, output_length);

	if (output_length < (int)key_length)
		memset(output + output_length, 0, key_length - output_length);

	BN_free(y);
fail_bn_y:
	BN_clear_free(x);
fail_bn_x:
	BN_free(exp);
fail_bn_exp:
	BN_free(mod);
fail_bn_mod:
	BN_CTX_free(ctx);
fail_bn_ctx:
	free(input_reverse);
	return output_length;
}