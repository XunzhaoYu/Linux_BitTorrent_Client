//文件名: server/app_simple_server.c

//描述: 这是简单版本的服务器程序代码. 服务器首先连接到本地SIP进程. 然后它调用stcp_server_init()初始化STCP服务器. 
//它通过两次调用stcp_server_sock()和stcp_server_accept()创建2个套接字并等待来自客户端的连接. 服务器然后接收来自两个连接的客户端发送的短字符串. 
//最后, 服务器通过调用stcp_server_close()关闭套接字, 并断开与本地SIP进程的连接.

//创建日期: 2013年1月

//输入: 无

//输出: STCP服务器状态

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "../common/constants.h"
#include "../topology/topology.h"
#include "stcp_server.h"

//创建两个连接, 一个使用客户端端口号87和服务器端口号88. 另一个使用客户端端口号89和服务器端口号90.
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90
//在接收到字符串后, 等待15秒, 然后关闭连接.
#define WAITTIME 15

//这个函数连接到本地SIP进程的端口SIP_PORT. 如果TCP连接失败, 返回-1. 连接成功, 返回TCP套接字描述符, STCP将使用该描述符发送段.
int connectToSIP() {

	int sockfd;
	struct sockaddr_in servaddr;
	//Create a socket for the client
	//If socketd<0 there was an error in the creation of the socket
	if((sockfd = socket (AF_INET,SOCK_STREAM,0))<0) {
		printf("Problem in creating the socket\n");
		return -1;
	}
	
	//Creation of the socket
	memset(&servaddr,0,sizeof(servaddr));
	servaddr.sin_family = AF_INET;

	servaddr.sin_addr.s_addr=topology_getIP(topology_getMyNodeID());

	servaddr.sin_port = htons(SIP_PORT);	//convert to big-endian order
	
	//Connection of the client to the socket
	if(connect(sockfd,(struct sockaddr *) &servaddr,sizeof(servaddr))<0) {
		printf("Can Not Connect To Server!\n");	
		return -1;
	}
	printf("STCP to SIP Build\n");
	return sockfd;
	
}

//这个函数断开到本地SIP进程的TCP连接. 
void disconnectToSIP(int sip_conn) {

	close(sip_conn);
	return ;
	
}

int main() {
	//用于丢包率的随机数种子
	srand(time(NULL));

	//连接到SIP进程并获得TCP套接字描述符
	sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("can not connect to the local SIP process\n");
	}

	//初始化STCP服务器
	stcp_server_init(sip_conn);

	//在端口SERVERPORT1上创建STCP服务器套接字 
	int sockfd= stcp_server_sock(SERVERPORT1);
	if(sockfd<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//监听并接受来自STCP客户端的连接 
	stcp_server_accept(sockfd);

	//在端口SERVERPORT2上创建另一个STCP服务器套接字
	int sockfd2= stcp_server_sock(SERVERPORT2);
	if(sockfd2<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//监听并接受来自STCP客户端的连接 
	stcp_server_accept(sockfd2);

	char buf1[6];
	char buf2[7];
	int i;
	//接收来自第一个连接的字符串
	for(i=0;i<5;i++) {
		stcp_recv(sockfd,buf1,6);
		printf("recv string: %s from connection 1\n",buf1);
	}
	//接收来自第二个连接的字符串
	for(i=0;i<5;i++) {
		stcp_recv(sockfd2,buf2,7);
		printf("recv string: %s from connection 2\n",buf2);
	}

	sleep(WAITTIME);

	//关闭STCP服务器 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				
	if(stcp_server_close(sockfd2)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				

	//断开与SIP进程之间的连接
	disconnectToSIP(sip_conn);
}
