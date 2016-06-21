//
//  main.m
//  MqttCommand
//
//  Created by anjubao on 4/13/16.
//  Copyright © 2016 anjubao. All rights reserved.
//

#include <stdio.h>

#include <pthread.h>
#include <sys/stat.h>

pthread_cond_t cont_t;
pthread_mutex_t mtx;

char enable_running = 1;
const char *config_file;

#include "AJBMqttClient.h"

typedef struct {
    int interval_ms;
    int times;
}PubConfig;

typedef struct {
    int index;
    MqttConfigure   *config;
    MqttDispatcher  *dispatcher;
}SeesionConfig;

const char *configFile(int argc,const char *argv[]){
    
    if (argc<2) {
        return "config.cfg";
    }
    return argv[1];
}

void cfinish2(int sig)
{
    pthread_mutex_lock(&mtx);
    enable_running = 0;
    pthread_cond_signal(&cont_t);
    printf("sending end wait signal ...");
    pthread_mutex_unlock(&mtx);
    
}

void initLock(){
    pthread_mutex_init(&mtx, NULL);
    pthread_cond_init(&cont_t, NULL);
}

void pipeHandle(int signal){
    printf("pipe checked\n");
}


#pragma mark - callback

void on_connect(void *client1,int code){
    
    pthread_mutex_lock(&mtx);
    pthread_cond_signal(&cont_t);
    pthread_mutex_unlock(&mtx);
}
void on_subscribe(void *client1,int *qoses){
    
}

void on_receive(void *client1,char *topic,void *message,int len){
    
}

void on_hasErr(void *client1){
    //    MqttLog("[USER hasErr] client error occured" );
}

void on_shouldSubscribe(void *client){
    char topic[255]={};
    getSuggestTopic(topic, ((AJBMqttClient *)client)->clientId);
    mqttClient_subscribe(client, topic, 2);
}

void on_Loop(void *client){
    ((AJBMqttClient *)client)->keepworking = enable_running;
    if (enable_running == 0) {
        printf("begin stop client : %s\n",((AJBMqttClient *)client)->clientId);
    }
}

#pragma mark - 合并文件


void mergeFiles(int fileCount,char *basepath){
    char path[128];
    char block[2048];
    
    
        int i = 0;
        for(i = 0;i<fileCount;i++){
            FILE *bfp = fopen(basepath, "ab");
            fseek(bfp, 0, SEEK_END);
            if (bfp) {
                sprintf(path, "log/%d-%s",i,basepath);
                FILE *cfp = fopen(path, "rb");
                if (cfp) {
                    int readLen = 0;
                    int writeLen = 0;
                    while (!feof(cfp)) {
                        readLen = (int)fread(block, 1, 2048, cfp);
                        writeLen = 0;
                        while (writeLen<readLen) {
                            writeLen += fwrite(block+writeLen, 1, readLen - writeLen, bfp);
                        }
                        usleep(5);
                        
                    }
                    fclose(cfp);
                }
                fclose(bfp);
                usleep(20);
            }
        }
    
}

#pragma mark - publish

int startClientWithSessionConfig(AJBMqttClient *client,SeesionConfig *param){
    MqttConfigure *config = ((SeesionConfig *)param)->config;
    MqttDispatcher dispatcher = *((SeesionConfig *)param)->dispatcher;
    
    newAJBMqttClient(client,config->
                     username, config->password, config->clientid,config->cleansession);
    sprintf(client->clientId, "%s%d",config->clientid,((SeesionConfig *)param)->index);
    
    client->keepAlive = config->keepAlive;    //default is 20s
    client->qos = 2;                      //default is QOS2
    client->timout_ms = config->timeout_ms;   //default is 2000ms
    client->aliveAttr = config->aliveAttr;
    client->indexTag = param->index;
    
    MqttLog("%d-%d-%d-%d",client->aliveAttr.auto_con,client->aliveAttr.recon_int,client->aliveAttr.recon_max,client->aliveAttr.reconnecting);
    
    client->dispatcher = dispatcher;
    client->dispatcher.shouldReSubscribe = NULL;
    
    
    mqttClient_setDispatcher(client,dispatcher);
    
    client->keepworking = 1;
    int rc = mqttClient_connect(client,config->host,config->port);
    if (rc!=SUCCESS) {
        goto exit;
    }
    
    char topic[MAX_TOPIC_LEN]={};
    getSuggestTopic(topic, client->clientId);
    mqttClient_subscribe(client, topic, 2);
    
    client->dispatcher.shouldReSubscribe = dispatcher.shouldReSubscribe;
exit:
    return rc;
    
}

void *mqttRecvRunloop(void *param){
    
    int index = ((SeesionConfig *)param)->index;
    
    AJBMqttClient client;
    client.sendBuf = (unsigned char *)malloc(PACKET_BUF_SIZE);
    client.readBuf = (unsigned char *)malloc(PACKET_BUF_SIZE);
    client.c.tmpTopic = (char *)malloc(PACKET_BUF_SIZE);
    client.c.tmpMessage = (char *)malloc(PACKET_BUF_SIZE);

    if(startClientWithSessionConfig(&client,(SeesionConfig *)param) == SUCCESS){
        client.keepRunning(&client);
    }
    else{
        MqttLog("connect session failed,index :    %4d",index);
        logToLocal(index, log_erro_path, "connect session failed,index :    %4d",index)
    }
    free(client.c.tmpTopic);
    free(client.c.tmpMessage);
    free(client.readBuf);
    free(client.sendBuf);
    
    
    return "finished";
}


#pragma mark - main

int main(int argc, const char * argv[]) {
    
    if (access("log", 0) != 0) {
        int status = mkdir("log",0777);
        printf("status ---- %d\n",status);
    }
   
    
    initLock();
    
    signal(SIGINT, cfinish2);
    signal(SIGTERM, cfinish2);
    
#ifdef SIGPIPE
    signal(SIGPIPE, pipeHandle);
#endif
    
    MqttConfigure config = DEFAULT_CONFIG;
    if (argc==1) {
        config_file="/Users/lukai/Desktop/Command-Demo/MqttMutiClient/mqttc/config.cfg";
    }
    else{
        config_file = argv[1];
    }
    get_mqtt_opts(config_file, &config);
    
    MqttDispatcher dispatcher = {};
    dispatcher.onConnect = on_connect;
    dispatcher.onSubscribe = on_subscribe;
    dispatcher.shouldReSubscribe = on_shouldSubscribe;
    dispatcher.onError = on_hasErr;
    dispatcher.onRecevie = on_receive;
    dispatcher.onLoop = on_Loop;
    
    enable_running = 1;
    
    int alive_thread_count = 0;
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * config.sessionCount);
    int i =0;
    for (i =0 ; i<config.sessionCount; i++) {
        if (!enable_running) {
            break;
        }
        SeesionConfig session = {i+config.startIndex,&config,&dispatcher};
        pthread_t thread;
        pthread_create(&thread, NULL, mqttRecvRunloop, &session);
        threads[i] = thread;
        alive_thread_count++;
        usleep(config.startInterval*1000);
    }
    usleep(100*1000);
    printf("start client finished\n");
    void *resutl;
    for (i = 0; i<alive_thread_count; i++) {
        pthread_join(threads[i], &resutl);
    }
    
    printf("\nbegin Merge the log files...\n");
    mergeFiles(10000, log_send_path);
    mergeFiles(10000, log_erro_path);
    mergeFiles(10000, log_file_path);

    usleep(1000000);
    
    return 0;
}
