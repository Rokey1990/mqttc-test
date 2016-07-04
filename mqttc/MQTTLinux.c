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
 *    Allan Stockdill-Mander - initial API and implementation and/or initial documentation
 *******************************************************************************/



#include "MQTTLinux.h"

#if PLATFORM_LINUX==1

char expired(Timer* timer)
{
	struct timeval now, res;
	gettimeofday(&now, NULL);
	timersub(&timer->end_time, &now, &res);		
	return res.tv_sec < 0 || (res.tv_sec == 0 && res.tv_usec <= 0);
}


void countdown_ms(Timer* timer, unsigned int timeout)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	struct timeval interval = {timeout / 1000, (timeout % 1000) * 1000};
	timeradd(&now, &interval, &timer->end_time);
}


void countdown(Timer* timer, unsigned int timeout)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	struct timeval interval = {timeout, 0};
	timeradd(&now, &interval, &timer->end_time);
}


int left_ms(Timer* timer)
{
	struct timeval now, res;
	gettimeofday(&now, NULL);
	timersub(&timer->end_time, &now, &res);
	//printf("left %d ms\n", (res.tv_sec < 0) ? 0 : res.tv_sec * 1000 + res.tv_usec / 1000);
	return (res.tv_sec < 0) ? 0 : res.tv_sec * 1000 + res.tv_usec / 1000;
}


void InitTimer(Timer* timer)
{
	timer->end_time = (struct timeval){0, 0};
}


int linux_read(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
    
    struct timeval interval = {timeout_ms/1000, (timeout_ms%1000)*1000};
    
    if (setsockopt(n->my_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&interval, sizeof(struct timeval)) < 0) {
        printf("-----------------------------set recvtimeout error\n");
    }

	int bytes = 0;
    int retryTimes = 0;
	while (bytes < len)
	{
		int rc = recv(n->my_socket, buffer+bytes, (size_t)(len - bytes), 0);
		if (rc == -1)
		{
            if (bytes>0) {
                printf("%s receive -1\n",__func__);
                continue;
            }
            bytes = -1;
            break;
		}
        else if (rc ==0){
            printf("%s receive 0 -- %d -- %d %d %d\n",__func__,errno,n->my_socket,bytes,len);
            return 0;
        }
		else
			bytes += rc;
	}
	return bytes;
}


int linux_write(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
	struct timeval tv;

	tv.tv_sec = 0;  /* 30 Secs Timeout */
	tv.tv_usec = timeout_ms * 1000;  // Not init'ing this can cause strange errors

	setsockopt(n->my_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv,sizeof(struct timeval));
	int	rc = write(n->my_socket, buffer, len);
	return rc;
}


void linux_disconnect(Network* n)
{
	close(n->my_socket);
}


void NewNetwork(Network* n)
{
	n->my_socket = 0;
	n->mqttread = linux_read;
	n->mqttwrite = linux_write;
	n->disconnect = linux_disconnect;
    pthread_mutex_init(&n->mutex, NULL);
}


int ConnectNetwork(Network* n, char* addr, int port)
{
	int type = SOCK_STREAM;
	struct sockaddr_in address;
	int rc = -1;
	sa_family_t family = AF_INET;
	struct addrinfo *result = NULL;
	struct addrinfo hints = {0, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, 0, NULL, NULL, NULL};

	if ((rc = getaddrinfo(addr, NULL, &hints, &result)) == 0)
	{
		struct addrinfo* res = result;

		/* prefer ip4 addresses */
		while (res)
		{
			if (res->ai_family == AF_INET)
			{
				result = res;
				break;
			}
			res = res->ai_next;
		}

		if (result->ai_family == AF_INET)
		{
			address.sin_port = htons(port);
			address.sin_family = family = AF_INET;
			address.sin_addr = ((struct sockaddr_in*)(result->ai_addr))->sin_addr;
		}
		else
			rc = -1;

		freeaddrinfo(result);
	}

	if (rc == 0)
	{
		n->my_socket = socket(family, type, 0);
		if (n->my_socket != -1)
		{
//            struct timeval cTimeOut = {5,0};
//            if(setsockopt(n->my_socket,IPPROTO_TCP,0x20,(char *)&cTimeOut, sizeof( struct timeval))<0)
//            {
//                printf("hello,set connect timeout error!\n");
//                return -1;
//            }
			rc = connect(n->my_socket, (struct sockaddr*)&address, sizeof(address));
            if (rc == 0) {
#ifdef SO_NOSIGPIPE
                int set = 1;
                if (setsockopt(n->my_socket, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set))<0) {
                    perror("set no sigpipe error");
                }
#endif
                int bufSize = 100*1024;
                if (setsockopt(n->my_socket, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize))>=0) {
//                    printf("set send buffer size ------ %d\n",bufSize);
                }
                if (setsockopt(n->my_socket, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize))>=0) {
//                    printf("set recevie buffer size ------ %d\n",bufSize);
                }
            }
            
            
		}
	}

	return rc;
}

#endif