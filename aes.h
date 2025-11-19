/*
 * @Author: lujie
 * @Date: 2022-11-01 15:11:48
 * @LastEditors: lujie
 * @LastEditTime: 2022-11-02 09:10:06
 * @FilePath: \update\aes.h
 * @Description: 
 * 
 * Copyright (c) 2022 by lujie/clever, All Rights Reserved. 
 */
#ifndef _AES_H
#define _AES_H

#include "stdafx.h"
#include <string.h>

	/****************************************************************************
	*  Include Files
	*****************************************************************************/


	/*****************************************************************************
	*  Define
	******************************************************************************/
	//以bit为单位的密钥长度，只能为 128，192 和 256 三种
#define AES_KEY_LENGTH	256

//加解密模式
#define AES_MODE_ECB	0				// 电子密码本模式
#define AES_MODE_CBC	1				// 密码分组链接模式
#define AES_MODE		AES_MODE_CBC	// 配置加密模式

/*****************************************************************************
*  Functions Define
******************************************************************************/

/******************************************************************************
//	函数名：	AES_Init_aa
//	描述：		初始化，在此执行扩展密钥操作。
//	输入参数：	pKey -- 原始密钥，其长度必须为 AES_KEY_LENGTH/8 字节。
//	输出参数：	无。
//	返回值：	无。
******************************************************************************/
	extern void aes_init(const void *pKey);

	/******************************************************************************
	//	函数名：	AES_Encrypt_aa
	//	描述：		加密数据
	//	输入参数：	pPlainText	-- 明文，即需加密的数据，其长度为nDataLen字节。
	//				nDataLen	-- 数据长度，以字节为单位，必须为AES_KEY_LENGTH/8的整倍数。
	//				pIV			-- 初始化向量，如果使用ECB模式，可设为NULL。
	//	输出参数：	pCipherText	-- 密文，即由明文加密后的数据，可以与pPlainText相同。
	//	返回值：	无。
	******************************************************************************/
	void aes_encrypt(const unsigned char *pPlainText, unsigned char *pCipherText,
		unsigned int nDataLen, const unsigned char *pIV);

	/******************************************************************************
	//	函数名：	AES_Decrypt_aa
	//	描述：		解密数据
	//	输入参数：	pCipherText -- 密文，即需解密的数据，其长度为nDataLen字节。
	//				nDataLen	-- 数据长度，以字节为单位，必须为AES_KEY_LENGTH/8的整倍数。
	//				pIV			-- 初始化向量，如果使用ECB模式，可设为NULL。
	//	输出参数：	pPlainText  -- 明文，即由密文解密后的数据，可以与pCipherText相同。
	//	返回值：	无。
	******************************************************************************/
	void aes_decrypt(unsigned char *pPlainText, const unsigned char *pCipherText,
		unsigned int nDataLen, const unsigned char *pIV);

	/*****************************************************************************
	//	函数名：	AES_add_pkcs7Padding_aa
	//	描述：		PKCS7Padding填充补齐
	//	输入参数：	input -- 后面最多预留16个字节空间用于存放填充值
	//				len --  数据的长度
	//	输出参数：	input  -- 添加填充码后的数据
	//	返回值：	填充后的长度
	*****************************************************************************/
	unsigned int aes_add_pkcs7Padding(unsigned char *input, unsigned int len);

	/*****************************************************************************
	//	函数名：	AES_delete_pkcs7Padding_aa
	//	描述：		PKCS7Padding填充密文解密后剔除填充值
	//	输入参数：	input -- 解密后的数据
	//				len --  数据的长度
	//	输出参数：	input  -- 删除填充码后的数据
	//	返回值：	删除后的实际有效数据长度，为0表示传入的数据异常
	*****************************************************************************/
	unsigned int aes_delete_pkcs7Padding(unsigned char *input, unsigned int len);


#endif  /* _AES_H */
