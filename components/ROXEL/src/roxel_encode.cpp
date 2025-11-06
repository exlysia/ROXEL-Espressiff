#include "roxel_encode.h"

#include "mbedtls/aes.h"
#include "mbedtls/md5.h"
#include "mbedtls/base64.h"
#include "freertos/FreeRTOS.h"

roxel_encode::roxel_encode(const EncX &config)
{
    derive_key(config.secretKey, _key);
    derive_key(config.iv, _iv);
}

void roxel_encode::derive_key(const char *key_str, uint8_t *key)
{
    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts(&ctx);
    mbedtls_md5_update(&ctx, (const unsigned char *)key_str, strlen(key_str));
    mbedtls_md5_finish(&ctx, key);

    mbedtls_md5_free(&ctx);
}

char *roxel_encode::aes_encrypt_base64(const char *input, const uint8_t *key, const uint8_t *iv)
{
    size_t input_len = strlen(input);
    size_t padded_len = ((input_len / _ENC_X_AES_BLOCK_SIZE) + 1) * _ENC_X_AES_BLOCK_SIZE;
    uint8_t input_buf[padded_len];
    uint8_t output_buf[padded_len];
    uint8_t iv_copy[_ENC_X_AES_BLOCK_SIZE];
    memcpy(input_buf, input, input_len);
    memset(input_buf + input_len, padded_len - input_len, padded_len - input_len);
    memcpy(iv_copy, iv == nullptr ? _iv : iv, _ENC_X_AES_BLOCK_SIZE);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key == nullptr ? _key : key, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_len, iv_copy, input_buf, output_buf);
    mbedtls_aes_free(&aes);

    size_t base64_len = 0;
    char *base64_buf = (char *)pvPortMalloc(padded_len * 2);
    mbedtls_base64_encode((unsigned char *)base64_buf, padded_len * 2, &base64_len, output_buf, padded_len);
    base64_buf[base64_len] = '\0';
    return base64_buf;
}

char *roxel_encode::aes_decrypt_base64(const char *base64_input, const uint8_t *key, const uint8_t *iv)
{
    size_t ciphertext_len;
    uint8_t *ciphertext = (uint8_t *)pvPortMalloc(strlen(base64_input));
    mbedtls_base64_decode(ciphertext, strlen(base64_input), &ciphertext_len, (const unsigned char *)base64_input, strlen(base64_input));

    uint8_t output_buf[ciphertext_len];
    uint8_t iv_copy[_ENC_X_AES_BLOCK_SIZE];
    memcpy(iv_copy, iv == nullptr ? _iv : iv, _ENC_X_AES_BLOCK_SIZE);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key == nullptr ? _key : key, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, ciphertext_len, iv_copy, ciphertext, output_buf);
    mbedtls_aes_free(&aes);

    int padding = output_buf[ciphertext_len - 1];
    output_buf[ciphertext_len - padding] = '\0';

    vPortFree(ciphertext);
    return strdup((char *)output_buf);
}