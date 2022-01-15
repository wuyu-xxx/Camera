#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>

#include "ser_start.h"
int ser_start(int sockfd)
{	
	/*1.监听套接字的创建*/
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1)
	{
		printf("socket");
		return -1;
	}

	int val = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR , &val, sizeof(val));    	 //设置ip地址和端口的复用

	/*2.套接字的绑定*/
	struct sockaddr_in  seraddr, cliaddr;

	bzero(&seraddr, sizeof(seraddr));
	seraddr.sin_family = AF_INET;
	//seraddr.sin_addr.s_addr = inet_addr("192.168.2.199"); //inet_addr("0.0.0.0");
	seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	seraddr.sin_port = htons(8888);


	socklen_t len = sizeof(seraddr);

	if(bind(sockfd, (struct sockaddr*)&seraddr, len) == -1)
	{
		perror("bind failed");
		return -1;
	}

	/*3.监听套接字*/
	if(listen(sockfd , 1024) == -1)
	{
		perror("listen failed");
		return -1;
	}

	printf("listen----success\n");
	/*4.接收客户端的请求.请求成功，返回通信套接字*/

	int connfd = accept(sockfd, (struct sockaddr*)&cliaddr, &len);
	if(connfd == -1)
	{
		perror("accept");
		return -1;
	}

	printf("accept----success\n");
	return connfd;
}