#include "ftp.h"

static char encoding_table[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/' };
static unsigned char decoding_table[256];
static int mod_table[] = { 0, 2, 1 };
static char SALT[] = "microftpVolgaCTF";
static char admin_hash[] = "2aeab3a7d4946b0e6748f1ca6f4e23d7cc9f3bf27eecd2b9ef1a8d223b04d4feffd7bdf572ce7d1abeef6ab664e52faec5e182d638b4d8f0c9bdd8ae3501b41c";

ELGAMAL_params elgamal_params;

bool base64_encode(const unsigned char *input_data, unsigned char *output_buffer, int input_length, int output_buffer_length)
{
	if ((!input_data) || (!output_buffer))
		return false;

	int output_length = 4 * ((input_length + 2) / 3);
	if (output_length >= output_buffer_length)
		return false;
	memset(output_buffer, 0, output_buffer_length);

	for (unsigned int i = 0, j = 0; i < input_length;)
	{
		uint32_t octet_a = i < input_length ? (unsigned char)input_data[i++] : 0;
		uint32_t octet_b = i < input_length ? (unsigned char)input_data[i++] : 0;
		uint32_t octet_c = i < input_length ? (unsigned char)input_data[i++] : 0;

		uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

		output_buffer[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
		output_buffer[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
		output_buffer[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
		output_buffer[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
	}

	for (int i = 0; i < mod_table[input_length % 3]; i++)
		output_buffer[output_length - 1 - i] = '=';
	return true;
}

void base64_init()
{
	for (int i = 0; i < 64; i++)
		decoding_table[(unsigned char)encoding_table[i]] = i;
}

bool base64_decode(const unsigned char *input_data, unsigned char *output_buffer, int output_buffer_length, int* output_length)
{
	if ((!input_data) || (!output_buffer) || (!output_length))
		return false;
	int input_length = strlen(input_data);
	if (input_length % 4)
		return false;

	*output_length = (input_length / 4) * 3;
	if (*output_length >= output_buffer_length)
		return false;
	memset(output_buffer, 0, output_buffer_length);

	if (input_data[input_length - 1] == '=')
		(*output_length)--;
	if (input_data[input_length - 2] == '=')
		(*output_length)--;

	for (unsigned int i = 0, j = 0; i < input_length;)
	{
		uint32_t sextet_a = input_data[i] == '=' ? 0 & i++ : decoding_table[input_data[i++]];
		uint32_t sextet_b = input_data[i] == '=' ? 0 & i++ : decoding_table[input_data[i++]];
		uint32_t sextet_c = input_data[i] == '=' ? 0 & i++ : decoding_table[input_data[i++]];
		uint32_t sextet_d = input_data[i] == '=' ? 0 & i++ : decoding_table[input_data[i++]];

		uint32_t triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6) + (sextet_c << 1 * 6) + (sextet_d << 0 * 6);

		if (j < *output_length) output_buffer[j++] = (triple >> 2 * 8) & 0xFF;
		if (j < *output_length) output_buffer[j++] = (triple >> 1 * 8) & 0xFF;
		if (j < *output_length) output_buffer[j++] = (triple >> 0 * 8) & 0xFF;
	}

	return true;
}

bool check_acct(char* message_with_path, char* sign)
{
	char* message = basename(message_with_path);
	char hash[SHA512_DIGEST_LENGTH];
	char hash_hex[SHA512_DIGEST_LENGTH * 2 + 1];
	memset(hash, 0, SHA512_DIGEST_LENGTH);
	SHA512_CTX sha_ctx;
	SHA512_Init(&sha_ctx);
	SHA512_Update(&sha_ctx, message, strlen(message));
	SHA512_Final(hash, &sha_ctx);
	for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
		sprintf(&hash_hex[i * 2], "%02x", (unsigned char)hash[i]);

	BN_CTX *ctx = BN_CTX_new();
	BIGNUM * h = BN_new();
	BIGNUM * r = BN_new();
	BIGNUM * s = BN_new();
	if (!BN_hex2bn(&r, (const char *)hash_hex))
		return false;
	BN_nnmod(h, r, elgamal_params.modulus_p, ctx);

	char* result = strtok(sign, HASHDELIM);
	if (!result)
		return false;
	if (!BN_hex2bn(&r, result))
		return false;

	result = strtok(NULL, HASHDELIM);
	if (!result)
		return false;
	BN_hex2bn(&s, result);

	BIGNUM *v1 = BN_new();
	BIGNUM *v2 = BN_new();
	BIGNUM *t1 = BN_new();
	BIGNUM *t2 = BN_new();
	bool ret_value = (BN_mod_exp(v1, elgamal_params.generator_g, h, elgamal_params.modulus_p, ctx) && BN_mod_exp(t1, r, s, elgamal_params.modulus_p, ctx) &&
		BN_mod_exp(t2, elgamal_params.y, r, elgamal_params.modulus_p, ctx) && BN_mod_mul(v2, t1, t2, elgamal_params.modulus_p, ctx) && !BN_cmp(v1, v2));

	BN_clear_free(t2);
	BN_clear_free(t1);
	BN_clear_free(v2);
	BN_clear_free(v1);
	BN_clear_free(h);
	BN_clear_free(r);
	BN_clear_free(s);
	BN_CTX_free(ctx);
	return ret_value;
}

bool generate_passphrase(char* data, char* passphrase)
{
	char hash[SHA512_DIGEST_LENGTH];
	char hash_hex[SHA512_DIGEST_LENGTH * 2 + 1];
	memset(hash, 0, SHA512_DIGEST_LENGTH);
	SHA512_CTX sha_ctx;
	SHA512_Init(&sha_ctx);
	SHA512_Update(&sha_ctx, data, strlen(data));
	SHA512_Final(hash, &sha_ctx);
	for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
		sprintf(&hash_hex[i * 2], "%02x", (unsigned char)hash[i]);
	BIGNUM * h = BN_new();
	BIGNUM * gcd = BN_new();
	if (!BN_hex2bn(&gcd, (const char *)hash_hex))
		return false;
	BIGNUM * range = BN_dup(elgamal_params.modulus_p);
	BN_sub_word(range, 4);
	BIGNUM * temp = BN_dup(elgamal_params.modulus_p);
	BN_sub_word(temp, 1);
	BN_CTX *ctx = BN_CTX_new();
	BN_nnmod(h, gcd, elgamal_params.modulus_p, ctx);

	BIGNUM * session_key = BN_new();
	BN_zero(gcd);	
	while (!BN_is_one(gcd))
	{
		BN_rand_range(session_key, range);
		BN_add_word(session_key, 2);
		BN_gcd(gcd, session_key, temp, ctx);
	}

	BIGNUM * r = BN_new();
	BN_mod_exp(r, elgamal_params.generator_g, session_key, elgamal_params.modulus_p, ctx);

	BIGNUM * s = BN_new();
	BN_mod_mul(range, r, elgamal_params.secret_x, temp, ctx);
	BN_mod_sub(gcd, h, range, temp, ctx);
	BN_mod_inverse(range, session_key, temp, ctx);
	BN_mod_mul(s, gcd, range, temp, ctx);

	memset(passphrase, 0, PASSPHRASE_SIZE);
	char* hex_str = BN_bn2hex(r);
	strcpy(passphrase, hex_str);
	OPENSSL_free(hex_str);

	strcat(passphrase, ":");

	hex_str = BN_bn2hex(s);
	strcat(passphrase, hex_str);
	OPENSSL_free(hex_str);

	BN_clear_free(h);
	BN_clear_free(range);
	BN_clear_free(temp);
	BN_clear_free(gcd);
	BN_clear_free(session_key);
	BN_clear_free(r);
	BN_clear_free(s);
	BN_CTX_free(ctx);
	return true;
}

void print_help_message(int sock)
{
	//We change the position of new line and carriage return
	send_repl_control(sock, "	This is a tiny and nonstandart FTPserver.\n\r");
	send_repl_control(sock, "	Known FTPclients won't help you. - Go and write your own.\n\r");
	send_repl_control(sock, "	See rfc959 for some help\n\r");
	send_repl_control(sock, "	Begin server public key:\n\r");
	char key[PASSPHRASE_SIZE];
	BIGNUM* public_key[] = { elgamal_params.modulus_p, elgamal_params.generator_g, elgamal_params.y, NULL };
	BIGNUM** target = &public_key[0];
	while (*target)
	{
		memset(key, 0, PASSPHRASE_SIZE);
		key[0] = '	';
		key[1] = '	';
		char* hex_str = BN_bn2hex(*target);
		strcpy(key + 2, hex_str);
		strcat(key, "\n\r");
		OPENSSL_free(hex_str);
		send_repl_control(sock, key);
		target++;
	}
	send_repl_control(sock, "	End server public key.\n\r");
}

bool generate_key()
{
	RAND_load_file("/dev/urandom", SEED_LENGTH);

	//first we are going to generate modulus
	elgamal_params.modulus_p = BN_new();
	int bitlen = 0;
	while (bitlen != PRIME_LENGTH)
	{
		BN_generate_prime_ex(elgamal_params.modulus_p, PRIME_LENGTH, 1, NULL, NULL, NULL);
		bitlen = BN_num_bits(elgamal_params.modulus_p);
	}

	// g is a generator if g^2 and g^q are both different from 1, but g^2q = 1 (q = (p-1)\2)
	elgamal_params.generator_g = BN_new();
	BIGNUM* p_minus_1 = BN_dup(elgamal_params.modulus_p);
	BN_sub_word(p_minus_1, 1);
	BIGNUM* q = BN_dup(p_minus_1);
	BN_div_word(q, 2);
	bool flag = false;
	BIGNUM* temp = BN_new();
	BN_CTX *ctx = BN_CTX_new();
	while (!flag)
	{
		flag = true;
		BN_rand_range(elgamal_params.generator_g, elgamal_params.modulus_p);
		BN_mod_sqr(temp, elgamal_params.generator_g, elgamal_params.modulus_p, ctx);
		flag &= !BN_is_one(temp);
		BN_mod_exp(temp, elgamal_params.generator_g, q, elgamal_params.modulus_p, ctx);
		flag &= !BN_is_one(temp);
		BN_mod_exp(temp, elgamal_params.generator_g, p_minus_1, elgamal_params.modulus_p, ctx);
		flag &= BN_is_one(temp);
	}
	
	//x is between [2, p-2]
	elgamal_params.secret_x = BN_new();
	BIGNUM* p_minus_3 = BN_dup(elgamal_params.modulus_p);
	BN_sub_word(p_minus_3, 3);
	BN_rand_range(elgamal_params.secret_x, p_minus_3);
	BN_add_word(elgamal_params.secret_x, 2);

	//y = g^x
	elgamal_params.y = BN_new();
	BN_mod_exp(elgamal_params.y, elgamal_params.generator_g, elgamal_params.secret_x, elgamal_params.modulus_p, ctx);

	BN_clear_free(p_minus_1);
	BN_clear_free(q);
	BN_clear_free(temp);
	BN_clear_free(p_minus_3);
	BN_CTX_free(ctx);
	return true;
}

bool check_if_admin(char* pass)
{
	char salt_buf[SCRYPT_SALT_LEN + 1];
	char hash[SCRYPT_HASH_LEN + 1];
	strcpy(salt_buf, SALT);
	if (libscrypt_scrypt(pass, strlen(pass), salt_buf, SCRYPT_SALT_LEN, SCRYPT_N, SCRYPT_r, 1, hash, SCRYPT_HASH_LEN))
		return false;
	char hash_hex[2 * SCRYPT_HASH_LEN + 1];
	for (int i = 0; i < SCRYPT_HASH_LEN; i++)
		sprintf(&hash_hex[i * 2], "%02x", (unsigned char)hash[i]);
	printf("hash: %s\n", hash_hex);
	return (!strcmp(hash_hex, admin_hash));
}
