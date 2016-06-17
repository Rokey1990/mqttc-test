//
//  MqttLog.c
//  MqttCommand
//
//  Created by anjubao on 16/5/24.
//  Copyright © 2016年 anjubao. All rights reserved.
//


#include "MqttDefines.h"


char enableLog = 1;
char enableLogTimestamp = 0;
char enableLogLocal = 0;
char log_file_path[128] = "mqtt_recv.log";
char log_send_path[128] = "mqtt_send.log";
char log_erro_path[128] = "mqtt_error.log";

char _timestamp[32];

void errTolocal(void *data,int len){
    FILE *fp = fopen(log_erro_path, "a+");
    if (fp) {
        if (ENABLE_LOG_TIMESTAMP){
            struct timeval timev;
            gettimeofday(&timev, NULL);
            time_t ctime = timev.tv_sec;
            time(&ctime);
            struct tm *timeinfo = localtime(&ctime);
            fprintf(fp,"[MQTTERR][%02d:%02d:%02d]",timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
        }
        fwrite(data, 1, len, fp);
        fclose(fp);
    }
}

char *timestamp(){
    struct timeval timev;
    gettimeofday(&timev, NULL);
    time_t ctime = timev.tv_sec;
    time(&ctime);
    struct tm *timeinfo = localtime(&ctime);
    sprintf(_timestamp, "%d-%02d-%02d %02d:%02d:%02d:%03d",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec,timev.tv_usec/1000);
    return _timestamp;
}