//
//  AJBMqttDefines.h
//  LKMqttDemo
//
//  Created by anjubao on 4/12/16.
//  Copyright © 2016 anjubao. All rights reserved.
//

#ifndef MqttDefines_h
#define MqttDefines_h

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "PlatformDefines.h"
#include "MQTTPacket.h"
#include "MqttDispatcher.h"

#define PACKET_TIMEOUT          65535
#define PACKET_BUF_SIZE         2048
#define MAX_CLIENT_ID_LEN       128
#define MAX_TOPIC_LEN           128
#define MAX_CONTENT_LEN         2048
#define MAX_USERNAME_LEN        128
#define MAX_PASSWORD_LEN        128


#define MAX_PACKET_ID           65535       /*最大packet type id   0x1111111111111111*/
#define MAX_MESSAGE_HANDLERS    5

#define MAX_KEEPALIVE_TIMEO     10          /*心跳延时允许的最大时间*/
#define ERR_PACKET_TYPE         65534       /*网络异常时，设定PACKET_TYPE无效,设定错误id为0x1111111111111110*/

#define ENABLE_LOG              1
#define ENABLE_LOG_TIMESTAMP    1

/*
extern char enableLog;
extern char enableLogTimestamp;
extern char enableLogLocal;
*/

extern char log_file_path[128];
extern char log_send_path[128];
extern char log_erro_path[128];

void errTolocal(void *data,int len);
char *timestamp();

#define logToLocal(index,file_path,fmt,...) {\
    char path[128]={};\
    sprintf(path,"log/%d-%s",index,file_path);\
    FILE *fp = fopen(path, "a+");\
    if (fp) {\
        if (ENABLE_LOG_TIMESTAMP){\
            struct timeval timev;\
            gettimeofday(&timev, NULL);\
            time_t ctime = timev.tv_sec;\
            struct tm *timeinfo = localtime(&ctime);\
            fprintf(fp,"%d-%02d-%02d %02d:%02d:%02d:%3d ",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec,timev.tv_usec/1000);\
        }\
        fprintf(fp,fmt,##__VA_ARGS__);\
        fprintf(fp,"\n");\
        fclose(fp);\
    }\
    else{\
        fprintf(stderr,"[WRITE FAILED] [%s] "fmt"\n",log_file_path,##__VA_ARGS__);\
        fflush(stderr);\
    }\
}

#if ENABLE_LOG
#define MqttLog(fmt,...) {\
                            if (ENABLE_LOG_TIMESTAMP){\
                                printf("%s: ",timestamp());\
                            }\
                            printf(fmt,##__VA_ARGS__);\
                            printf("\n");\
                         }
#else
#define MqttLog(fmt,...) {}
#endif

#ifndef bool
typedef char  bool;
#endif
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif
enum QoS { QOS0 = 0, QOS1, QOS2 };
typedef enum QoS MqttServiceQos;

typedef struct {
    
    char willContent[MAX_CONTENT_LEN];
    char willTopic[MAX_TOPIC_LEN];
    MqttServiceQos qos;
    int retain;
    int willFlag;
}MqttClientWillInfo;

typedef struct {
    unsigned char auto_con; //auto reconnect enable flag
    unsigned char recon_int;//reconnect interval
    unsigned char recon_max;//reconnect max count
    unsigned char reconnecting;  //reconnecting flag,default 0
}KeepAliveAttr;

typedef struct{
    char *publishContent;
    char *publishTopic;
    int qos;
    int dup;
    int retain;
}MqttClientPublishInfo;

#define MqttPublishInfoIniter {NULL,NULL,2,0,0}

typedef enum {
    MqttReturnCodeOverflow = -2,
    MqttReturnCodeFailed = -1,
    MqttReturnCodeSuccess = 0,
}MqttReturnCode;

typedef struct {
    KeepAliveAttr aliveAttr;
    int keepAlive;
    int timeout_ms;
    char topic[32];
    int qos;
    char host[32];
    short port;
    char username[32];
    char password[32];
    char clientid[32];
    char log_file[64];
    char cleansession;
    
    int sessionCount;
    int startInterval;
    int pubCount;
    int pubInterval_ms;
    int disable_subscribe;
//    char willflag;
//    char willtopic[32];
//    char willcontent[64];
//    int willqos;
}MqttConfigure;

#define DEFAULT_CONFIG {{1,1,0,0},20,2000,"",2,"",1883,"username","password","clientid","log_file",1}

#endif /* AJBMqttDefines_h */
