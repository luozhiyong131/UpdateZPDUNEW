/*
 * @Author: lujie
 * @Date: 2022-10-28 14:50:08
 * @LastEditors: lujie
 * @LastEditTime: 2022-11-02 15:10:18
 * @FilePath: \update\common.h
 * @Description: 
 * 
 * Copyright (c) 2022 by lujie/clever, All Rights Reserved. 
 */
#ifndef __COMMON__
#define __COMMON__

#pragma warning(disable:4996)
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
//#include <unistd.h>
//#include <pthread.h>
//#include <windows.h>

#include "md5.h"
//#include "dyad.h"
#include "cJSON.h"
#include "aes.h"



static int aes_encrypt_buff(unsigned char *data, unsigned int len, unsigned char **cipher)
{
    *cipher = (unsigned char*)malloc((len/32+1)*32);
    if(!cipher)
    {
        perror("malloc");
        return -1;
    }
    int ret = aes_add_pkcs7Padding(data, len);
    unsigned char iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
    aes_encrypt(data, *cipher, ret, iv);
    return ret;
}

static int aes_decrypt_buff(unsigned char **data, unsigned int len, unsigned char *cipher)
{
    unsigned char iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

    *data = (unsigned char*)malloc(len+1);
    if(!data)
    {
        perror("malloc");
        return -1;
    }

    aes_decrypt(*data, cipher, len, iv);
    int ret = aes_delete_pkcs7Padding(*data, len);
    (*data)[ret] = '\0';
    return ret;
}

static int aes_encrypt_str(unsigned char *data, unsigned int len, unsigned char *cipher)
{
    int ret = aes_add_pkcs7Padding(data, len);
    unsigned char iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
    aes_encrypt(data, cipher, ret, iv);

    return ret;
}

static int aes_encrypt_fp(FILE *fp, unsigned char *cipher)
{
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    rewind(fp);

    //unsigned char *c = (unsigned char*)malloc((size/32+1)*32);
    //if(!c)
    //{
    //    perror("malloc");
    //    return -1;
    //}

    if(fread(cipher, size, 1, fp) != 1)
    {
        perror("fread");
        //free(c);
        return -1;
    }
    
    int ret = aes_encrypt_str(cipher, size, cipher);

    //*cipher = c;
    
    return ret;
}

static int aes_decrypt_str(unsigned char *data, unsigned int len, unsigned char *cipher)
{
    unsigned char iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

    aes_decrypt(data, cipher, len, iv);
    return aes_delete_pkcs7Padding(data, len);
}

static int aes_decrypt_fp(FILE *fp,  unsigned char **clear)
{
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    rewind(fp);

    unsigned char *c = (unsigned char*)malloc(size);
    if(!c)
    {
        perror("malloc");
        return -1;
    }

    if(fread(c, size, 1, fp) != 1)
    {
        perror("fread");
        free(c);
        return -1;
    }

    int ret = aes_decrypt_str(c, size, c);
    

    *clear = c;
    
    return ret;
}

static int aes_encrypt_file(const char *filename, unsigned char *cipher)
{
    int ret;
    FILE *fp = fopen(filename, "rb");
    if(!fp)
    {
        perror("fopen");
        return -1;
    }

    ret = aes_encrypt_fp(fp, cipher);
    fclose(fp);

    return ret;
}

static int aes_decrypt_file(const char *filename,  unsigned char **clear)
{
    int ret;
    FILE *fp = fopen(filename, "rb");
    if(!fp)
    {
        perror("fopen");
        return -1;
    }

    ret = aes_decrypt_fp(fp, clear);
    fclose(fp);

    return ret;
}

#endif