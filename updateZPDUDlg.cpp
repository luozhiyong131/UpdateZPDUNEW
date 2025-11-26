
// updateZPDUDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "updateZPDU.h"
#include "updateZPDUDlg.h"
#include "afxdialogex.h"
#include "common.h"
#include "sha256.h"
#include "ping.h"
//#include <WS2tcpip.h> // InetPton
#include<vector>
#include<string>
#include <sstream>
#include <IPHlpApi.h>
#pragma comment(lib,"IPHlpApi.lib")


#define WM_MY_MESSAGE   ( WM_USER + 0x100)
#define WM_MY_START_TIME_MESSAGE   ( WM_USER + 0x101)
#define WM_MY_PROGESS_MESSAGE   ( WM_USER + 0x102)
#define WM_UPDATE_EDIT (WM_USER + 0x103)
#define WM_UPDATE_ERROR_EDIT (WM_USER + 0x105)

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define TEST_FILE "/tmp/update.tar.bz2"
#define UP_FILE "/tmp/update.tar.bz2"
//#define KILLALL_APP "#echo Performance > /sys/bus/cpu/devices/cpu0/cpufreq/scaling_governor;touch /tmp/.update_now;killall master order web alarm chart sensor timing  modbus modbus_tcp releasespace snmpd screen sshd"
#define KILLALL_APP "echo Performance > /sys/bus/cpu/devices/cpu0/cpufreq/scaling_governor;touch /tmp/.update_now;killall master order web alarm chart sensor timing  modbus modbus_tcp releasespace snmpd screen"
//#define UPDATE "#busybox rm -rf /tmp/update;tar -jxf /tmp/update.tar.bz2 -C /tmp;cd /tmp/update;./busybox rm /tmp/update.tar.bz2;./busybox chmod 777 update;./busybox sh update -cz"
#define UPDATE "busybox rm -rf /tmp/update;tar -jxf /tmp/update.tar.bz2 -C /tmp;cd /tmp/update;./busybox rm /tmp/update.tar.bz2;./busybox chmod 777 update;./busybox sh update -zukdr;./busybox sync;"
#define SERVER_IP "192.168.10.240" 
#define AES_KEY "zpduadminadmin"
CString gFilePath;
char gStartIp[255];
char gEndIp[255];
std::vector<CString> gVecIP;
int gIndex;
HANDLE gMainThreads;
CString gName;
CString gPassword;
HWND hText;
CProgressCtrl* g_Prog;
CProgressCtrl* g_TotalProg;
HWND gHwnd;
int size;
char* gbuff;
char* gdata;

typedef struct client_info
{
	char name[1024]; //文件名
	char update[1024]; //升级命令
	char pre_update[1024]; //预升级命令
	char update_file[1024]; //保存文件名
	FILE *file;
	int size;
	char md5[64]; //校验码
	char hash[257]; //校验码
	unsigned char aes_cbc_key[64];
	unsigned char cipher[30*1024*1024];
	//dyad_Event *e;
	SOCKET ctrl_sock;
	SOCKET file_sock;
} client_info;

client_info g_client_info;

// CupdateZPDUDlg 对话框

static int open_ctrl_sock(client_info* info)
{
	info->ctrl_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (info->ctrl_sock == INVALID_SOCKET)
	{
		printf("ctrl sock,%d", WSAGetLastError());
		printf("%s %d exit\n", __func__, __LINE__);
		//exit(-1);
		CString msg;
		msg.Format(_T("ctrl sock error!!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE , (WPARAM)0 , (LPARAM)0);
		return -1;
	}

	SOCKADDR_IN addrSrv;
	addrSrv.sin_family = AF_INET;
	char buf[255];
	strncpy_s(buf, CT2A(gVecIP[gIndex]), sizeof(buf));
	addrSrv.sin_addr.s_addr = inet_addr(buf);
	addrSrv.sin_port = htons(30964);

	if (connect(info->ctrl_sock, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == INVALID_SOCKET)
	{
		printf("connect error,%d", WSAGetLastError());
		printf("%s %d exit\n", __func__, __LINE__);
		CString msg;
		msg.Format(_T("connect error!!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg+ _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	return 0;
}

static int open_file_sock(client_info* info)
{
	info->file_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (info->file_sock == INVALID_SOCKET)
	{
		printf("file sock, %d", WSAGetLastError());
		printf("%s %d exit\n", __func__, __LINE__);
		//exit(-1);
		SetWindowTextA(hText, "file sock init error !!!");

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}

	SOCKADDR_IN addrSrv;
	addrSrv.sin_family = AF_INET;
	char buf[255];
	strncpy_s(buf, CT2A(gVecIP[gIndex]), sizeof(buf));
	addrSrv.sin_addr.s_addr = inet_addr(buf);
	addrSrv.sin_port = htons(30965);

	if (connect(info->file_sock, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == INVALID_SOCKET)
	{
		printf("connect error,%d\n", WSAGetLastError());
		printf("%s %d exit\n", __func__, __LINE__);
		//exit(-1);
		CString msg;
		msg.Format(_T("connect error!!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	return 0;
}

static int read_file(FILE *fp, unsigned char *cipher)
{
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	rewind(fp);

	if (fread(cipher, size, 1, fp) != 1)
	{
		perror("fread");
		//free(c);
		return -1;
	}

	return size;
}

static int encrypt(client_info* info)
{
	int ret = aes_encrypt_fp(info->file, info->cipher);
	//int ret = read_file(info->file, info->cipher);
	if (ret < 0)
	{
		printf("%s aes_encrypt_fp error\n", __func__);
		printf("%s %d exit\n", __func__, __LINE__);
		//exit(-1);
		CString msg;
		msg.Format(_T("aes_encrypt_fp error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}

	info->size = ret;
	uint8_t temp[SHA256_DIGESTLEN];
	compute_sha(info->cipher, ret, temp);
	print_as_hex_temp(temp, sizeof(temp), (char*)info->hash);

	return 0;
}

static int get_file(client_info* info)
{
	//strcpy(info->pre_update, KILLALL_APP);
	//strcpy(info->pre_update, "1");
	

	strcpy(info->pre_update, KILLALL_APP);
	strcpy(info->update, UPDATE);
	strcpy(info->update_file, UP_FILE);
	//strcpy(info->name, TEST_FILE);
	const char* sstr;
	char temp[1024];
	memset(temp, 0, sizeof(char) * 1024);
	::wsprintfA(temp, "%ls", (LPCTSTR)gFilePath);
	sstr = temp;
	strcpy(info->name, sstr);
	info->file = fopen(info->name, "rb");
	if (!info->file)
	{
		//perror(TEST_FILE);
		printf("%s %d exit\n", __func__, __LINE__);
		//exit(-1);
		CString msg;
		msg.Format(_T("fopen file error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	encrypt(info);
	printf("file info name:%s size:%d hash:%s\n", info->name, info->size, info->hash);
	return 0;
}

static int say_hello(client_info* info)
{
	cJSON* obj = cJSON_CreateObject();
	if (obj == NULL)
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("cJSON_CreateObject error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	//printf("%s %d\n", __func__, __LINE__);
	cJSON* func = NULL;
	cJSON* pre_update = NULL;
	cJSON* update = NULL;
	cJSON* name = NULL;
	cJSON* size = NULL;
	cJSON* hash = NULL;
	//cJSON* md5 = NULL;

	if (!(func = cJSON_CreateNumber(0)))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("cJSON_CreateNumber error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	if (!(pre_update = cJSON_CreateString(info->pre_update)))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("cJSON_CreateString error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	if (!(update = cJSON_CreateString(info->update)))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("cJSON_CreateString error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	if (!(name = cJSON_CreateString(info->update_file)))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("cJSON_CreateString error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	if (!(size = cJSON_CreateNumber(info->size)))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("cJSON_CreateString error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	if (!(hash = cJSON_CreateString((char*)info->hash)))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("cJSON_CreateString error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	printf("%s %d\n", __func__, __LINE__);
	if (!cJSON_AddItemToObject(obj, "func", func))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("func error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	if (!cJSON_AddItemToObject(obj, "pre_update", pre_update))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("pre_update error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255,0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	if (!cJSON_AddItemToObject(obj, "update", update))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("update error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	if (!cJSON_AddItemToObject(obj, "name", name))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("name error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	if (!cJSON_AddItemToObject(obj, "size", size))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("size error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	if (!cJSON_AddItemToObject(obj, "hash", hash))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("hash error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	//if (!cJSON_AddItemToObject(obj, "md5", md5))
	//{
	//	printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
	//	//exit(-1);
	//	SetWindowTextA(hText, "md5 error !!!");
	//	g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
	//	::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
	//	return -1;
	//}
	char* data = cJSON_Print(obj);
	printf("%s\n", data);
	unsigned char* c;
	int ret = aes_encrypt_buff((unsigned char* )data, strlen(data), &c);
	if (send(info->ctrl_sock,(const char*) c, ret, MSG_OOB) == SOCKET_ERROR)
	{
		perror("say hello error");
		printf("%s %d exit\n", __func__, __LINE__);
		//exit(-1);
		CString msg;
		msg.Format(_T("say hello error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	cJSON_free(data);
	cJSON_Delete(obj);
	free(c);
	return 0;
}


static int start_trans(client_info* info)
{
	printf("%s\n", __func__);

	open_file_sock(info);
	if (send(info->file_sock, (const char*)info->cipher, info->size, MSG_OOB) == SOCKET_ERROR)
	{
		printf("send file error\n");
		printf("%s %d exit\n", __func__, __LINE__);
		//exit(-1);
		CString msg;
		msg.Format(_T("send file error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	return 0;
}

static int update_now(client_info* info)
{
	cJSON* func = NULL;

	cJSON* obj = cJSON_CreateObject();
	if (obj == NULL)
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		CString msg;
		msg.Format(_T("func json error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		//exit(-1);
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}

	if (!(func = cJSON_CreateNumber(2)))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("json error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}

	if (!cJSON_AddItemToObject(obj, "func", func))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("func json error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}


	char* data = cJSON_Print(obj);
	//printf("%s\n", data);
	unsigned char* c;
	int ret = aes_encrypt_buff((unsigned char*)data, strlen(data), &c);
	if (send(info->ctrl_sock, (const char*)c, ret, MSG_OOB) == SOCKET_ERROR)
	{
		printf("send update error\n");
		printf("%s %d exit\n", __func__, __LINE__);
		//exit(-1);
		CString msg;
		msg.Format(_T("send update error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	cJSON_free(data);
	cJSON_Delete(obj);
	free(c);
	return 0;
}

static int check_hash(client_info *info)
{

	cJSON* func = NULL;

	cJSON* obj = cJSON_CreateObject();
	if (obj == NULL)
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("json error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}

	if (!(func = cJSON_CreateNumber(1)))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("json error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}

	if (!cJSON_AddItemToObject(obj, "func", func))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("func json error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return - 1;
	}


	char* data = cJSON_Print(obj);
	//printf("%s\n", data);
	unsigned char* c;
	int ret = aes_encrypt_buff((unsigned char*)data, strlen(data), &c);
	if (send(info->ctrl_sock, (const char*)c, ret, MSG_OOB) == SOCKET_ERROR)
	{
		printf("send get hash error\n");
		printf("%s %d exit\n", __func__, __LINE__);
		//exit(-1);
		CString msg;
		msg.Format(_T("send get hash error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}

	cJSON_free(data);
	cJSON_Delete(obj);
	free(c);
	return 0;
}

static int check_md5(client_info* info)
{

	cJSON* func = NULL;

	cJSON* obj = cJSON_CreateObject();
	if (obj == NULL)
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("json error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}

	if (!(func = cJSON_CreateNumber(1)))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("json error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}

	if (!cJSON_AddItemToObject(obj, "func", func))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("func json error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}


	char* data = cJSON_Print(obj);
	printf("%s\n", data);
	unsigned char* c;
	int ret = aes_encrypt_buff((unsigned char*)data, strlen(data), &c);
	if (send(info->ctrl_sock, (const char*)c, ret, MSG_OOB) == SOCKET_ERROR)
	{
		printf("send get md5 error\n");
		printf("%s %d exit\n", __func__, __LINE__);
		//exit(-1);
		CString msg;
		msg.Format(_T("send get md5 error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}

	cJSON_free(data);
	cJSON_Delete(obj);
	free(c);

}

static int recv_data_(client_info* info)
{
	gbuff = (char*)malloc(1024 * 1024);
	if (!gbuff)
	{
		printf("no mem\n");
		printf("%s %d exit\n", __func__, __LINE__);
		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	memset(gbuff, 0, sizeof(gbuff));

	int ret = recv(info->ctrl_sock, gbuff, 1024 * 1024, 0);
	if (!ret)
	{
		printf("Disconnect\n");//网络错误
		printf("%s %d exit\n", __func__, __LINE__);
		CString msg;
		msg.Format(_T("Disconnect !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		return -1;
	}
	else if (ret == SOCKET_ERROR)
	{
		//printf("recv() fail:%d\n", WSAGetLastError());
		//exit(-1);//网络错误
		CString msg;
		msg.Format(_T("network error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		return -1;
	}
	else
	{
		char oob = recv(info->ctrl_sock, gbuff +ret, 1, MSG_OOB);
		ret++;
		gbuff[ret] = '\0';
		printf("%s\n", gbuff);
	}

	if (aes_decrypt_buff((unsigned char**)&gdata, ret, (unsigned char*)gbuff) <= 0)
	{
		printf("%s aes_decrypt_buff error\n", __func__);
		gdata = gbuff;
		gdata[ret] = '\0';
	}

	cJSON* obj = cJSON_Parse(gdata);
	if (!obj)
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("json error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		if (gbuff) {
			free(gbuff);
			gbuff = NULL;
		}
		cJSON_Delete(obj);
		return -1;
	}

	cJSON* func = cJSON_GetObjectItem(obj, "func");
	if (!func)
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("func json error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		if (gbuff) {
			free(gbuff);
			gbuff = NULL;
		}
		cJSON_Delete(obj);
		return -1;
	}
	cJSON* status = cJSON_GetObjectItem(obj, "status");
	if (!status)
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("status json error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		if (gbuff) {
			free(gbuff);
			gbuff = NULL;
		}
		cJSON_Delete(obj);
		return -1;
	}

	if (!cJSON_IsNumber(func) || !cJSON_IsNumber(status))
	{
		printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
		//exit(-1);
		CString msg;
		msg.Format(_T("json error !!! IP: %s"), gVecIP[gIndex]);
		CT2A pszA(msg);   // 转成 ANSI
		LPCSTR pStr = pszA;
		SetWindowTextA(hText, pStr);

		CString* str = new CString(msg + _T("\r\n"));
		::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

		g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
		::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		if (gbuff) {
			free(gbuff);
			gbuff = NULL;
		}
		cJSON_Delete(obj);
		return -1;
	}

	switch ((int)cJSON_GetNumberValue(func))
	{
	case 0:
		if ((int)cJSON_GetNumberValue(status))
		{
			start_trans(info);
			check_hash(info);
		}
		else {
			printf("hello error\n");//握手失败
			CString msg;
			msg.Format(_T("hello error !!! IP: %s"), gVecIP[gIndex]);
			CT2A pszA(msg);   // 转成 ANSI
			LPCSTR pStr = pszA;
			SetWindowTextA(hText, pStr);

			CString* str = new CString(msg + _T("\r\n"));
			::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

			g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
			::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		}
		break;
	case 1:
		if ((int)cJSON_GetNumberValue(status) == 1)
		{
			update_now(info);
			closesocket(info->file_sock);
			printf("update now\n");
			CString msg;
			msg.Format(_T("update now !!! IP: %s"), gVecIP[gIndex]);
			CT2A pszA(msg);   // 转成 ANSI
			LPCSTR pStr = pszA;
			SetWindowTextA(hText, pStr);

			CString* str = new CString(msg + _T("\r\n"));
			::PostMessage(gHwnd, WM_UPDATE_EDIT, 0, (LPARAM)str);

		}
		else if ((int)cJSON_GetNumberValue(status) == -1) {
			CString msg;
			msg.Format(_T("update error !!! IP: %s"), gVecIP[gIndex]);
			CT2A pszA(msg);   // 转成 ANSI
			LPCSTR pStr = pszA;
			SetWindowTextA(hText, pStr);//过程

			CString* str = new CString(msg + _T("\r\n"));
			::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

			g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
			::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
		}
		else
		{
			Sleep(1000);
			check_hash(info);
		}
		break;
	case 2:
		{
			if ((int)cJSON_GetNumberValue(status))
			{
				printf("update ok\n");
			}
			printf("%s %d exit\n", __func__, __LINE__);
			CString msg;
			msg.Format(_T("Wait for decompression !!! IP: %s"), gVecIP[gIndex]);
			CT2A pszA(msg);   // 转成 ANSI
			LPCSTR pStr = pszA;
			SetWindowTextA(hText, pStr);//过程

			CString* str = new CString(msg + _T("\r\n"));
			::PostMessage(gHwnd, WM_UPDATE_EDIT, 0, (LPARAM)str);

			::SendMessage(gHwnd, WM_MY_START_TIME_MESSAGE, (WPARAM)0, (LPARAM)0);

			g_Prog->SetPos(50);
			if (gbuff) {
				free(gbuff);
				gbuff = NULL;
			}
			if (gdata) {
				free(gdata);
				gdata = NULL;
			}
			cJSON_Delete(obj);
			return -2;
		}
		//exit(0);
	case 3:
		if ((int)cJSON_GetNumberValue(status))
		{
			printf("Authentication failed\n");//认证失败
			printf("%s %d exit\n", __func__, __LINE__);
			CString msg;
			msg.Format(_T("Authentication failed !!! IP: %s"), gVecIP[gIndex]);
			CT2A pszA(msg);   // 转成 ANSI
			LPCSTR pStr = pszA;
			SetWindowTextA(hText, pStr);//过程

			CString* str = new CString(msg + _T("\r\n"));
			::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

			g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
			::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
			if (gbuff) {
				free(gbuff);
				gbuff = NULL;
			}
			cJSON_Delete(obj);
			//exit(-1);
			return -1;
		}
		break;
	case 4:{
		int ret = -1;
		ret = (int)cJSON_GetNumberValue(status);
		switch (ret) {
		case 0: {
			CString msg;
			msg.Format(_T("Update finish,decompression OK !!! IP: %s"), gVecIP[gIndex]);
			CT2A pszA(msg);   // 转成 ANSI
			LPCSTR pStr = pszA;
			SetWindowTextA(hText, pStr);//过程

			CString* str = new CString(msg + _T("\r\n"));
			::PostMessage(gHwnd, WM_UPDATE_EDIT, 0, (LPARAM)str);

			::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
			if (gbuff) {
				free(gbuff);
				gbuff = NULL;
			}
			if (gdata) {
				free(gdata);
				gdata = NULL;
			}
			cJSON_Delete(obj);
			return 1;
		}
		case 1: {
			CString msg;
			msg.Format(_T("Version is lower !!! IP: %s"), gVecIP[gIndex]);
			CT2A pszA(msg);   // 转成 ANSI
			LPCSTR pStr = pszA;
			SetWindowTextA(hText, pStr);//过程

			CString* str = new CString(msg + _T("\r\n"));
			::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

			::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
			g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
			if (gbuff) {
				free(gbuff);
				gbuff = NULL;
			}
			if (gdata) {
				free(gdata);
				gdata = NULL;
			}
			cJSON_Delete(obj);
			return -1;
		}
		case 2: {
			CString msg;
			msg.Format(_T("Update failed , please try again !!! IP: %s"), gVecIP[gIndex]);
			CT2A pszA(msg);   // 转成 ANSI
			LPCSTR pStr = pszA;
			SetWindowTextA(hText, pStr);//过程

			CString* str = new CString(msg + _T("\r\n"));
			::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

			::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
			g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
			if (gbuff) {
				free(gbuff);
				gbuff = NULL;
			}
			cJSON_Delete(obj);
			return -1;
		}
		}//switch
		}//case 
	default:
		printf("unknown func %d\n", (int)cJSON_GetNumberValue(func));
	}
	if (gbuff) {
		free(gbuff);
		gbuff = NULL;
	}
	if (gdata) {
		free(gdata);
		gdata = NULL;
	}
	cJSON_Delete(obj);
	return -2;
}

// 通过IP地址和子网掩码计算网络号，传入正确的ip，正确的netmask，网络号，网络号长度。 成功返回1，失败返回0
int ip_netmask_to_NSID(char* ip, char* netmask, char* NSID, int size)
{
	int ip_len = 0;
	int netmask_len = 0;
	int temp_len = 0;
	int temp_len2 = 0;
	int ip_arr[4] = { 0 };
	int netmask_arr[4] = { 0 };
	int NSID_arr[4] = { 0 };
	int i = 0;
	char temp_str[4] = { 0 };
	char NSID_str[16] = { 0 };
	char* p_ip = ip;
	char* p_netmask = netmask;

	// 临时指针
	char* temp = NULL;
	// 4次循环依次获取每位ip
	for (i = 0; i < 4; i++)
	{
		// printf("i=%d\n", i);
		ip_len = strlen(p_ip);
		// 获取'.'首次出现的位置
		if (i != 3)
		{
			temp = strchr(p_ip, '.');
			if (NULL == temp)
			{
				return 0;
			}
		}
		else
		{
			temp = p_ip;
		}
		temp_len = strlen(temp);
		// printf("temp_len=%d\n", temp_len);
		// 计算第一位ip的长度
		if (i != 3)
		{
			temp_len2 = ip_len - temp_len;
		}
		else
		{
			temp_len2 = temp_len;
		}
		// printf("temp_len2=%d\n", temp_len2);
		// 字符串截取
		memset(temp_str, 0, sizeof(temp_str));
		strncpy(temp_str, p_ip, temp_len2);
		// 存入数组
		ip_arr[i] = atoi(temp_str);
		// printf("ip_arr[%d]=%d\n", i, ip_arr[i]);
		// 前3个进行指针偏移
		if (i != 3)
		{
			p_ip += (temp_len2 + 1);
		}
	}

	temp = NULL;
	temp_len = 0;
	temp_len2 = 0;

	// 4次循环依次获取每位netmask
	for (i = 0; i < 4; i++)
	{
		// printf("i=%d\n", i);
		netmask_len = strlen(p_netmask);
		// 获取'.'首次出现的位置
		if (i != 3)
		{
			temp = strchr(p_netmask, '.');
			if (NULL == temp)
			{
				return 0;
			}
		}
		else
		{
			temp = p_netmask;
		}
		temp_len = strlen(temp);
		// printf("temp_len=%d\n", temp_len);
		// 计算netmask的长度
		if (i != 3)
		{
			temp_len2 = netmask_len - temp_len;
		}
		else
		{
			temp_len2 = temp_len;
		}
		// printf("temp_len2=%d\n", temp_len2);
		// 字符串截取
		memset(temp_str, 0, sizeof(temp_str));
		strncpy(temp_str, p_netmask, temp_len2);
		// 存入数组
		netmask_arr[i] = atoi(temp_str);
		// printf("netmask_arr[%d]=%d\n", i, netmask_arr[i]);
		// 前3个进行指针偏移
		if (i != 3)
		{
			p_netmask += (temp_len2 + 1);
		}
	}

	temp = NULL;

	// 计算各位网络号
	for (i = 0; i < 4; i++)
	{
		NSID_arr[i] = ip_arr[i] & netmask_arr[i];
	}

	// 拼接为完整的网络号
	snprintf(NSID_str, 15, "%d.%d.%d.%d", NSID_arr[0], NSID_arr[1], NSID_arr[2], NSID_arr[3]);

	strncpy(NSID, NSID_str, size);

	return 1;
}

bool CupdateZPDUDlg::etLocalAdaptersInfo()
{
	//IP_ADAPTER_INFO结构体
	m_ComboBox.ResetContent();
	PIP_ADAPTER_INFO pIpAdapterInfo = NULL;
	pIpAdapterInfo = new IP_ADAPTER_INFO;
	bool flag = false;

	//结构体大小
	unsigned long ulSize = sizeof(IP_ADAPTER_INFO);

	//获取适配器信息
	int nRet = GetAdaptersInfo(pIpAdapterInfo, &ulSize);

	if (ERROR_BUFFER_OVERFLOW == nRet)
	{
		//空间不足，删除之前分配的空间
		delete[]pIpAdapterInfo;

		//重新分配大小
		pIpAdapterInfo = (PIP_ADAPTER_INFO) new BYTE[ulSize];
		flag = true;

		//获取适配器信息
		nRet = GetAdaptersInfo(pIpAdapterInfo, &ulSize);

		//获取失败
		if (ERROR_SUCCESS != nRet)
		{
			if (pIpAdapterInfo != NULL)
			{
				delete[]pIpAdapterInfo;
			}
			return FALSE;
		}
	}

	//MAC 地址信息
	char szMacAddr[20];
	//赋值指针
	PIP_ADAPTER_INFO pIterater = pIpAdapterInfo;
	while (pIterater)
	{
		//cout<<"网卡名称："<<pIterater->AdapterName<<endl;

		//cout<<"网卡描述："<<pIterater->Description<<endl;

		sprintf_s(szMacAddr, 20, "%02X-%02X-%02X-%02X-%02X-%02X",
			pIterater->Address[0],
			pIterater->Address[1],
			pIterater->Address[2],
			pIterater->Address[3],
			pIterater->Address[4],
			pIterater->Address[5]);

		//cout<<"MAC 地址："<<szMacAddr<<endl;

		//cout<<"IP地址列表："<<endl<<endl;

		//指向IP地址列表
		PIP_ADDR_STRING pIpAddr = &pIterater->IpAddressList;
		while (pIpAddr)
		{
			//cout << "IP地址：  " << pIpAddr->IpAddress.String << endl;
			CString temp(pIpAddr->IpAddress.String);
			if (temp != "0.0.0.0") {
				char netid[64];
				if (ip_netmask_to_NSID(pIpAddr->IpAddress.String, pIpAddr->IpMask.String, netid, 64))
				{
					netid[strlen(netid) - 1] = '\0';
					CString temp(netid);
					m_ComboBox.AddString(temp);
				}
			}
			
			//cout<<"子网掩码："<<pIpAddr->IpMask.String<<endl;

			//指向网关列表
			PIP_ADDR_STRING pGateAwayList = &pIterater->GatewayList;
			while (pGateAwayList)
			{
				//cout<<"网关：    "<<pGateAwayList->IpAddress.String<<endl;

				pGateAwayList = pGateAwayList->Next;
			}

			pIpAddr = pIpAddr->Next;
		}
		//cout<<endl<<"--------------------------------------------------"<<endl;

		pIterater = pIterater->Next;
	}
	//清理
	if (flag) {
		if (pIpAdapterInfo)
		{
			delete[]pIpAdapterInfo;
			pIpAdapterInfo = NULL;
		}
	}
	else
	{
		if (pIpAdapterInfo)
		{
			delete pIpAdapterInfo;
			pIpAdapterInfo = NULL;
		}
	}

	return TRUE;
}


bool CupdateZPDUDlg::etLocalAdaptersInfoEnd()
{
	//IP_ADAPTER_INFO结构体
	m_endIPAddress.ResetContent();
	PIP_ADAPTER_INFO pIpAdapterInfo = NULL;
	pIpAdapterInfo = new IP_ADAPTER_INFO;
	bool flag = false;

	//结构体大小
	unsigned long ulSize = sizeof(IP_ADAPTER_INFO);

	//获取适配器信息
	int nRet = GetAdaptersInfo(pIpAdapterInfo, &ulSize);

	if (ERROR_BUFFER_OVERFLOW == nRet)
	{
		//空间不足，删除之前分配的空间
		delete[]pIpAdapterInfo;

		//重新分配大小
		pIpAdapterInfo = (PIP_ADAPTER_INFO) new BYTE[ulSize];
		flag = true;

		//获取适配器信息
		nRet = GetAdaptersInfo(pIpAdapterInfo, &ulSize);

		//获取失败
		if (ERROR_SUCCESS != nRet)
		{
			if (pIpAdapterInfo != NULL)
			{
				delete[]pIpAdapterInfo;
			}
			return FALSE;
		}
	}

	//MAC 地址信息
	char szMacAddr[20];
	//赋值指针
	PIP_ADAPTER_INFO pIterater = pIpAdapterInfo;
	while (pIterater)
	{
		//cout<<"网卡名称："<<pIterater->AdapterName<<endl;

		//cout<<"网卡描述："<<pIterater->Description<<endl;

		sprintf_s(szMacAddr, 20, "%02X-%02X-%02X-%02X-%02X-%02X",
			pIterater->Address[0],
			pIterater->Address[1],
			pIterater->Address[2],
			pIterater->Address[3],
			pIterater->Address[4],
			pIterater->Address[5]);

		//cout<<"MAC 地址："<<szMacAddr<<endl;

		//cout<<"IP地址列表："<<endl<<endl;

		//指向IP地址列表
		PIP_ADDR_STRING pIpAddr = &pIterater->IpAddressList;
		while (pIpAddr)
		{
			//cout << "IP地址：  " << pIpAddr->IpAddress.String << endl;
			CString temp(pIpAddr->IpAddress.String);
			if (temp != "0.0.0.0") {
				char netid[64];
				if (ip_netmask_to_NSID(pIpAddr->IpAddress.String, pIpAddr->IpMask.String, netid, 64))
				{
					netid[strlen(netid) - 1] = '\0';
					CString temp(netid);
					m_endIPAddress.AddString(temp);
				}
			}

			//cout<<"子网掩码："<<pIpAddr->IpMask.String<<endl;

			//指向网关列表
			PIP_ADDR_STRING pGateAwayList = &pIterater->GatewayList;
			while (pGateAwayList)
			{
				//cout<<"网关：    "<<pGateAwayList->IpAddress.String<<endl;

				pGateAwayList = pGateAwayList->Next;
			}

			pIpAddr = pIpAddr->Next;
		}
		//cout<<endl<<"--------------------------------------------------"<<endl;

		pIterater = pIterater->Next;
	}
	//清理
	if (flag) {
		if (pIpAdapterInfo)
		{
			delete[]pIpAdapterInfo;
			pIpAdapterInfo = NULL;
		}
	}
	else
	{
		if (pIpAdapterInfo)
		{
			delete pIpAdapterInfo;
			pIpAdapterInfo = NULL;
		}
	}

	return TRUE;
}

void CupdateZPDUDlg::fun()
{
	
	/*memset(update3, 0, 1024);
	strcat(update3, update1);
	if (m_boot)
	{
		strcat(update3, "u");
	}
	if (m_kernel)
	{
		strcat(update3, "kd");
	}
	if (m_app)
	{
		strcat(update3, "r");
	}
	strcat(update3, update2);*/
}

CupdateZPDUDlg::CupdateZPDUDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_UPDATEZPDU_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CupdateZPDUDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO1, m_ComboBox);
	DDX_Control(pDX, IDC_CHECK1, m_BootLoader);
	DDX_Control(pDX, IDC_CHECK2, m_Kernel);
	DDX_Control(pDX, IDC_CHECK3, m_App);
	DDX_Control(pDX, IDC_COMBO2, m_method);
	DDX_Control(pDX, IDC_COMBO3, m_endIPAddress);
	DDX_Control(pDX, IDC_EDIT1, m_editOK);
	DDX_Control(pDX, IDC_EDIT2, m_editError);
}

BEGIN_MESSAGE_MAP(CupdateZPDUDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_UPDATE, &CupdateZPDUDlg::OnBnClickedUpdate)
	ON_BN_CLICKED(IDC_CHOOSE_BTN, &CupdateZPDUDlg::OnBnClickedChooseBtn)
	ON_BN_CLICKED(IDC_CHECK1, &CupdateZPDUDlg::OnBnClickedCheck1)
	ON_BN_CLICKED(IDC_CHECK2, &CupdateZPDUDlg::OnBnClickedCheck2)
	ON_BN_CLICKED(IDC_CHECK3, &CupdateZPDUDlg::OnBnClickedCheck3)
	ON_WM_TIMER()
	ON_MESSAGE(WM_MY_MESSAGE , &CupdateZPDUDlg::OnMyMessage)
	ON_MESSAGE(WM_MY_PROGESS_MESSAGE, &CupdateZPDUDlg::OnMyProgressMessage)
	ON_MESSAGE(WM_MY_START_TIME_MESSAGE, &CupdateZPDUDlg::OnMyStartTimerMessage)
	ON_CBN_DROPDOWN(IDC_COMBO1, &CupdateZPDUDlg::OnCbnDropdownCombo1)
	ON_CBN_SELCHANGE(IDC_COMBO1, &CupdateZPDUDlg::OnCbnSelchangeCombo1)
	ON_CBN_SELCHANGE(IDC_COMBO2, &CupdateZPDUDlg::OnCbnSelchangeCombo2)

	ON_CBN_DROPDOWN(IDC_COMBO3, &CupdateZPDUDlg::OnCbnDropdownCombo3)
	ON_CBN_SELCHANGE(IDC_COMBO3, &CupdateZPDUDlg::OnCbnSelchangeCombo3)

	ON_MESSAGE(WM_UPDATE_EDIT, &CupdateZPDUDlg::OnUpdateEdit)
	ON_MESSAGE(WM_UPDATE_ERROR_EDIT, &CupdateZPDUDlg::OnUpdateErrorEdit)//WM_UPDATE_ERROR_EDIT
END_MESSAGE_MAP()


// CupdateZPDUDlg 消息处理程序

BOOL CupdateZPDUDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标
	
	etLocalAdaptersInfo();
	if (m_ComboBox.GetCount())
	{
		m_ComboBox.SetCurSel(0);
	}
	m_ComboBox.SetFocus();
	keybd_event(VK_RIGHT, 0, 0, 0);
	m_App.SetCheck(true);
	m_BootLoader.SetCheck(true);
	m_Kernel.SetCheck(true);

	m_App.ShowWindow(false);
	m_Kernel.ShowWindow(false);
	m_BootLoader.ShowWindow(false);
	((CWnd*)GetDlgItem(IDC_STATIC6))->ShowWindow(false);
	m_boot = 1;
	m_kernel = 1;
	m_app = 1;
	m_batch = 0;
	gIndex = 0;
	hideControl();

	CString str("Individual upgrade");
	m_method.InsertString(0,str);
	str = "Batch upgrade";
	m_method.InsertString(1,str);
	m_method.SetCurSel(0);

	g_Prog = (CProgressCtrl*)GetDlgItem(IDC_PROGRESS1);
	g_Prog->SetRange(0, 100);
	g_Prog->SetPos(0);

	g_TotalProg = (CProgressCtrl*)GetDlgItem(IDC_PROGRESS2);
	g_TotalProg->SetRange(0, 100);
	g_TotalProg->SetPos(0);
	gHwnd = this->m_hWnd;
	return FALSE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CupdateZPDUDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CupdateZPDUDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

unsigned WINAPI MainThread(void* param)
{
	int vecSize = gVecIP.size();
	for (gIndex = 0; gIndex < vecSize; gIndex++)
	{
		int count = 0;
		BOOL bResult = false;

		PingReply* reply = new PingReply;
		do
		{
			CPing objPing;
			count++;
			char buf[255];
			strncpy_s(buf, CT2A(gVecIP[gIndex]), sizeof(buf));
			bResult = objPing.Ping(buf, reply);
			if (bResult == TRUE)
			{
				break;
			}
			Sleep(1000);
		} while (count < 10);
		if (bResult == false)
		{
			CString msg;
			msg.Format(_T("ping error!!! IP: %s"), gVecIP[gIndex]);
			CT2A pszA(msg);   // 转成 ANSI
			LPCSTR pStr = pszA;
			SetWindowTextA(hText, pStr);//过程

			CString* str = new CString(msg + _T("\r\n"));
			::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

			::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
			g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(255, 0, 0));
			if (reply) { delete reply; reply = NULL; }
			//return -1;
			::PostMessage(gHwnd, WM_MY_PROGESS_MESSAGE, (WPARAM)(gIndex+1), (LPARAM)0);
			continue;
		}
		if (reply) { delete reply; reply = NULL; }
		memset(&g_client_info, 0, sizeof(g_client_info));

		WORD	wVersionRequested;
		WSADATA wsaData;
		wVersionRequested = MAKEWORD(2, 2);
		if (WSAStartup(wVersionRequested, &wsaData))
		{
			printf("Load WinSock Failed!\n");
			printf("%s %d exit\n", __func__, __LINE__);
			CString msg;
			msg.Format(_T("Load WinSock Failed !!! IP: %s"), gVecIP[gIndex]);
			CT2A pszA(msg);   // 转成 ANSI
			LPCSTR pStr = pszA;
			SetWindowTextA(hText, pStr);//过程

			CString* str = new CString(msg + _T("\r\n"));
			::PostMessage(gHwnd, WM_UPDATE_ERROR_EDIT, 0, (LPARAM)str);

			//return -1;
			::PostMessage(gHwnd, WM_MY_PROGESS_MESSAGE, (WPARAM)(gIndex + 1), (LPARAM)0);
			continue;
		}

		client_info* info = &g_client_info;
		char name[512];
		memset(name, 0, sizeof(char) * 512);
		char password[512];
		memset(password, 0, sizeof(char) * 512);
		::wsprintfA(name, "%ls", (LPCTSTR)(gName));
		::wsprintfA(password, "%ls", (LPCTSTR)(gPassword));
		//char key[2048] = "zpdu";
		//strcat(key, name);
		//strcat(key, password);

		unsigned char temp[16];
		char temp1[32 + 1];
		compute_pbkdf2((uint8_t*)password, strlen(password), (uint8_t*)password, strlen(password), 1000, 16, temp);
		print_as_hex_temp(temp, sizeof(temp), temp1);
		temp1[31] = '\0';
		sprintf((char*)info->aes_cbc_key, "%s%s", name, temp1);
		printf("aes cbc key: %s\n", info->aes_cbc_key);
		//if (Compute_string_md5((unsigned char*)AES_KEY, strlen(AES_KEY), (char*)info->aes_cbc_key) < 0)
		//{
		//	printf("%s Compute_string_md5 error\n", __func__);
		//	return -1;
		//}
		//printf("aes cbc key: %s\n", info->aes_cbc_key);
		aes_init((const void*)info->aes_cbc_key);


		get_file(info);

		open_ctrl_sock(info);

		say_hello(info);
		int ret = -3;
		while (1)
		{
			ret = recv_data_(info);
			if (ret == 1 || ret == -1) break;
		}

		WSACleanup();
		fclose(info->file);
		memset(info->cipher, 0, sizeof(info->cipher));

		::PostMessage(gHwnd, WM_MY_PROGESS_MESSAGE, (WPARAM)(gIndex + 1), (LPARAM)0);
	}
	::PostMessage(gHwnd, WM_MY_MESSAGE, (WPARAM)0, (LPARAM)0);
	//::PostMessage(gHwnd, WM_MY_PROGESS_MESSAGE, (WPARAM)(gIndex), (LPARAM)0);
	return 0;
}

bool CupdateZPDUDlg::IsValidIPv4(const CString& ip)
{
	in_addr addr;
	return InetPton(AF_INET, ip, &addr) == 1;
}

bool CupdateZPDUDlg::IsValidIPv6(const CString& ip)
{
	in6_addr addr6;
	return InetPton(AF_INET6, ip, &addr6) == 1;
}


// IP 转整数
unsigned int IpToInt(const CString& ip)
{
	unsigned int a, b, c, d;
	_stscanf_s(ip, _T("%u.%u.%u.%u"), &a, &b, &c, &d);
	return (a << 24) | (b << 16) | (c << 8) | d;
}

// 整数转 IP
CString IntToIp(unsigned int ipInt)
{
	unsigned int a = (ipInt >> 24) & 0xFF;
	unsigned int b = (ipInt >> 16) & 0xFF;
	unsigned int c = (ipInt >> 8) & 0xFF;
	unsigned int d = ipInt & 0xFF;

	CString ip;
	ip.Format(_T("%u.%u.%u.%u"), a, b, c, d);
	return ip;
}

// 生成 IP 范围，排除网络地址和广播地址
std::vector<CString> GenerateIpRangeExcludeNetBroadcast(
	const CString& startIp,
	const CString& endIp,
	const CString& netmask)
{
	std::vector<CString> ipList;

	unsigned int start = IpToInt(startIp);
	unsigned int end = IpToInt(endIp);
	unsigned int mask = IpToInt(netmask);

	if (start > end) std::swap(start, end);

	// 网络地址 & 广播地址
	unsigned int network = start & mask;
	unsigned int broadcast = network | (~mask);

	for (unsigned int ip = start; ip <= end; ++ip)
	{
		if (ip == network || ip == broadcast)
			continue; // 跳过网络地址和广播地址

		ipList.push_back(IntToIp(ip));
	}

	return ipList;
}

void CupdateZPDUDlg::OnBnClickedUpdate()
{
	// TODO: 在此添加控件通知处理程序代码

	m_editOK.SetSel(0, -1);   // 全选
	m_editOK.Clear();
	m_editError.SetSel(0, -1);   // 全选
	m_editError.Clear();

	CString tempip;
	if (m_batch == 0) {
		m_ComboBox.GetWindowTextW(tempip);
		::wsprintfA(gStartIp, "%ls", (LPCTSTR)tempip);
	}else {
		m_ComboBox.GetWindowTextW(tempip);
		::wsprintfA(gStartIp, "%ls", (LPCTSTR)tempip);
		m_endIPAddress.GetWindowTextW(tempip);
		::wsprintfA(gEndIp, "%ls", (LPCTSTR)tempip);
		g_TotalProg->SetPos(0);
		g_TotalProg->SendMessage(PBM_SETBARCOLOR, 0, RGB(0, 255, 0));
	}
	GetDlgItem(IDC_ACCOUNT)->GetWindowTextW(gName);
	GetDlgItem(IDC_PASSWORD)->GetWindowTextW(gPassword);//IDC_TIPS
	GetDlgItem(IDC_TIPS, &hText);
	g_Prog->SetPos(0);
	g_Prog->SendMessage(PBM_SETBARCOLOR, 0, RGB(0, 255, 0));
	
	if (!(m_boot || m_kernel || m_app)) {
		SetWindowTextA(hText, "Please choose update content !!!");
		return;
	}
	if (m_batch == 0) 
	{
		if (gFilePath.IsEmpty() || gName.IsEmpty() || gPassword.IsEmpty()) {
			SetWindowTextA(hText, "Filepath , Name, Password is empty !!!");
		}
		else if (strlen(gStartIp) == 0) {
			SetWindowTextA(hText, "IP address is empty !!!");
		}
		else if (!IsValidIPv4(CString(gStartIp) ) ) {
			SetWindowTextA(hText, "IP address is invalid !!!");
		}
		else
		{
			gIndex = 0;
			gVecIP = std::vector<CString>();
			gVecIP.push_back(CString(gStartIp));
			
			((CButton*)GetDlgItem(IDC_UPDATE))->EnableWindow(false);
			((CButton*)GetDlgItem(IDC_CHOOSE_BTN))->EnableWindow(false);
			(GetDlgItem(IDC_ACCOUNT)->EnableWindow(false));
			(GetDlgItem(IDC_PASSWORD)->EnableWindow(false));
			(GetDlgItem(IDC_COMBO1)->EnableWindow(false));
			(GetDlgItem(IDC_FILEPATH)->EnableWindow(false));
			fun();
			CString msg;
			msg.Format(_T("Update start !!! num: %d"), gVecIP.size());
			CT2A pszA(msg);   // 转成 ANSI
			LPCSTR pStr = pszA;
			SetWindowTextA(hText, pStr);
			gMainThreads = (HANDLE)_beginthreadex(NULL, 0, MainThread, 0, 0, NULL);

		}
	}
	else
	{
		if ( gFilePath.IsEmpty() || gName.IsEmpty() || gPassword.IsEmpty()) 
		{
			SetWindowTextA(hText, "Filepath , Name, Password is empty !!!");
		}
		else if (strlen(gStartIp) == 0  || strlen(gEndIp) == 0)
		{
			SetWindowTextA(hText, "IP address is empty !!!");
		}
		else if (!IsValidIPv4(CString(gStartIp))|| !IsValidIPv4(CString(gEndIp))) 
		{
			SetWindowTextA(hText, "IP address is invalid !!!");
		}
		else
		{
			gIndex = 0;
			
			gVecIP = std::vector<CString>();
			gVecIP = GenerateIpRangeExcludeNetBroadcast(CString(gStartIp), CString(gEndIp),CString("255.255.255.0"));
			//for (const auto& ip : gVecIP)
			//{
			//	AfxMessageBox(ip); // 输出结果
			//}
			((CButton*)GetDlgItem(IDC_UPDATE))->EnableWindow(false);
			((CButton*)GetDlgItem(IDC_CHOOSE_BTN))->EnableWindow(false);
			GetDlgItem(IDC_ACCOUNT)->EnableWindow(false);
			GetDlgItem(IDC_COMBO3)->EnableWindow(false);
			GetDlgItem(IDC_PASSWORD)->EnableWindow(false);
			GetDlgItem(IDC_COMBO1)->EnableWindow(false);
			GetDlgItem(IDC_FILEPATH)->EnableWindow(false);
			GetDlgItem(IDCANCEL)->EnableWindow(false);
			fun();
			CString msg;
			msg.Format(_T("Update start !!! num: %d"), gVecIP.size());
			CT2A pszA(msg);   // 转成 ANSI
			LPCSTR pStr = pszA;
			SetWindowTextA(hText, pStr);
			gMainThreads = (HANDLE)_beginthreadex(NULL, 0, MainThread, 0, 0, NULL);

		}
	}	
}


void CupdateZPDUDlg::OnBnClickedChooseBtn()
{
	// TODO: 在此添加控件通知处理程序代码
	CFileDialog FDlg(TRUE, _T(".tar.bz2"), NULL, OFN_HIDEREADONLY, _T("Upgrade file(*.tar.bz2)|*.tar.bz2||"));

	if (FDlg.DoModal() == IDOK)
	{
		gFilePath = FDlg.GetPathName();
		GetDlgItem(IDC_FILEPATH)->SetWindowText(gFilePath);
	}
}



void CupdateZPDUDlg::OnBnClickedCheck1()
{
	// TODO: 在此添加控件通知处理程序代码
	m_boot = !m_boot;
	
}


void CupdateZPDUDlg::OnBnClickedCheck2()
{
	// TODO: 在此添加控件通知处理程序代码
	m_kernel = !m_kernel;
}


void CupdateZPDUDlg::OnBnClickedCheck3()
{
	// TODO: 在此添加控件通知处理程序代码
	m_app = !m_app;
}


void CupdateZPDUDlg::OnCbnDropdownCombo1()
{
	// TODO: 在此添加控件通知处理程序代码
	this->etLocalAdaptersInfo();
}

void CupdateZPDUDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	switch (nIDEvent)
	{
	case 1:
	{
		static int count = 0;
		count++;
		g_Prog->SetPos(count+50);
		if (count == 50) 
		{
			KillTimer(1); 
			count = 0; 
			if (gIndex == gVecIP.size()) {
				GetDlgItem(IDC_UPDATE)->EnableWindow(true);
				GetDlgItem(IDC_CHOOSE_BTN)->EnableWindow(true);
				GetDlgItem(IDC_ACCOUNT)->EnableWindow(true);
				GetDlgItem(IDC_PASSWORD)->EnableWindow(true);
				GetDlgItem(IDC_COMBO1)->EnableWindow(true);
				GetDlgItem(IDC_FILEPATH)->EnableWindow(true);
				GetDlgItem(IDC_COMBO3)->EnableWindow(true);
				GetDlgItem(IDCANCEL)->EnableWindow(true);
			}
			//SetWindowTextA(hText, "Upgrade finish, wait for the PDU to restart.No power off during upgrade!!!");
		}
	}
	break;
	}

	CDialog::OnTimer(nIDEvent);
}

LRESULT CupdateZPDUDlg::OnMyMessage(WPARAM w, LPARAM l)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	g_Prog->SetPos(100);
	if (gIndex == gVecIP.size()) {
		GetDlgItem(IDC_UPDATE)->EnableWindow(true);
		GetDlgItem(IDC_CHOOSE_BTN)->EnableWindow(true);
		GetDlgItem(IDC_ACCOUNT)->EnableWindow(true);
		GetDlgItem(IDC_PASSWORD)->EnableWindow(true);
		GetDlgItem(IDC_COMBO1)->EnableWindow(true);
		GetDlgItem(IDC_FILEPATH)->EnableWindow(true);
		GetDlgItem(IDC_COMBO3)->EnableWindow(true);
		GetDlgItem(IDCANCEL)->EnableWindow(true);
	}
	return 0;
}


LRESULT CupdateZPDUDlg::OnMyProgressMessage(WPARAM w, LPARAM l)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	int index = (int)w;

	// 假设进度条范围是 0 ~ 100
	int pos = (int)((double)index / gVecIP.size() * 100);

	g_TotalProg->SetPos(pos);
	return 0;
}

LRESULT CupdateZPDUDlg::OnMyStartTimerMessage(WPARAM w, LPARAM l)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	SetTimer(1, 600, NULL);
	return 0;
}



void CupdateZPDUDlg::OnCbnSelchangeCombo1()
{
	// TODO: 在此添加控件通知处理程序代码
	m_ComboBox.SetFocus();
	keybd_event(VK_RIGHT, 0, 0, 0);
}


void CupdateZPDUDlg::OnCbnSelchangeCombo2()
{
	// TODO: 在此添加控件通知处理程序代码
	m_batch = m_method.GetCurSel();
	if (m_batch == 0) {//单独升级
		hideControl();
	}
	else {//批量升级
		showControl();
	}
}

void CupdateZPDUDlg::hideControl()
{
	GetDlgItem(IDC_STATIC)->SetWindowTextW(_T("IP Address:"));
	((CWnd*)GetDlgItem(IDC_STATICEND))->ShowWindow(false);
	((CWnd*)GetDlgItem(IDC_COMBO3))->ShowWindow(false);
	((CWnd*)GetDlgItem(IDC_STATICSUBPROGRESS2))->ShowWindow(false);
	((CWnd*)GetDlgItem(IDC_PROGRESS2))->ShowWindow(false);
}

void CupdateZPDUDlg::showControl()
{
	GetDlgItem(IDC_STATIC)->SetWindowTextW(_T("Start IP Address:"));

	((CWnd*)GetDlgItem(IDC_STATICEND))->ShowWindow(true);
	((CWnd*)GetDlgItem(IDC_COMBO3))->ShowWindow(true);
	((CWnd*)GetDlgItem(IDC_STATICSUBPROGRESS2))->ShowWindow(true);
	((CWnd*)GetDlgItem(IDC_PROGRESS2))->ShowWindow(true);
	etLocalAdaptersInfoEnd();
	if (m_endIPAddress.GetCount())
	{
		m_endIPAddress.SetCurSel(0);
	}
}


void CupdateZPDUDlg::OnCbnSelchangeCombo3()
{
	// TODO: 在此添加控件通知处理程序代码
	m_endIPAddress.SetFocus();
	keybd_event(VK_RIGHT, 0, 0, 0);
}

void CupdateZPDUDlg::OnCbnDropdownCombo3()
{
	// TODO: 在此添加控件通知处理程序代码
	this->etLocalAdaptersInfoEnd();
}

LRESULT CupdateZPDUDlg::OnUpdateEdit(WPARAM wParam, LPARAM lParam)
{
	CString* pStr = (CString*)lParam;
	m_editOK.SetSel(0, 0);
	m_editOK.ReplaceSel(*pStr);
	delete pStr;                      // 记得释放
	return 0;
}


LRESULT CupdateZPDUDlg::OnUpdateErrorEdit(WPARAM wParam, LPARAM lParam)
{
	CString* pStr = (CString*)lParam;
	m_editError.SetSel(0, 0);
	m_editError.ReplaceSel(*pStr);
	delete pStr;                      // 记得释放
	return 0;
}