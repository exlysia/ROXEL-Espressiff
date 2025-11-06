#ifndef _ROXEL_ENCODE_H_
#define _ROXEL_ENCODE_H_

#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define _ENC_X_AES_KEY_SIZE 16
#define _ENC_X_AES_BLOCK_SIZE 16

struct EncX
{
    const char *secretKey = nullptr;
    const char *iv = nullptr;
};

class roxel_encode
{
public:
    roxel_encode(const EncX &config = EncX());
    void derive_key(const char *key_str, uint8_t *key);
    char *aes_encrypt_base64(const char *input, const uint8_t *key = nullptr, const uint8_t *iv = nullptr);
    char *aes_decrypt_base64(const char *base64_input, const uint8_t *key = nullptr, const uint8_t *iv = nullptr);

private:
    uint8_t _key[_ENC_X_AES_KEY_SIZE];
    uint8_t _iv[_ENC_X_AES_BLOCK_SIZE];
};

#endif