//
//  MqttDispatcher.h
//  MqttCommand
//
//  Created by anjubao on 5/16/16.
//  Copyright © 2016 anjubao. All rights reserved.
//

#ifndef MqttDispatcher_h
#define MqttDispatcher_h

struct MQTTMessage;

//typedef void (*MqttClientMsgHandler)(void *client,struct MQTTMessage *message);
typedef void (*MqttClientOnConnect)(void *client,int code);
//typedef void (*MqttClientOnDisconnect)(void *client,int code);
typedef void (*MqttClientOnRecevieMsg)(void *client,char *topic,void *message,int len);
typedef void (*MqttClientOnSubscribe)(void *client,int *qoses);
typedef void (*MqttClientOnUnsubscribe)(void *client,int code);
typedef void (*MqttClientOnPublish)(void *client,int code);
typedef void (*MqttClientHasError)(void *client);
typedef void (*MqttClientShouldReSubscribe)(void *client);
typedef void (*MqttClientOnLoop)(void *client);

typedef struct {
    /**
     *  Mqtt客户端连接成功回调
     */
    MqttClientOnConnect     onConnect;

//    MqttClientOnDisconnect  onDisconnect;
    /**
     *  发布消息成功的回调
     */
    MqttClientOnPublish     onPublish;
    
    /**
     *  接收到publish消息的回调
     */
    MqttClientOnRecevieMsg  onRecevie;
    
    /**
     *  订阅主题成功回调
     */
    MqttClientOnSubscribe   onSubscribe;
    
    /**
     *  取消订阅成功回调
     */
    MqttClientOnUnsubscribe unSubscribe;
    
    /**
     *  客户端发生socket异常或其他异常的回调
     */
    MqttClientHasError      onError;
    
    /**
     *  客户端重连时，需要重新订阅主题的回调(如设定cleanSession为1时，
     *  重连需要重新订阅方能继续接收对应主题的消息)
     */
    MqttClientShouldReSubscribe   shouldReSubscribe;
    
    MqttClientOnLoop onLoop;
}MqttDispatcher;

#endif /* MqttDispatcher_h */
