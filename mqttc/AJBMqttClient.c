//
//  AJBMqttClient.c
//  LKMqttDemo
//
//  Created by anjubao on 4/12/16.
//  Copyright © 2016 anjubao. All rights reserved.
//

#include "AJBMqttClient.h"
#include <string.h>
#define strlen(str) strlen((char *)str)
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

void auto_reconnect(AJBMqttClient *client);

#define MessageMake publishMessage

static char *errRea[4] = {"keepalive timeout","socket invalid","MQTT EXCEPTION","UNKNOWN"};

#pragma mark - client func
char *errReason(int errCode){
    if (errCode == SUCCESS) {
        return errRea[0];
    }
    else if (errCode == FAILURE){
        return errRea[2];
    }
    else if (errCode == SOCK_ERROR){
        return errRea[1];
    }
    else{
        return errRea[3];
    }
}

void mqttClient_keepRunning(AJBMqttClient *client){
    client->keepworking = 1;
    while (client->isConnected && client->keepworking) {
        int rc = MQTTYield(&client->c, client->timout_ms);
        client->c.ping_outstanding += client->timout_ms/1000.0;
        
        if (rc!=SUCCESS || client->c.ping_outstanding > client->keepAlive+MAX_KEEPALIVE_TIMEO) {
            
            MqttLog("[ERR network]:   %s ---> %s",errReason(rc),client->clientId);
            logToLocal(client->indexTag,log_erro_path, "[ERR network]:   %s ---> %s",errReason(rc),client->clientId);
            if (rc==FAILURE) {
                continue;
            }
            client->n.disconnect(&client->n);
            client->isConnected = false;
            if (client->dispatcher.onError) {
                client->dispatcher.onError(client);
            }
            auto_reconnect(client);
        }
        if (client->dispatcher.onLoop) {
            client->dispatcher.onLoop(client);
        }
    }
    if (client->isConnected) { //多线程下，stop时，若客户端处于重连状态，则有可能客户端在stop之后连接成功，需要stop掉
        mqttClient_stopRunning(client);
    }
}

int reportOnline(AJBMqttClient *client){
    usleep(200*1000);
    MqttClientPublishInfo info = MqttPublishInfoIniter;
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    char state[MAX_CONTENT_LEN]={};
    sprintf(state,"{\"type\":\"online\",\"time\":\"%ld.%d\",\"msg\":{\"clientId\":\"%s\",\"status\":\"online\"}}",now.tv_sec,now.tv_usec,client->clientId);
    char topic[MAX_TOPIC_LEN]={};
    sprintf(topic, "c/%s/status",client->clientId);
    info.publishTopic = topic;
    info.publishContent = state;
    mqttClient_publish2(client, &info);
    return 0;
}

MqttReturnCode mqttClient_connect(AJBMqttClient *client, char *host,int port){
    
    int rc = 0;//return code of mqtt function
    
    if (host != client->host) {
        strcpy(client->host, host);
    }
    
    client->port = port;
    NewNetwork(&client->n);
    
    int interval = client->aliveAttr.recon_int;
    interval = interval>0?interval:1;
    int max = client->aliveAttr.recon_max;
    client->c.indexTag = client->indexTag;
reconnect:
    rc = ConnectNetwork(&client->n, host, port);
//    MqttLog("connect result -------> %d",rc);
    MQTTClient(&client->c, &client->n, client->timout_ms, client->sendBuf, PACKET_BUF_SIZE, client->readBuf, PACKET_BUF_SIZE);
    
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    
    //    data.willFlag = 0;
    data.MQTTVersion = 4;
    data.clientID.cstring = client->clientId;
    data.username.cstring = client->username;
    data.password.cstring = client->password;
    data.keepAliveInterval = client->keepAlive;
    data.cleansession = client->cleanSession;
    
    if (client->will.willFlag) {
        data.willFlag = client->will.willFlag;
        data.will.message.cstring = client->will.willContent;
        data.will.qos = 2;
        data.will.retained = client->will.retain;
        data.will.topicName.cstring = client->will.willTopic;
    }
    MqttLog("[CONNECT request] %s:%d",host,port);
    rc = MQTTConnect(&client->c, &data);
    
    client->isConnected = (rc==SUCCESS);
    
    if(rc == SUCCESS){
        printf("%s,%16s:%d,%4d ----------------ok\n",client->clientId,host,port,client->cleanSession);
        reportOnline(client);
    }
    else{
        client->n.disconnect(&client->n);
        if (client->dispatcher.onLoop) {
            client->dispatcher.onLoop(client);
        }
        MqttLog("[CONNECT failed] %s:%d",host,port);
        logToLocal(client->indexTag,log_erro_path,"[CONNECT failed] %s:%d ---> %s %d",host,port,client->clientId,client->keepworking);
        if (client->keepworking) {
            if (client->aliveAttr.auto_con && ((client->aliveAttr.recon_max==0) || (--max >0)) ) {
                sleep(interval);
                goto reconnect;
            }
        }
    }
    
    return rc;
}


MqttReturnCode mqttClient_reconnect(AJBMqttClient *client){
    logToLocal(client->indexTag,log_erro_path,"[CONNECT reconnect] %s:%d ---> %s",client->host,client->port,client->clientId);
    return mqttClient_connect(client, client->host, client->port);
}

void auto_reconnect(AJBMqttClient *client){
    if (client->keepworking && client->aliveAttr.auto_con && (client->aliveAttr.reconnecting == 0)) {
        client->aliveAttr.reconnecting = 1;
        int max = client->aliveAttr.recon_max;
        int interval = client->aliveAttr.recon_int;
        interval = interval>0?interval:1;
        sleep(interval);
        MqttLog("[RECONNECT %s] left times: %d",client->clientId,max);
        MqttReturnCode rc = mqttClient_reconnect(client);
        if (rc==MqttReturnCodeSuccess) {
            if (client->dispatcher.shouldReSubscribe==NULL && client->cleanSession) {
                MqttLog("[ERR %s] the callback for resubscribe must be implenmented!",client->clientId);
            }
            else{
                client->dispatcher.shouldReSubscribe(client);
            }
            client->aliveAttr.reconnecting = 0;
        }
    }
    else{
        //         MqttLog("[RECONECT disabled] reason: %s",client->aliveAttr.auto_con==0?"not allowed":"already reconnecting");
    }
}

int mqttClient_stopRunning(AJBMqttClient *client){
    
    int rc = FAILURE;
    if (client->isConnected && client->keepworking==0) {
        MqttLog("[DISCONNECT %s]",client->clientId);
        client->isConnected = false;
        rc = MQTTDisconnect(&client->c);
        client->n.disconnect(&client->n);
    }
    client->keepworking = 0;
    return rc;
}

//void mqttClient_setMessageHandler(AJBMqttClient *client,MqttClientMsgHandler msgHandler){
//    client->on_recvMessage = msgHandler;
//}


MQTTMessage publishMessage(MqttClientPublishInfo *publishData,unsigned short messageId){
    MQTTMessage message;
    message.payload = publishData->publishContent;
    message.retained = publishData->retain;
    message.payloadlen = strlen(message.payload);
    message.dup = publishData->dup;
    message.qos = publishData->qos;
    message.id = messageId;
    return message;
}

MqttReturnCode mqttClient_publish(AJBMqttClient *client,
                                  MqttClientPublishInfo *data){
    MQTTMessage message = MessageMake(data, client->getMessageId(client));
    int rc = MQTTPublish2(&client->c, data->publishTopic, &message);
    logToLocal(client->indexTag,log_send_path,"INFO:发布消息--> topic: %s message:%s",data->publishTopic,data->publishContent);
    return rc;
}


MqttReturnCode mqttClient_publish2(AJBMqttClient *client,
                                   MqttClientPublishInfo *data){
    
    MQTTMessage message = MessageMake(data, client->getMessageId(client));
    int msgId = getPubMessageId(data->publishContent);
    if (msgId != -1) {
        printf("[PUB %s] id = %d SEND ------->>>>>>>",data->publishTopic,msgId);
    }
    
    int rc = MQTTPublish(&client->c, data->publishTopic, &message);
    
    
    if (rc == SOCK_ERROR) {
        MqttLog("[PUB failed ] publish message failed,lost connect!");
        MqttLog("[ERR network] %s",errReason(rc));
        logToLocal(client->indexTag,log_erro_path, "[ERR network] %s",errReason(rc));
        mqttClient_reconnect(client);
    }
    else if (rc == SUCCESS){
        if (msgId!=-1) {
            MqttLog("[PUB result] %d SUCCESS",msgId);
        }
        
        logToLocal(client->indexTag,log_send_path,"INFO:发布消息--> topic: %s message: %s",data->publishTopic,data->publishContent);
    }
    else{
        MqttLog("[PUB warning] -------- %d TIMEOUT",getPubMessageId(data->publishContent));
        
    }
    return rc;
}

void mqttClient_setWill(AJBMqttClient *client,char *topic,char *content){
    if (topic){
        strcpy(client->will.willTopic, topic);
    }
    else{
        MqttLog("set will failed, topic cannot be null");return;
    }
    if (content){
        strcpy(client->will.willContent, content);
    }
    else{
        MqttLog("set will failed, content cannot be null");return;
    }
    client->will.qos = 2;
    client->will.retain = 0;
    client->will.willFlag = 1;
}

MqttReturnCode mqttClient_subscribe(AJBMqttClient *client,char *topic,MqttServiceQos qos){
    int rc = MqttReturnCodeFailed;
    if (strlen(topic) == 0) {
        MqttLog("[SUB error] invalid topic");
        return rc;
    }
    rc = MQTTSubscribe2(&client->c, topic, qos, NULL);
    MqttLog("[SUB %s]: %d",topic,rc);
    return rc;
}

MqttReturnCode mqttClient_unsubscribe(AJBMqttClient *client,char *topic){
    return MQTTUnsubscribe2(&client->c, topic);
}

int getMessageId(AJBMqttClient *client){
    client->messageId++;
    return client->messageId;
}

void newAJBMqttClient(AJBMqttClient *client,char *username,char *password,char *clientId,char cleanSession){
    if (username)
        strcpy(client->username, username);
    if (password)
        strcpy(client->password, password);
    strcpy(client->clientId, clientId);
    client->timout_ms = 2000;
    client->keepAlive = 20;
    client->qos = 2;
    client->cleanSession = cleanSession&0x01;
    client->keepRunning = mqttClient_keepRunning;
    client->getMessageId = getMessageId;
    client->keepworking = 1;
    
    //设置遗嘱消息
    client->will.willFlag = 1;
    char will[MAX_CONTENT_LEN]={};
    sprintf(will,"{\"type\":\"offline\",\"msg\":{\"clientId\":\"%s\",\"status\":\"offline\"}}",clientId);
    strcpy(client->will.willContent, will);
    char topic[MAX_TOPIC_LEN]={};
    sprintf(topic, "c/%s/status",clientId);
    strcpy(client->will.willTopic, topic);
    client->will.qos=2;
    client->will.retain = 0;
    
    
}

void mqttClient_setDispatcher(AJBMqttClient *client,MqttDispatcher dispather){
    client->c.usedObj = client;
    client->dispatcher = dispather;
    client->c.dispatcher = &client->dispatcher;
}

void getSuggestTopic(char *topic,char *clientId){
    sprintf(topic,"c/%s/info",clientId);
}


MqttReturnCode  mqttClient_start(AJBMqttClient *client,char *configFile){
    
    MqttConfigure config = DEFAULT_CONFIG;
    get_mqtt_opts(configFile, &config);
    return mqttClient_start2(client, config, client->dispatcher);
}
MqttReturnCode  mqttClient_start2(AJBMqttClient *client,MqttConfigure config,MqttDispatcher dispatcher){
    newAJBMqttClient(client,config.username, config.password, config.clientid,config.cleansession);
    
    client->keepAlive = config.keepAlive;    //default is 20s
    client->qos = 2;                      //default is QOS2
    client->timout_ms = config.timeout_ms;   //default is 2000ms
    client->aliveAttr = config.aliveAttr;
    //    MqttLog("%d-%d-%d-%d",client->aliveAttr.auto_con,client->aliveAttr.recon_int,client->aliveAttr.recon_max,client->aliveAttr.reconnecting);
    
    //    setWill(&client, "c/test1234567890/info", "this is a will");
    client->dispatcher = dispatcher;
    client->dispatcher.shouldReSubscribe = NULL;
    
    
    mqttClient_setDispatcher(client,dispatcher);
    
    int raw = client->keepworking;
    client->keepworking = 1;
    mqttClient_connect(client,config.host,config.port);
    client->keepworking = raw;
    
    char topic[MAX_TOPIC_LEN]={};
    getSuggestTopic(topic, client->clientId);
    if ((!config.disable_subscribe) || (client->cleanSession!=0)) {
        mqttClient_subscribe(client, topic, 2);
    }
    else{
        MqttLog("[SUB %s] skip subscribe",client->clientId);
    }
    
    //    mqttClient_subscribe(client, "c/lixiao-0612/info", 2);
    dispatcher.shouldReSubscribe = dispatcher.shouldReSubscribe;
    //    client->keepRunning(client);
    
    return 0;
}


#pragma mark - 测试函数

void testPublish (AJBMqttClient *client){
    MqttClientPublishInfo info = MqttPublishInfoIniter;
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    char state[MAX_CONTENT_LEN]={};
    sprintf(state,"{time:%ld.%d,clientid:%s}",now.tv_sec,now.tv_usec,client->clientId);
    char topic[MAX_TOPIC_LEN]={};
    sprintf(topic, "c/%s/status",client->clientId);
    info.publishTopic = client->topic;
    info.publishContent = state;
    mqttClient_publish(client, &info);
}


MqttReturnCode mqttClient_startPub(AJBMqttClient *client,MqttConfigure config,MqttDispatcher dispatcher){
    
    newAJBMqttClient(client,config.username, config.password, config.clientid,config.cleansession);
    
    client->keepAlive = config.keepAlive;    //default is 20s
    client->qos = 2;                      //default is QOS2
    client->timout_ms = config.timeout_ms;   //default is 2000ms
    client->aliveAttr = config.aliveAttr;
    
    MqttLog("%d-%d-%d-%d",client->aliveAttr.auto_con,client->aliveAttr.recon_int,client->aliveAttr.recon_max,client->aliveAttr.reconnecting);
    
    client->dispatcher = dispatcher;
    client->dispatcher.shouldReSubscribe = NULL;
    
    
    mqttClient_setDispatcher(client,dispatcher);
    
    client->keepworking = 1;
    mqttClient_connect(client,config.host,config.port);
    //    client->keepworking = 0;
    strcpy(client->topic, config.topic);
    
    dispatcher.shouldReSubscribe = dispatcher.shouldReSubscribe;
    printf("%s\n",client->clientId);
    return 0;
}

#pragma mark - configure options process

void getvalue(char *name,char *value,char *str){
    char *vs = value;
    while (*str != '\0') {
        *name++ = *str++;
        if (*str == '='){
            if (*str==' ') {
                str++;
                continue;
            }
            str++;
            break;
        }
    }
    while (*str != '\0') {
        if (*str==' '&&vs==value) {
            str++;
            continue;
        }
        if (*str=='/'&&(*(str+1)=='/' || *(str+1)=='*')) {
            break;
        }
        if(*str==',')
            break;
        *value++ = *str++;
    }
    *name = '\0';
    *value = '\0';
}

void get_mqtt_opts(const char *filePath,MqttConfigure *config){
    
    printf("---------------config file---------------\n");
    FILE *fp = fopen(filePath, "rt");
    if(fp==NULL){
        MqttLog("[ERR ] no config file");
        exit(1);
        return;
    }
    size_t size;
    char str[1000] = {};
    size = fread(str, 1000, 1, fp);
    char *newstr = strtok(str, "\n");
    
    char name[100],value[100];
    while (newstr != NULL) {
        if (newstr[0] != ';') {
            getvalue(name,value, newstr);
            
            if (strcmp(name, "client_id")==0) {
                strcpy(config->clientid, value);
            }
            else if (strcmp(name, "topic")==0) {
                strcpy(config->topic, value);
            }
            else if (strcmp(name, "qos")==0) {
                config->qos = atoi(value);
            }
            else if (strcmp(name, "host")==0) {
                strcpy(config->host, value);
            }
            else if (strcmp(name, "port")==0) {
                config->port = atoi(value);
            }
            else if (strcmp(name, "username")==0) {
                strcpy(config->username, value);
            }
            else if (strcmp(name, "password")==0) {
                strcpy(config->password, value);
            }
            else if (strcmp(name, "keepalive")==0) {
                config->keepAlive = atoi(value);
            }
            else if (strcmp(name, "timeout")==0){
                config->timeout_ms = atoi(value);
            }
            else if (strcmp(name, "cleansession")==0) {
                config->cleansession = atoi(value);
            }
            else if (strcmp(name, "auto_reconnect")==0){
                config->aliveAttr.auto_con = atoi(value);
            }
            else if (strcmp(name, "recon_interval")==0){
                config->aliveAttr.recon_int = atoi(value);
            }
            else if (strcmp(name, "recon_maxcount")==0){
                config->aliveAttr.recon_max = atoi(value);
            }
            
            else if (strcmp(name, "pub_interval")==0){
                config->pubInterval_ms = atoi(value);
            }
            else if (strcmp(name, "pub_count")==0){
                config->pubCount = atoi(value);
            }
            else if (strcmp(name, "log_file_path")==0){
                strcpy(config->log_file, value);
            }
            else if (strcmp(name, "disable_subscribe")==0){
                if(strcmp(value, config->clientid) == 0){
                    config->disable_subscribe = 1;
                }
            }
            else if (strcmp(name, "start_count")==0){
                config->sessionCount = atoi(value);
            }
            else if (strcmp(name, "start_interval")==0){
                config->startInterval = atoi(value);
            }
            else if (strcmp(name, "start_index")==0){
                config->startIndex = atoi(value);
            }
            else{
                
            }
            printf("%s=%s\n",name,value);
        }
        
        newstr = strtok(NULL, "\n");
    }
    printf("-----------------------------------------\n");
}
void set_disable_subscribe(const char *filePath,char *clientId,int disable){
    FILE *fp = fopen(filePath, "a+");
    if (fp) {
        
        fprintf(fp, "\ndisable_subscribe=%s",clientId);
        fclose(fp);
    }
}


