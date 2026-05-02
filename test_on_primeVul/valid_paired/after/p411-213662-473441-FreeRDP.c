static int crypto_rsa_common(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus,
                             const BYTE* exponent, int exponent_size, BYTE* output)
{
	BN_CTX* ctx = NULL;
	int output_length = -1;
	BYTE* input_reverse = NULL;
	BYTE* modulus_reverse = NULL;
	BYTE* exponent_reverse = NULL;
	BIGNUM* mod = NULL;
	BIGNUM* exp = NULL;
	BIGNUM* x = NULL;
	BIGNUM* y = NULL;
	size_t bufferSize = 2 * key_length + exponent_size;

	if (!input || (length < 0) || (exponent_size < 0) || !modulus || !exponent || !output)
		return -1;

	if (length > bufferSize)
		bufferSize = length;

	input_reverse = (BYTE*)calloc(bufferSize, 1);

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

	if (!BN_bin2bn(modulus_reverse, key_length, mod))
		goto fail;

	if (!BN_bin2bn(exponent_reverse, exponent_size, exp))
		goto fail;
	if (!BN_bin2bn(input_reverse, length, x))
		goto fail;
	if (BN_mod_exp(y, x, exp, mod, ctx) != 1)
		goto fail;
	output_length = BN_bn2bin(y, output);
	if (output_length < 0)
		goto fail;
	crypto_reverse(output, output_length);

	if (output_length < key_length)
		memset(output + output_length, 0, key_length - output_length);

fail:
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