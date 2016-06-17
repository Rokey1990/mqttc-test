//
//  AJBMqttClient.h
//  LKMqttDemo
//
//  Created by anjubao on 4/12/16.
//  Copyright © 2016 anjubao. All rights reserved.
//

#ifndef AJBMqttClient_h
#define AJBMqttClient_h

#include "MQTTClient.h"

struct AJBMqttClient{
    /**
     *  用户名，选填，默认为空串
     */
    char username[MAX_USERNAME_LEN];
    /**
     *  密码，选填，默认为空串
     */
    char password[MAX_PASSWORD_LEN];
    /**
     *  客户端订阅主题
     */
    char topic[MAX_TOPIC_LEN];
    /**
     *  客户端ID，每个客户端需所对应的唯一ID
     */
    char clientId[MAX_CLIENT_ID_LEN];
    /**
     *  服务器地址，可以为域名和ip
     */
    char host[32];
    /**
     *  服务端端口，默认1883
     */
    int port;
    /**
     *  客户端接收超时时间，默认为2000ms
     */
    short timout_ms;
    /**
     *  客户端心跳时间间隔，默认为20s
     */
    short keepAlive;
    /**
     *  保持连接运行的属性
     */
    KeepAliveAttr aliveAttr;
    /**
     *  publish的消息ID，初始化为随机值
     */
    unsigned short messageId;
    /**
     *  服务质量，默认为QOS2(一次)，可以为QOS1（至少一次）、QOS0（最多一次）
     */
    MqttServiceQos qos;
    /**
     *  重连时是否清理回话状态标志，为0则不清楚，否则清除回话
     */
    char cleanSession;
//    /**
//     *  客户端接收到消息的回调
//     */
//    MqttClientMsgHandler on_recvMessage;
    /**
     *  客户端事件分发器，包含命令请求结果的回调及收到消息的回调
     */
    MqttDispatcher dispatcher;
    /**
     *  回调函数需要引用的数据
     */
    void *usedobj;
    /**
     *  客户端是否处于连接状态，true连接状态，false非连接状态
     */
    char isConnected;
    /**
     *  客户端是否应处于工作状态，keepworking为0时客户端需强制停止runloop(配合多线程选择使用)
     */
    char keepworking;
    /**
     *  客户端遗嘱信息
     */
    MqttClientWillInfo will;
    /**
     *  mqtt客户端
     */
    Client c;
    /**
     *  mqtt网络结构体，依赖于硬件平台
     */
    Network n;
   
    /**
     *  运行runloop，客户端进入持续接收数据的状态
     */
    
    void (*keepRunning)(struct AJBMqttClient *client);
    /**
     *  获取messageid
     */
    int (*getMessageId)(struct AJBMqttClient *client);
    MqttConfigure *config;
    int indexTag;
    
    unsigned char *readBuf;
    unsigned char *sendBuf;
};

typedef struct AJBMqttClient AJBMqttClient;

#pragma mark - mqtt base

/**
 *  连接mqtt客户端到指定服务器,同步接口
 *
 *  @param client mqtt客户端
 *  @param host   需要连接的mqtt服务器的url
 *  @param port   mqtt服务器端口
 *
 *  @return 连接结果，成功返回0，否则返回-1
 */
MqttReturnCode mqttClient_connect(AJBMqttClient *client, char *host,int port);

/**
 *  客户端重连
 *
 *  @param client 需要执行重连操作的客户端
 *
 *  @return 重连结果
 */
MqttReturnCode mqttClient_reconnect(AJBMqttClient *client);

/**
 *  断开与mqtt服务端之间的连接
 *
 *  @param client 需要断开的客户端
 *
 *  @return 返回断开结果，成功返回0，否则返回-1
 */
int            mqttClient_stopRunning(AJBMqttClient *client);
//void           mqttClient_setMessageHandler(AJBMqttClient *client,MqttClientMsgHandler msgHandler);
/**
 *  订阅一个话题，异步调用，订阅成功后调用client->dispatcher.onSubscribe函数
 *
 *  @param client 执行订阅操作的mqtt客户端
 *  @param topic  订阅主题
 *  @param qos    订阅的QOS，可指定为QOS0 QOS1 QOS2
 *
 *  @return 订阅命令是否发送成功，注意：发送成功不代表订阅成功
 */
MqttReturnCode mqttClient_subscribe(AJBMqttClient *client,char *topic,MqttServiceQos qos);

/**
 *  取消订阅一个话题，异步调用，订阅成功后调用client->dispatcher.unSubscribe函数
 *
 *  @param client 执行订阅操作的mqtt客户端
 *  @param topic  订阅主题
 *
 *  @return 取消订阅命令是否发送成功，注意：发送成功不代表订阅成功
 */
MqttReturnCode mqttClient_unsubscribe(AJBMqttClient *client,char *topic);

/**
 *  异步发布消息
 *
 *  @param client      需要发布消息的客户端
 *  @param publishData 发布的消息数据
 *
 *  @return 发布消息结果
 */
MqttReturnCode mqttClient_publish(AJBMqttClient *client,MqttClientPublishInfo *publishData);

/**
 *  同步发布消息
 *
 *  @param client      需要发布消息的客户端
 *  @param publishData 发布的消息数据
 *
 *  @return 发送结果
 */
MqttReturnCode mqttClient_publish2(AJBMqttClient *client,MqttClientPublishInfo *publishData);

/**
 *  设置客户端遗嘱消息，注意:需要放在connect之前调用
 *
 *  @param client  需要设置遗嘱的客户端
 *  @param topic   遗嘱消息的主题
 *  @param content 遗嘱消息的内容
 */
void mqttClient_setWill(AJBMqttClient *client,char *topic,char *content);


#pragma mark - mqtt extension

/**
 *  自动分配的消息Id
 *
 *  @param client 需要获取消息id的客户端
 *
 *  @return 获取到的消息id
 */
int            getMessageId(AJBMqttClient *client);

/**
 *  获取建议的主题，根据与服务端约定，默认为c/[clientId]/info
 *
 *  @param topic    建议的主题c/[clientId]/info
 *  @param clientId 客户端id
 */
void getSuggestTopic(char *topic,char *clientId);


void newAJBMqttClient(AJBMqttClient *client,char *username,char *password,char *clientId,bool cleanSession);

/**
 *  设置客户端消息及请求回调
 *
 *  @param client     设置回调函数的客户端
 *  @param dispatcher 需要设置的回调，参加MqttDispatcher说明
 */
void mqttClient_setDispatcher(AJBMqttClient *client,const MqttDispatcher dispatcher);

/**
 *  启动
 *
 *  @param client 需要启动的客户端
 *  @param config 配置文件路径
 *
 *  @return 返回启动结果，
 */
MqttReturnCode  mqttClient_start(AJBMqttClient *client,char *configFile);

/**
 *  启动一个Mqtt客户端
 *
 *  @param client     需要启动的客户端
 *  @param config     客户端配置信息
 *  @param dispatcher 包含所有回调函数的结构体
 *
 *  @return 启动结果
 */
MqttReturnCode  mqttClient_start2(AJBMqttClient *client,MqttConfigure config,MqttDispatcher dispatcher);
//MqttReturnCode  mqttClient_start3(AJBMqttClient *client,char *configFile);

/**
 *  SDK测试函数
 *
 *  @return 启动是否成功的结果
 */
MqttReturnCode mqttClient_startPub(AJBMqttClient *client,MqttConfigure config,MqttDispatcher dispatcher);

/**
 *  获取配置文件
 *
 *  @param filePath 文件路径
 *  @param config   获取到的config信息
 */
void get_mqtt_opts(const char *filePath,MqttConfigure *config);

void set_disable_subscribe(const char *filePath,char *clientId,int disable);


#endif /* AJBMqttClient_h */
