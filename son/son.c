//文件名: son/son.c
//
//描述: 这个文件实现SON进程
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程. 
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中.  
//
//创建日期: 2013年1月

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "neighbortable.h"

//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 15

/**************************************************************/
//声明全局变量
/**************************************************************/
int hostNodeID;		//本机NodeID
//将邻居表声明为一个全局变量 
nbr_entry_t* nt; 
//将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn; 

/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止.
void* waitNbrs(void* arg) {
	//你需要编写这里的代码.
	int listenfd,connfd;
	socklen_t nbr_addr_len;
	struct sockaddr_in nbr_addr,son_addr;
	//创建监听套接字
	if((listenfd = socket(AF_INET,SOCK_STREAM,0)) <0) 
	{
		printf("Problem in creating the socket\n");
		pthread_exit(NULL);
	}
	//填充套接字地址
	memset(&nbr_addr,0,sizeof(nbr_addr));
	memset(&son_addr,0,sizeof(son_addr));
	son_addr.sin_family = AF_INET;
	son_addr.sin_addr.s_addr = topology_getIP(hostNodeID);
	son_addr.sin_port = htons(CONNECTION_PORT);
	//绑定监听套接字
	bind(listenfd,(struct sockaddr *)&son_addr,sizeof(son_addr));
	listen(listenfd,8);		
	
	//没有邻居时退出
	int nbr_num = topology_getNbrNum();	
	if(nbr_num <= 0)
		pthread_exit(NULL);
	//找到邻居表中有多少项比本机NodeID大
	int accept_num = 0;
	int i;
	for(i=0;i<nbr_num;i++)
		if(nt[i].nodeID > hostNodeID)
			accept_num++;
	//等待邻居进入连接,没有需要等待的邻居时，退出
	nbr_addr_len = sizeof(nbr_addr);
	int nbr_NodeID;
	printf("accept_num: %d\n",accept_num);
	//开始接收来自邻居的连接
	while(accept_num > 0)
	{
		connfd = accept(listenfd,(struct sockaddr *)&nbr_addr,&nbr_addr_len);
		nbr_NodeID = topology_getNodeIDfromip(&nbr_addr.sin_addr);
		//printf("nbr_NodeID: %d\n",nbr_NodeID);
		if(nbr_NodeID > hostNodeID)
		{
			nt_addconn(nt,nbr_NodeID,connfd);
			accept_num --;		
		}
	}
	pthread_exit(NULL);
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {
	//你需要编写这里的代码.
	int n = topology_getNbrNum();
	int i;
	for(i = 0; i<n; i++)
	{
		if(nt[i].nodeID < hostNodeID)
		{	
			int sockfd;
			if((sockfd =  socket (AF_INET,SOCK_STREAM,0))<0) {
				printf("Problem in creating the socket\n");
				return -1;
			}

			struct sockaddr_in nbr_addr;	
			memset(&nbr_addr,0,sizeof(nbr_addr));
			nbr_addr.sin_family = AF_INET;
			nbr_addr.sin_port = htons(CONNECTION_PORT);
			nbr_addr.sin_addr.s_addr = topology_getIP(nt[i].nodeID);
			//printf("IP:%d \n",nbr_addr.sin_addr.s_addr);
			printf("connect to Nbr %d\n",nt[i].nodeID);
			if(connect(sockfd,(struct sockaddr *) &nbr_addr,sizeof(nbr_addr))<0) {
				printf("fail to connect to  Nbr %d\n",nt[i].nodeID);
				return -1;
			}
			nt_addconn(nt,nt[i].nodeID,sockfd);
		}
	}
  	return 1;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的.
void* listen_to_neighbor(void* arg) {
	//你需要编写这里的代码.
	//接收来自邻居的报文错误时，终止线程
	int conn = nt[*((int*)arg)].conn;
	sip_pkt_t pkt;
	printf("listen to Nbr %d conn: %d\n",nt[*((int*)arg)].nodeID,conn);
	//while(1) {
		while(recvpkt(&pkt, conn)>=0) {
			printf("prepare forwardpktToSIP\n");
			forwardpktToSIP(&pkt,sip_conn);
		}
	//}
	printf("listen to Nbr finish\n");
	pthread_exit(NULL);
  return 0;
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP() {
	//你需要编写这里的代码.
	//接收来自SIP层的报文错误时，终止线程
	int listenfd;
	socklen_t sip_addr_len;
	struct sockaddr_in sip_addr,son_addr;
	//创建监听套接字
	if((listenfd = socket(AF_INET,SOCK_STREAM,0)) <0) {
		printf("Problem in creating the socket\n");
		return ;
	}
	//填充套接字地址
	memset(&sip_addr,0,sizeof(sip_addr));
	memset(&son_addr,0,sizeof(son_addr));
	son_addr.sin_family = AF_INET;
	son_addr.sin_addr.s_addr = topology_getIP(hostNodeID);
	son_addr.sin_port = htons(SON_PORT);
	//绑定监听套接字
	bind(listenfd,(struct sockaddr *)&son_addr,sizeof(son_addr));
	listen(listenfd,8);
	sip_addr_len = sizeof(sip_addr);
	//建立与SIP层的连接
	printf("son wait sip to connect!\n");
	sip_conn = accept(listenfd,(struct sockaddr *)&sip_addr,&sip_addr_len);
	printf("sip connect!\n");
		
	sip_pkt_t pkt;
	int nextNode;
	int n = topology_getNbrNum();
	while(getpktToSend(&pkt,&nextNode,sip_conn)>0)
	{
		printf("dest%d src:%d\n",pkt.header.dest_nodeID,pkt.header.src_nodeID);
		//printPacket((seg_t*)(pkt.data));
		if(nextNode != BROADCAST_NODEID)
		{	
			int i = 0;
			while((i < n) && (nt[i].nodeID != nextNode))
				i++;
			if(i == n)
				printf("No such nodeID: %d\n",nextNode);	
			else
				sendpkt(&pkt,nt[i].conn);
		}
		else//广播
		{
			int i = 0;
			while(i < n)
			{
				sendpkt(&pkt,nt[i].conn);
				i++;		
			}
		}
	}
	printf("waitSIP stop!\n");
	pthread_exit(NULL);
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() {
	//你需要编写这里的代码.
	//你需要编写这里的代码.
	close(sip_conn);
	sip_conn = -1;
	int nbrNum = topology_getNbrNum();
	int index;	
	for(index = 0; index<nbrNum; index++)
		close(nt[index].conn);
	nt_destroy(nt);
}

int main() {
	//启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n",topology_getMyNodeID());
	
	//获取本机NodeID	
	hostNodeID = topology_getMyNodeID();
	if(hostNodeID == -1) return -1;	

	//创建一个邻居表
	nt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接.
	sip_conn = -1;
	
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);

	//打印所有邻居
	int nbrNum = topology_getNbrNum();
	int i;
	for(i=0;i<nbrNum;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}

	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread,NULL,waitNbrs,(void*)0);

	//等待其他节点启动
	sleep(SON_START_DELAY);
	
	//连接到节点ID比自己小的所有邻居
	connectNbrs();

	//等待waitNbrs线程返回
	pthread_join(waitNbrs_thread,NULL);	

	//此时, 所有与邻居之间的连接都建立好了
	
	//创建线程监听所有邻居
	for(i=0;i<nbrNum;i++) {
		int* idx = (int*)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread,NULL,listen_to_neighbor,(void*)idx);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");

	//等待来自SIP进程的连接
	waitSIP();
}
