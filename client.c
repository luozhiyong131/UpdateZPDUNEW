/*
 * @Author: lujie
 * @Date: 2022-10-28 14:26:29
 * @LastEditors: lujie
 * @LastEditTime: 2022-11-04 16:42:15
 * @FilePath: \update\client.c
 * @Description:
 *
 * Copyright (c) 2022 by lujie/clever, All Rights Reserved.
 */
#include "common.h"

#define TEST_FILE "/home/lujie/work/zpdu/v20-20221104/ota/标准/update_app.tar.bz2"
#define UP_FILE "/tmp/update.tar.bz2"
#define KILLALL_APP "echo Performance > /sys/bus/cpu/devices/cpu0/cpufreq/scaling_governor;touch /tmp/.update_now;killall start master order web alarm chart sensor timing  modbus modbus_tcp releasespace snmpd screen sshd"
#define UPDATE "busybox rm -rf /tmp/update;tar -jxf /tmp/update.tar.bz2 -C /tmp;cd /tmp/update;./busybox chmod 777 update;./busybox tar -Jxf system.tar.xz;./busybox sh update  -ukdr;./busybox sync;./busybox flash_eraseall /dev/mtd1;./busybox reboot -f"
//-u uboot引导 -k kernel -d device-tree 系统 -r app 应用
#define SERVER_IP "192.168.10.164" 
#define AES_KEY "zpduadminSz123456"

typedef struct client_info
{
    char name[1024]; //文件名
    char update[1024]; //升级命令
    char pre_update[1024]; //预升级命令
    char update_file[1024]; //保存文件名
    FILE *file;
    int size;
    char md5[64]; //校验码
    unsigned char aes_cbc_key[32];
    unsigned char *cipher;
    dyad_Event *e;
} client_info;
client_info g_client_info;

static void check_md5(dyad_Event *e)
{
    client_info *info = (client_info*)e->udata;
    printf("%s\n",__func__);

    cJSON *func = NULL;

    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL)
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }

    if (!(func = cJSON_CreateNumber(1)))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }

    if (!cJSON_AddItemToObject(obj, "func", func))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }


    char *data = cJSON_Print(obj);
    unsigned char *c;
    int ret = aes_encrypt_buff(data, strlen(data), &c);
    dyad_write(e->stream, c, ret);
    printf("%s\n", data);
    cJSON_free(data);
    cJSON_Delete(obj);
    free(c);

}
static void update_now(dyad_Event *e)
{
    printf("%s\n",__func__);
    client_info *info = (client_info*)e->udata;
    
    cJSON *func = NULL;

    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL)
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }

    if (!(func = cJSON_CreateNumber(2)))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }

    if (!cJSON_AddItemToObject(obj, "func", func))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }


    char *data = cJSON_Print(obj);
    unsigned char *c;
    int ret = aes_encrypt_buff(data, strlen(data), &c);
    dyad_write(e->stream, c, ret);
    printf("%s\n", data);
    cJSON_free(data);
    cJSON_Delete(obj);
    free(c);

}
static void onClose(dyad_Event *e)
{
    client_info *info = (client_info*)e->udata;
    printf("%s\n",__func__);
    sleep(2);
    check_md5(info->e);
}

static void send_now(dyad_Event *e)
{
    client_info *info = (client_info*)e->udata;
    printf("%s\n", __func__);
    // dyad_writef(e->stream, "%r", info->file);
    dyad_write(e->stream, info->cipher, info->size);
    dyad_end(e->stream);
}
static void start_trans(client_info *info)
{
    printf("%s\n", __func__);
    dyad_Stream *s = dyad_newStream();

    dyad_addListener(s, DYAD_EVENT_CONNECT, send_now, (void *)info);
    dyad_addListener(s, DYAD_EVENT_CLOSE, onClose, (void *)info);
    dyad_connect(s, SERVER_IP, 30965);
}

static void onData_ctrl(dyad_Event *e)
{
    client_info *info = (client_info*)e->udata;
    info->e = e;

    char *data;
    if (aes_decrypt_buff((unsigned char**)&data, e->size, (unsigned char*)e->data) <= 0)
    {
        printf("%s aes_decrypt_buff error\n", __func__);
        data = e->data;
    }

    printf("%s\n", data);
    cJSON *obj = cJSON_Parse(data);
    if (!obj)
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }

    cJSON *func = cJSON_GetObjectItem(obj, "func");
    if (!func)
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    cJSON *status = cJSON_GetObjectItem(obj, "status");
    if (!status)
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }

    if (!cJSON_IsNumber(func) || !cJSON_IsNumber(status))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }

    switch ((int)cJSON_GetNumberValue(func))
    {
    case 0:
        if((int)cJSON_GetNumberValue(status))
            start_trans(info);
        else
            printf("hello error\n");
        break;
    case 1:
        if((int)cJSON_GetNumberValue(status) == 1)
        {
            update_now(info->e);
            printf("update now\n");
        }
        else if((int)cJSON_GetNumberValue(status) == -1)
            printf("update error\n");
        else
        {
            usleep(500*1000);
            check_md5(info->e);
        }
        break;
    case 2:
        if((int)cJSON_GetNumberValue(status))
        {
            printf("update ok\n");//finish 
        }
        exit(0);
    case 3:
        if((int)cJSON_GetNumberValue(status))
        {
            printf("Authentication failed\n");//name password error
            exit(-1);
        }
    default:
        printf("unknown func %d\n", (int)cJSON_GetNumberValue(func));
    }
}
static void get_md5(client_info *info)
{
    if(Compute_file_md5(info->name, info->md5) < 0)
    {
        printf("%s error\n", __func__);
        exit(-1);
    }
}
static void encrypt(client_info *info)
{
    int ret = aes_encrypt_fp(info->file, &info->cipher);
    if(ret < 0)
    {
        printf("%s aes_encrypt_fp error\n", __func__);
        exit(-1);
    }

    info->size = ret;
    if(Compute_string_md5(info->cipher, ret, info->md5) < 0)
    {
        printf("%s Compute_string_md5 error\n", __func__);
        exit(-1);
    }

    // FILE *fp = fopen(".update_cipher", "wb");
    // if(!fp)
    // {
    //     perror("fopen");
    //     exit(-1);
    // }
    // if(fwrite(info->cipher, ret, 1, fp) != 1)
    // {
    //     perror("fwrite");
    //     exit(-1);
    // }
    // fclose(info->file);
    // info->file = fp;
    // info->size = ret;
}
static void get_file(client_info *info)
{
    strcpy(info->pre_update, KILLALL_APP);
    strcpy(info->update, UPDATE);
    strcpy(info->update_file, UP_FILE);
    strcpy(info->name, TEST_FILE);
    info->file = fopen(info->name, "rb");
    if (!info->file)
    {
        perror(TEST_FILE);
        exit(-1);
    }

    // fseek(info->file, 0, SEEK_END);
    // info->size = ftell(info->file);
    // rewind(info->file);

    // get_md5(info);
    encrypt(info);
    printf("file info name:%s size:%d md5:%s\n",info->name, info->size, info->md5);
}

static void onConnect(dyad_Event *e)
{
    client_info *info = (client_info *)e->udata;

    printf("connected: %s\n", e->msg);

    // get_file(info);

    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL)
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    printf("%s %d\n", __func__, __LINE__);
    cJSON *func = NULL;
    cJSON *pre_update = NULL;
    cJSON *update = NULL;
    cJSON *name = NULL;
    cJSON *size = NULL;
    cJSON *md5 = NULL;

    if (!(func = cJSON_CreateNumber(0)))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    if (!(pre_update = cJSON_CreateString(info->pre_update)))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    if (!(update = cJSON_CreateString(info->update)))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    if (!(name = cJSON_CreateString(info->update_file)))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    if (!(size = cJSON_CreateNumber(info->size)))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    if (!(md5 = cJSON_CreateString(info->md5)))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    printf("%s %d\n", __func__, __LINE__);
    if (!cJSON_AddItemToObject(obj, "func", func))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    if (!cJSON_AddItemToObject(obj, "pre_update", pre_update))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    if (!cJSON_AddItemToObject(obj, "update", update))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    if (!cJSON_AddItemToObject(obj, "name", name))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    if (!cJSON_AddItemToObject(obj, "size", size))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    if (!cJSON_AddItemToObject(obj, "md5", md5))
    {
        printf("%s %d %s", __func__, __LINE__, cJSON_GetErrorPtr());
        exit(-1);
    }
    printf("%s %d\n", __func__, __LINE__);
    char *data = cJSON_Print(obj);
    unsigned char *c;
    int ret = aes_encrypt_buff(data, strlen(data), &c);
    dyad_write(e->stream, c, ret);
    printf("%s\n", data);
    cJSON_free(data);
    cJSON_Delete(obj);
    free(c);
}

static void onClosexx(dyad_Event *e)
{
    
}

int main(void)
{
    dyad_init();
    if(Compute_string_md5(AES_KEY, strlen(AES_KEY), g_client_info.aes_cbc_key) < 0)
    {
        printf("%s Compute_string_md5 error\n", __func__);
        exit(-1);
    }
    printf("aes cbc key: %s\n", g_client_info.aes_cbc_key);
    AES_Init(g_client_info.aes_cbc_key);


    get_file(&g_client_info);

    dyad_Stream *s = dyad_newStream();
    dyad_addListener(s, DYAD_EVENT_CONNECT, onConnect, (void *)&g_client_info);
    dyad_addListener(s, DYAD_EVENT_DATA, onData_ctrl, (void *)&g_client_info);
    dyad_addListener(s, DYAD_EVENT_CLOSE, onClosexx, (void *)&g_client_info);
    dyad_connect(s, SERVER_IP, 30966);

    

    while (dyad_getStreamCount() > 0)
    {
        dyad_update();
    }

    dyad_shutdown();
    return 0;
}