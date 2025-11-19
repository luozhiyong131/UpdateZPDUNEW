#include "stdafx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PBKDF2_SHA256_STATIC
#define PBKDF2_SHA256_IMPLEMENTATION
#include "pbkdf2_sha256.h"

void print_as_hex(const uint8_t *s,  const uint32_t slen)
{
	for (uint32_t i = 0; i < slen; i++)
	{
		printf("%02X", s[ i ]);
	}
	printf("\n");
}

void print_as_hex_temp(const uint8_t *s, const uint32_t slen , char* temp)
{
	for (uint32_t i = 0; i < slen; i++)
	{
		sprintf(temp,"%02X", s[i]);
		temp += 2;
	}

}

void compute_sha(const uint8_t *msg, uint32_t mlen, uint8_t *hash)
{
	SHA256_CTX sha;
	sha256_init(&sha);
	sha256_update(&sha, msg, mlen);
	sha256_final(&sha, hash);
}

int Compute_file_sha256(const char *filename,  unsigned char *hash)
{
    int ret;
    FILE *fp = fopen(filename, "rb");
    if(!fp)
    {
        perror("fopen");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    rewind(fp);

    unsigned char *c = (unsigned char*)malloc(size);
    if(!c)
    {
        perror("malloc");
        return -1;
    }
    memset(c, 0, size);

    if(fread(c, size, 1, fp) != 1)
    {
        perror("fread");
        free(c);
        return -1;
    }
    fclose(fp);

	compute_sha(c, size, hash);

	free(c);
    return 0;
}

void compute_hmac(const uint8_t *key, uint32_t klen, const uint8_t *msg, uint32_t mlen, uint8_t *hash)
{
	HMAC_SHA256_CTX hmac;
	hmac_sha256_init(&hmac, key, klen);
	hmac_sha256_update(&hmac, msg, mlen);
	hmac_sha256_final(&hmac, hash);
}

void compute_pbkdf2(const uint8_t *key, uint32_t klen, const uint8_t *salt, uint32_t slen,
    uint32_t rounds, uint32_t dklen, uint8_t *hash)
{
	HMAC_SHA256_CTX pbkdf_hmac;
	pbkdf2_sha256(&pbkdf_hmac, key, klen, salt, slen, rounds, hash, dklen);
}