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

#include "MQTTClient.h"

void NewMessageData(MessageData* md, MQTTString* aTopicName, MQTTMessage* aMessgage) {
    md->topicName = aTopicName;
    md->message = aMessgage;
}


int getNextPacketId(Client *c) {
    return c->next_packetid = (c->next_packetid == MAX_PACKET_ID) ? 1 : c->next_packetid + 1;
}


int sendPacket(Client* c, int length, Timer* timer)
{
    int rc = FAILURE, 
    sent = 0;

    int retryTimes = 0;
    while (sent < length )
    {
        int time_left = expired(timer)?5:left_ms(timer);
        
        rc = c->ipstack->mqttwrite(c->ipstack, &c->buf[sent], length - sent, time_left);
        if (rc <= 0) {
            printf("[ERROR] -------- SEND LENGTH %d (%d)\n",rc,errno);
        }
        if (rc < 0){  // there was an error writing the data
            MqttLog("[WARNING] SEND PACKET FAILED %d",errno);
            break;
        }
        sent += rc;
        if (expired(timer)) {
            if (retryTimes++ >= 3) {
                printf("#############################\n");
                break;
            }
            printf("[IMPORTANT ]send expired,retring!!!\n");
        }
    }
    if (sent == length)
    {
        countdown(&c->ping_timer, c->keepAliveInterval); // record the fact that we have successfully sent the packet    
        rc = SUCCESS;
    }
    else
        rc = FAILURE;

    if (sent != length) {
        logToLocal(c->indexTag, log_erro_path, "send packet %d failed,sent Length %d",c->indexTag,sent);
    }
    return rc;
}


void MQTTClient(Client* c, Network* network, unsigned int command_timeout_ms, unsigned char* buf, size_t buf_size, unsigned char* readbuf, size_t readbuf_size)
{
//    int i;
    c->ipstack = network;
    
//    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
//        c->messageHandlers[i].topicFilter = 0;
    c->command_timeout_ms = command_timeout_ms;
    c->buf = buf;
    c->buf_size = buf_size;
    c->readbuf = readbuf;
    c->readbuf_size = readbuf_size;
    c->isconnected = 0;
    c->ping_outstanding = 0;
//    c->defaultMessageHandler = NULL;
    InitTimer(&c->ping_timer);
}


int decodePacket(Client* c, int* value, int timeout)
{
    unsigned char i;
    int multiplier = 0;
    int len = 0;
    int rc = MQTTPACKET_READ_ERROR;
    const int MAX_NO_OF_REMAINING_LENGTH_BYTES = 4;

    *value = 0;
    do
    {

        if (len > MAX_NO_OF_REMAINING_LENGTH_BYTES)
        {
            rc = MQTTPACKET_READ_ERROR; /* bad data */
            goto exit;
        }
        rc = c->ipstack->mqttread(c->ipstack, &i, 1, timeout);
        if (rc != 1) {
            MqttLog("%s:%d",__func__,rc);
            if (rc == -1 ) {
                printf("%s read -1\n",__func__);
                continue;
            }
            else{
                rc = MQTTPACKET_READ_ERROR;
            }
            goto exit;
        }
        len++;
        *value += (i & 127) << multiplier;
        multiplier += 7;
    } while ((i & 128) != 0);
exit:
    return rc;
}

int validateHeader(MQTTHeader *header){
    int rc = SUCCESS;
    switch (header->bits.type) {
        case 3:{
            if (header->bits.retain == 1 || header->bits.qos==3) {
                rc = FAILURE;
            }
            break;
        }
        case 1:
        case 2:
        case 4:
        case 5:
        case 7:
        case 9:
        case 11:
        case 12:
        case 13:
        case 14:
            if ((header->byte&0x0f)!=0) {
                rc = FAILURE;
            }
            break;
        case 6:
        case 8:
        case 10:{
            if ((header->byte&0x0f)!=2) {
                rc = FAILURE;
            }
        }

        default:
            break;
    }
    if (rc == FAILURE) {
        printf("invalid header %02x ---- %c\n",header->byte,header->byte);
    }

    return rc;
}

int readPacket(Client* c, Timer* timer) 
{
    memset(c->readbuf, '~', 1024);
    int rc = FAILURE;
    MQTTHeader header = {0};
    int len = 0;
    int rem_len = 0;

    /* 1. read the header byte.  This has the packet type in it */
    int readrc = 0;
    int crc = FAILURE;
    int droppedBytes = 0;
    
    do{
        readrc = c->ipstack->mqttread(c->ipstack, c->readbuf, 1, left_ms(timer));
        if (readrc == 0) {
            rc = ERR_PACKET_TYPE;
            goto exit;
        }
        else if (readrc != 1){
            goto exit;
        }
        header.byte = c->readbuf[0];
        crc = validateHeader(&header);
        if (crc==SUCCESS) {
            break;
        }
        else{
            printf("%s read -1\n",__func__);
            logToLocal(c->indexTag, log_erro_path, "unkown header byte ---- %02x",crc);
            return ERR_PACKET_TYPE;
        }
    }while (1);
    if (droppedBytes>0) {
        logToLocal(c->indexTag,log_erro_path, "[MQTT RECEIVE ERROR] dropped bytes:%d",droppedBytes);
    }
    len = 1;
    /* 2. read the remaining length.  This is variable in itself */
    int decrc = decodePacket(c, &rem_len, left_ms(timer));
    if (decrc == MQTTPACKET_READ_ERROR) {
        logToLocal(c->indexTag,log_erro_path, "[MQTTPACKET_READ_ERROR] rem_len : %d",rem_len);
        rc = ERR_PACKET_TYPE;
        goto exit;
    }
   
    len += MQTTPacket_encode(c->readbuf + 1, rem_len); /* put the original remaining length back into the buffer */

    /* 3. read the rest of the buffer using a callback to supply the rest of the data */
    
//    for (int i =0; i<len; i++) {
//        printf("%02x ",c->readbuf[i]);
//    }
//    printf("  len = %d\n",rem_len);
    
    int leftTime = 0;
    
    int readBytes = 0;
    while (readBytes<rem_len) {
        if (expired(timer)) {
            printf("[IMPORTANT] read packet retrying - %d: %d %d\n",c->ipstack->my_socket,readBytes,rem_len);
            leftTime = 100;
        }
        else{
            leftTime = left_ms(timer);
            leftTime = leftTime<100?100:leftTime;
        }
        
        int length = c->ipstack->mqttread(c->ipstack, c->readbuf + len + readBytes, rem_len - readBytes, leftTime);
        if (length < 0) {
            printf("%s read -1\n",__func__);
            logToLocal(c->indexTag, log_erro_path, "[RECV error]%s:length = %d",__func__,length);
            continue;
        }
        else if (length == 0){
            printf("%s read 0\n",__func__);
            logToLocal(c->indexTag, log_erro_path, "[RECV error]%s:length = %d",__func__,0);
            rc = ERR_PACKET_TYPE;
            goto exit;
        }
//        printf("%d-",length);
        readBytes += length;
    }
    if (rem_len > 0 && readBytes != rem_len){
        logToLocal(c->indexTag, log_erro_path, "[RECV] unfinished %d,%d\n",rem_len,readBytes);
//        printf("[IMPORTANT ] read packet error,unfinished\n");
        rc = FAILURE;
        goto exit;
    }
    
//    for (int i =len; i<rem_len+len; i++) {
//        printf("%c",c->readbuf[i]);
//    }
//    printf("\n");
    rc = header.bits.type;
exit:
    return rc;
}


// assume topic filter and name is in correct format
// # can only be at end
// + and # can only be next to separator
char isTopicMatched(char* topicFilter, MQTTString* topicName)
{
    char* curf = topicFilter;
    char* curn = topicName->lenstring.data;
    char* curn_end = curn + topicName->lenstring.len;
    
    while (*curf && curn < curn_end)
    {
        if (*curn == '/' && *curf != '/')
            break;
        if (*curf != '+' && *curf != '#' && *curf != *curn)
            break;
        if (*curf == '+')
        {   // skip until we meet the next separator, or end of string
            char* nextpos = curn + 1;
            while (nextpos < curn_end && *nextpos != '/')
                nextpos = ++curn + 1;
        }
        else if (*curf == '#')
            curn = curn_end - 1;    // skip until end of string
        curf++;
        curn++;
    };
    
    return (curn == curn_end) && (*curf == '\0');
}




int deliverMessage(Client* c, MQTTString* topicName, MQTTMessage* message)
{
//    char messageStr[MAX_CONTENT_LEN];
//    char topic[MAX_TOPIC_LEN];
    int rc = SUCCESS;
    int topicLen = (int)topicName->lenstring.len;
    int msgLen = (int)message->payloadlen;
    
//    for (int i = 0; i<600; i++) {
//        printf("%02x ",c->readbuf[i]);
//    }
//    printf("\n");
//    
//    for (int i = 0; i<600; i++) {
//        printf("%c ",c->readbuf[i]);
//    }
//    printf("\n");
//    exit(0);
    
    if (topicLen>MAX_TOPIC_LEN) {
        
        MqttLog("unknow topic name! len = %d: %d",c->indexTag,topicLen);
        logToLocal(c->indexTag,log_erro_path, "unknow topic name! len = %d",topicLen);
        topicLen = MAX_TOPIC_LEN - 1;
        return FAILURE;
    }
    else{
        memcpy(c->tmpTopic, topicName->lenstring.data, topicLen);
    }
    if (msgLen>MAX_CONTENT_LEN ) {
        
        MqttLog("unknow content ! len = %d",msgLen);
        logToLocal(c->indexTag,log_erro_path, "unknow content ! len = %d",msgLen);
        msgLen = MAX_CONTENT_LEN - 1;
        return FAILURE;
    }
    else{
        memcpy(c->tmpMessage, message->payload, msgLen);
    }
    
    c->tmpTopic[topicLen] = '\0';
    c->tmpMessage[msgLen] = '\0';
//    printf("[RECV (%d)%s] id = %d\n",topicName->lenstring.len,c->tmpTopic,getPubMessageId(c->tmpMessage));
    logToLocal(c->indexTag,log_file_path,"INFO:收到消息--> topic: %s message:%s",c->tmpTopic,c->tmpMessage);
    if (c->dispatcher->onRecevie) {
        c->dispatcher->onRecevie(c->usedObj,topicName->lenstring.data,message->payload,(int)message->payloadlen);
    }
    return rc;
}

int keepalive(Client* c)
{
    int rc = FAILURE;

    if (c->keepAliveInterval == 0)
    {
        rc = SUCCESS;
        goto exit;
    }
    if (expired(&c->ping_timer))
    {
        if (c->ping_outstanding < c->keepAliveInterval + MAX_KEEPALIVE_TIMEO)
        {
            Timer timer;
            InitTimer(&timer);
            countdown_ms(&timer, 3000);
            int len = MQTTSerialize_pingreq(c->buf, c->buf_size);
            int rc = sendPacket(c, len, &timer);
            if (rc!=SUCCESS) {
                logToLocal(c->indexTag, log_erro_path, "client %d send pingreq failed!",c->indexTag);
            }
//            MqttLog("[SEND] PINGREQ");
           
//            if (len > 0 && (rc = sendPacket(c, len, &timer)) == SUCCESS) // send the ping packet
//                c->ping_outstanding = 0;
        }
    }

exit:
    
    return rc;
}

int cycle(Client* c, Timer* timer)
{
    // read the socket, see what work is due
    unsigned short packet_type = readPacket(c, timer);
    if (packet_type==ERR_PACKET_TYPE){
        return SOCK_ERROR;
    }
    else if (packet_type != PACKET_TIMEOUT) {
        c->ping_outstanding = 0;
    }
    int len = 0,
    rc = SUCCESS;
    
    switch (packet_type)
    {
        case CONNACK:
            MqttLog("[CONNECT result] SUCCESS");
            if (c->dispatcher->onConnect) {
                c->dispatcher->onConnect(c->usedObj,SUCCESS);
            }break;
        case PUBACK:
            if (c->dispatcher->onPublish) {
                c->dispatcher->onPublish(c->usedObj,SUCCESS);
            }
            break;
        case SUBACK:{
            MqttLog("[SUB result] SUCCESS");
            unsigned short packetid;
            int count;
            int qoses[100]={};
            if (MQTTDeserialize_suback(&packetid, 100, &count, qoses, c->readbuf, (int)c->readbuf_size)) {
                if (c->dispatcher->onSubscribe)
                    c->dispatcher->onSubscribe(c->usedObj,qoses);
            }
            break;
        }
        case PUBLISH:
        {
            MQTTString topicName;
            MQTTMessage msg;
            int decRc = MQTTDeserialize_publish((unsigned char*)&msg.dup, (int*)&msg.qos, (unsigned char*)&msg.retained, (unsigned short*)&msg.id, &topicName,
                                                (unsigned char**)&msg.payload, (int*)&msg.payloadlen, c->readbuf, (int)c->readbuf_size);
            if (decRc != 1){
                printf("goto exit --- %d",c->indexTag);
                goto exit;
            }
            else{
                MqttLog("[cid %d] msg ---> %d (%d,%d,%d,%d,%d)\n",c->indexTag,decRc,msg.dup,msg.qos,msg.retained,msg.id,topicName.lenstring.len);
            }
            
            deliverMessage(c, &topicName, &msg);
            if (msg.qos != QOS0)
            {
                if (msg.qos == QOS1)
                    len = MQTTSerialize_ack(c->buf, c->buf_size, PUBACK, 0, msg.id);
                else if (msg.qos == QOS2)
                    len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREC, 0, msg.id);
                if (len <= 0){
                    MqttLog("[RECV failed] INVALID PACKET")
                    rc = FAILURE;
                }
                else {
                    rc = sendPacket(c, len, timer);
                    if (rc == FAILURE) {
                        MqttLog("[RECV failed] SEND PUBREC FAILED -------->>>>>>>%d",msg.id);
                    }
                }
                if (rc == FAILURE)
                    goto exit; // there was a problem
            }
            break;
        }
        case PUBREC:
        {
            unsigned short mypacketid = 0;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1){
                MqttLog("[PUB failed] INVALID PUBREC -------->>>>>>>%d",mypacketid);
                rc = FAILURE;
            }
            else if ((len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREL, 0, mypacketid)) <= 0){
                rc = FAILURE;
            }
            else if ((rc = sendPacket(c, len, timer)) != SUCCESS){ // send the PUBREL packet
                 MqttLog("[PUB failed] SEND PUBREL FAILED -------->>>>>>> %d ",mypacketid);
                rc = FAILURE; // there was a problem
            }
            if (rc == FAILURE)
                goto exit; // there was a problem
            break;
        }
        case PUBREL:{
            unsigned short mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1){
                MqttLog("[RECV failed] INVALID PUBREL -------->>>>>>> %d ",mypacketid);
                rc = FAILURE;
            }
            else if ((len = MQTTSerialize_ack(c->buf, c->buf_size, PUBCOMP, 0, mypacketid)) <= 0){
                MqttLog("[RECV failed] INVALID PUBCOMP -------->>>>>>> %d ",mypacketid);
                rc = FAILURE;
            }
            else if ((rc = sendPacket(c, len, timer)) != SUCCESS){ // send the PUBREL packet
                MqttLog("[RECV failed] SEND PUBCOMP FAILED -------->>>>>>> %d ",mypacketid);
                rc = FAILURE; // there was a problem
            }
            if (rc == FAILURE)
                goto exit; // there was a problem
            break;
        }
        case PUBCOMP:{
            unsigned short mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size)) {
//                MqttLog("[PUB result] %d SUCCESS",mypacketid);
            }
            if (c->dispatcher->onPublish) {
                c->dispatcher->onPublish(c->usedObj,SUCCESS);
            }
        }
            break;
        case UNSUBACK:
            MqttLog("[UNSUB result] SUCCESS");
            if (c->dispatcher->unSubscribe) {
                c->dispatcher->unSubscribe(c->usedObj,SUCCESS);
            }
            break;
        case PINGRESP:
//            MqttLog("[RECV] PINGRESP");
            c->ping_outstanding = 0;
            break;
    }
    keepalive(c);
exit:
    if (rc == SUCCESS)
        rc = packet_type;
    return rc;
}


int MQTTYield(Client* c, int timeout_ms)
{
    int rc = SUCCESS;
    Timer timer;

    InitTimer(&timer);    
    countdown_ms(&timer, timeout_ms);
    while (!expired(&timer))
    {
        int cyc_rc = cycle(c, &timer);
        if (cyc_rc == SOCK_ERROR) {
            rc = SOCK_ERROR;
            break;
        }
        if (cyc_rc == FAILURE)
        {
            rc = FAILURE;
            break;
        }
        if (c->ping_outstanding >= c->keepAliveInterval*2) {
            return FAILURE;
        }
    }
    
    return rc;
}


// only used in single-threaded mode where one command at a time is in process
int waitfor(Client* c, int packet_type, Timer* timer)
{
    int rc = FAILURE;
    
    while(!expired(timer)){
        rc = cycle(c, timer);
        if (rc == packet_type) {
            break;
        }
        else if (rc == SOCK_ERROR){
            rc = SOCK_ERROR;
            break;
        }
        else if (rc == MAX_PACKET_ID){
            rc = FAILURE;
        }
    }
    
//    do
//    {
//        if (expired(timer)) 
//            break; // we timed out
//    }
//    while ((rc = cycle(c, timer)) != packet_type);  
    
    return rc;
}


int MQTTConnect(Client* c, MQTTPacket_connectData* options)
{
    Timer connect_timer;
    int rc = FAILURE;
    MQTTPacket_connectData default_options = MQTTPacket_connectData_initializer;
    int len = 0;
    
    InitTimer(&connect_timer);
    countdown_ms(&connect_timer, c->command_timeout_ms);

    if (c->isconnected) // don't send connect packet again if we are already connected
        goto exit;

    if (options == 0)
        options = &default_options; // set default options if none were supplied
    
    c->keepAliveInterval = options->keepAliveInterval;
    countdown(&c->ping_timer, c->keepAliveInterval);
    if ((len = MQTTSerialize_connect(c->buf, c->buf_size, options)) <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &connect_timer)) != SUCCESS)  // send the connect packet
        goto exit; // there was a problem
    
    // this will be a blocking call, wait for the connack
    if (waitfor(c, CONNACK, &connect_timer) == CONNACK)
    {
        unsigned char connack_rc = 255;
        char sessionPresent = 0;
        if (MQTTDeserialize_connack((unsigned char*)&sessionPresent, &connack_rc, c->readbuf, c->readbuf_size) == 1)
            rc = connack_rc;
        else
            rc = FAILURE;
    }
    else
        rc = FAILURE;
    
exit:
    if (rc == SUCCESS)
        c->isconnected = 1;
    return rc;
}


int MQTTSubscribe(Client* c, const char* topicFilter, enum QoS qos, messageHandler messageHandler)
{ 
    int rc = FAILURE;  
    Timer timer;
    int len = 0;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;
    
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    if (!c->isconnected)
        goto exit;
    
    len = MQTTSerialize_subscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic, (int*)&qos);
    if (len <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit;             // there was a problem
    
    if (waitfor(c, SUBACK, &timer) == SUBACK)      // wait for suback 
    {
        printf("message handle\n");
        int count = 0, grantedQoS = -1;
        unsigned short mypacketid;
        if (MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, c->readbuf, c->readbuf_size) == 1)
            rc = grantedQoS; // 0, 1, 2 or 0x80 
//        if (rc != 0x80)
//        {
//            int i;
//            for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
//            {
//                if (c->messageHandlers[i].topicFilter == 0)
//                {
//                    c->messageHandlers[i].topicFilter = topicFilter;
//                    c->messageHandlers[i].fp = messageHandler;
//                    rc = 0;
//                    break;
//                }
//            }
//        }
    }
    else 
        rc = FAILURE;
        
exit:
    return rc;
}

int MQTTSubscribe2(Client* c, const char* topicFilter, enum QoS qos, messageHandler messageHandler)
{
    int rc = FAILURE;
    Timer timer;
    int len = 0;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;
    
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);
    
    if (!c->isconnected)
        return FAILURE;
    
    len = MQTTSerialize_subscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic, (int*)&qos);
    if (len <= 0)
        return FAILURE;
    rc = sendPacket(c, len, &timer); // send the subscribe packet
    
    return rc;
}

int MQTTUnsubscribe(Client* c, const char* topicFilter)
{   
    int rc = FAILURE;
    Timer timer;    
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;
    int len = 0;

    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);
    
    if (!c->isconnected)
        goto exit;
    
    if ((len = MQTTSerialize_unsubscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic)) <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem
    
    if (waitfor(c, UNSUBACK, &timer) == UNSUBACK)
    {
        unsigned short mypacketid;  // should be the same as the packetid above
        if (MQTTDeserialize_unsuback(&mypacketid, c->readbuf, c->readbuf_size) == 1)
            rc = 0; 
    }
    else
        rc = FAILURE;
    
exit:
    return rc;
}
int MQTTUnsubscribe2(Client* c, const char* topicFilter){
    int rc = FAILURE;
    Timer timer;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;
    int len = 0;
    
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);
    
    if (!c->isconnected)
        return rc;
    
    if ((len = MQTTSerialize_unsubscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic)) <= 0)
        return FAILURE;
    rc = sendPacket(c, len, &timer);
    return rc;
}


int MQTTPublish(Client* c, const char* topicName, MQTTMessage* message)
{
    int rc = FAILURE;
    Timer timer;   
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicName;
    int len = 0;

    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);
    
    if (!c->isconnected)
        goto exit;

    if (message->qos == QOS1 || message->qos == QOS2)
        message->id = getNextPacketId(c);
    
    len = MQTTSerialize_publish(c->buf, c->buf_size, 0, message->qos, message->retained, message->id, 
              topic, (unsigned char*)message->payload, message->payloadlen);
    if (len <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem
    
    if (message->qos == QOS1)
    {
        countdown_ms(&timer, c->command_timeout_ms);
        if (waitfor(c, PUBACK, &timer) == PUBACK)
        {
            unsigned short mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;
        }
        else
            rc = FAILURE;
    }
    else if (message->qos == QOS2)
    {
        int retryTimes = 2;/*新增代码*/
    waitCmd:
        countdown_ms(&timer, c->command_timeout_ms);
        int result = waitfor(c, PUBCOMP, &timer);
        if (result == PUBCOMP)
        {
            unsigned short mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;
        }
        else{
            rc = result;
            MqttLog("failture: %d , retryTimes: %d",rc,retryTimes);
            if (rc == FAILURE && retryTimes-- > 0) {
                MqttLog("waiting for PUBCOMP retrying!");
                goto waitCmd;
            }
        }
    }
    
exit:
    return rc;
}

int MQTTPublish2(Client* c, const char* topicName, MQTTMessage* message){
    
    int rc = FAILURE;
    Timer timer;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicName;
    int len = 0;
    
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);
    
    if (!c->isconnected)
        goto exit;
    
    if (message->qos == QOS1 || message->qos == QOS2)
        message->id = getNextPacketId(c);
    
    len = MQTTSerialize_publish(c->buf, c->buf_size, 0, message->qos, message->retained, message->id,
                                topic, (unsigned char*)message->payload, message->payloadlen);
    if (len <= 0){
        goto exit;
    }
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS){ // send the subscribe packet
        MqttLog("[PUB failed] SEND FAILED -------->>>>>>> %d ",message->id);
        goto exit;
    }
exit:
    return rc;
}

int MQTTDisconnect(Client* c)
{  
    int rc = FAILURE;
    Timer timer;     // we might wait for incomplete incoming publishes to complete
    int len = MQTTSerialize_disconnect(c->buf, c->buf_size);

    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    if (len > 0)
        rc = sendPacket(c, len, &timer);            // send the disconnect packet
        
    c->isconnected = 0;
    return rc;
}

#pragma mark - test

/*********************测试代码**********************/

char *dstStr = "num->";
int lkstrcmp(char *str,const char *dst){
    while (*str && *dst) {
        if (*str++ != *dst++) {
            return -1;
        }
    }
    return 0;
}
int getPubMessageId(char *message){
    unsigned long slen = 0;
    slen = strlen(message);
    char *s = message;
    char numStr[10] = {};
    
    char *nst = numStr;
    while (*s!='\0') {
        if (lkstrcmp(s, dstStr)==0) {
            s += strlen(dstStr);
            while ((*s != ' ')&&(*s != ']')) {
                *(nst++) = *(s++);
            }
            return atoi(numStr);
        }
        s++;
    }
    
    return -1;
}
/**************************************************/

