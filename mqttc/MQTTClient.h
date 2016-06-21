/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander/Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

#ifndef __MQTT_CLIENT_C_
#define __MQTT_CLIENT_C_

#include "MqttDispatcher.h"
#include <stdio.h>

#include "MqttDefines.h"

#if PLATFORM_LINUX == 1
#include "MQTTLinux.h" //Platform specific implementation header file
#include <pthread.h>
#else
#include "MQTTCC3200.h" //Platform specific implementation header file
#endif



// all failure return codes must be negative
enum returnCode { SOCK_ERROR = -3,BUFFER_OVERFLOW = -2, FAILURE = -1, SUCCESS = 0 };

void NewTimer(Timer*);

typedef struct MQTTMessage MQTTMessage;

typedef struct MessageData MessageData;

struct MQTTMessage
{
    enum QoS qos;
    char retained;
    char dup;
    unsigned short id;
    void *payload;
    size_t payloadlen;
};

struct MessageData
{
    MQTTMessage* message;
    MQTTString* topicName;
};

typedef void (*messageHandler)(MessageData*);

typedef struct Client Client;

int MQTTConnect (Client*, MQTTPacket_connectData*);
int MQTTPublish (Client*, const char*, MQTTMessage*);
int MQTTPublish2(Client* c, const char*, MQTTMessage*);/*异步接口*/
int MQTTSubscribe (Client*, const char*, enum QoS, messageHandler);
int MQTTSubscribe2 (Client*, const char*, enum QoS, messageHandler);/*异步接口*/
int MQTTUnsubscribe (Client*, const char*);
int MQTTUnsubscribe2 (Client*, const char*);/*异步接口*/
int MQTTDisconnect (Client*);
int MQTTYield (Client*, int);

//测试代码
int getPubMessageId(char *message);

void MQTTClient(Client*, Network*, unsigned int, unsigned char*, size_t, unsigned char*, size_t);

struct Client {
    unsigned int next_packetid;
    unsigned int command_timeout_ms;
    size_t buf_size, readbuf_size;
    unsigned char *buf;  
    unsigned char *readbuf; 
    unsigned int keepAliveInterval;
    float ping_outstanding;
    int isconnected;

//    struct MessageHandlers
//    {
//        const char* topicFilter;
//        void (*fp) (MessageData*);
//    } messageHandlers[MAX_MESSAGE_HANDLERS];      // Message handlers are indexed by subscription topic
//    void (*defaultMessageHandler) (MessageData*);
    
    void *usedObj;
    MqttDispatcher *dispatcher;
    
    Network* ipstack;
    Timer ping_timer;
    int indexTag;
    
    char *tmpTopic;
    char *tmpMessage;
};

#define DefaultClient {0, 0, 0, 0, NULL, NULL, 0, 0, 0}

#endif
