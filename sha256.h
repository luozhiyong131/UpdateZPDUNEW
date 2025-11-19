#ifndef __SHA256_H__
#define __SHA256_H__
#include <stdint.h>

#define SHA256_BLOCKLEN  64ul //size of message block buffer
#define SHA256_DIGESTLEN 32ul //size of digest in uint8_t
#define SHA256_DIGESTINT 8ul  //size of digest in uint32_t

void print_as_hex(const uint8_t *s,  const uint32_t slen);
void print_as_hex_temp(const uint8_t *s, const uint32_t slen, char* temp);
void compute_sha(const uint8_t *msg, uint32_t mlen, uint8_t *hash);
int Compute_file_sha256(const char *filename,  unsigned char *hash);
void compute_hmac(const uint8_t *key, uint32_t klen, const uint8_t *msg, uint32_t mlen, uint8_t *hash);
void compute_pbkdf2(const uint8_t *key, uint32_t klen, const uint8_t *salt, uint32_t slen,uint32_t rounds, uint32_t dklen, uint8_t *hash);

#endif
