//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程  
//
//创建日期: 2013年1月

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "sip.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"

//SIP层等待这段时间让SIP路由协议建立路由路径. 
#define SIP_WAITTIME 15

/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 			//到重叠网络的连接
int stcp_conn;			//到STCP的连接
nbr_cost_entry_t* nct;			//邻居代价表
dv_t* dv;				//距离矢量表
pthread_mutex_t* dv_mutex;		//距离矢量表互斥量
routingtable_t* routingtable;		//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT.
//成功时返回连接描述符, 否则返回-1.
int connectToSON() { 
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

	servaddr.sin_port = htons(SON_PORT);	//convert to big-endian order
	
	//Connection of the client to the socket
	if(connect(sockfd,(struct sockaddr *) &servaddr,sizeof(servaddr))<0) {
		printf("Can Not Connect To Server!\n");	
		return -1;
	}
	printf("sip connect son\n");
	return sockfd;
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.路由更新报文包含这个节点
//的距离矢量.广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
void* routeupdate_daemon(void* arg) {
 	int id=topology_getMyNodeID();
	sip_pkt_t pkt;
	while(1){
		pkt.header.src_nodeID=id;
		pkt.header.dest_nodeID=BROADCAST_NODEID;
		pkt.header.type=ROUTE_UPDATE;
		pkt_routeupdate_t up;
		int count=0;
		pthread_mutex_lock(dv_mutex);
		dv_t* head=dv;
		while(head!=NULL) if(head->nodeID==id){
			memcpy(&(up.entry[count]),head->dvEntry,sizeof(routeupdate_entry_t));
			count++;
			head=head->next;	
		}
		pthread_mutex_unlock(dv_mutex);
		up.entryNum=count;
		pkt.header.length=sizeof(pkt_routeupdate_t);
		memcpy(pkt.data,&up,sizeof(pkt_routeupdate_t));
		son_sendpkt(BROADCAST_NODEID,&pkt,son_conn);
		sleep(ROUTEUPDATE_INTERVAL);
	}
  	return 0;
}

//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表.
void* pkthandler(void* arg) {
	int id=topology_getMyNodeID();
	while(1){
		sip_pkt_t pkt;
		sip_hdr_t* h=&(pkt.header);
		while(son_recvpkt(&pkt,son_conn)!=1) ;
		if(h->type==SIP){
			if(h->dest_nodeID==id)	{ 
				forwardsegToSTCP(stcp_conn,h->src_nodeID, (seg_t*)&(pkt.data));
				printf("rcv from son!\n");
				printPacket((seg_t*)&(pkt.data));		
			}
			else{
				pthread_mutex_lock(routingtable_mutex);
				int next=routingtable_getnextnode(routingtable,h->dest_nodeID);
				pthread_mutex_unlock(routingtable_mutex);
				son_sendpkt(next, &pkt,son_conn);
				printf("switch packet!\n");
				printPacket((seg_t*)&(pkt.data));
			}	
		}
		else if(h->type==ROUTE_UPDATE){
			pthread_mutex_lock(routingtable_mutex);
			pthread_mutex_lock(dv_mutex);
			pkt_routeupdate_t* up=(pkt_routeupdate_t*)pkt.data;
			int i,base=dvtable_getcost(dv,id,h->src_nodeID);
			for(i=0;i<up->entryNum;i++){
				routeupdate_entry_t p=up->entry[i];
				int current=dvtable_getcost(dv,id,p.nodeID);
				if(current>base+p.cost){
					assert(p.nodeID!=id);
					dvtable_setcost(dv,id,p.nodeID,base+p.cost);
					routingtable_setnextnode(routingtable,p.nodeID, h->src_nodeID);				
				}
			}
			pthread_mutex_unlock(dv_mutex);
			pthread_mutex_unlock(routingtable_mutex);
		}
		else printf("pkt type fault!\n");
	}
	//你需要编写这里的代码.
  return 0;
}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop() {
	//你需要编写这里的代码.
	routingtable_destroy(routingtable);
	dvtable_destroy(dv);
	nbrcosttable_destroy(nct);
	free(dv_mutex);
	free(routingtable_mutex);
	close(stcp_conn);
	close(son_conn);
	son_conn = -1;
	stcp_conn=-1;
	//pthread_join(pkt_handler_thread,NULL);
	//pthread_join(routeupdate_thread,NULL);
	return;
}

//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t. 
//接收的段被封装进数据报(一个段在一个数据报中), 然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.
void waitSTCP() {
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
	son_addr.sin_addr.s_addr=topology_getIP(topology_getMyNodeID());
	son_addr.sin_port = htons(SIP_PORT);
	//绑定监听套接字
	bind(listenfd,(struct sockaddr *)&son_addr,sizeof(son_addr));
	listen(listenfd,8);
	sip_addr_len = sizeof(sip_addr);
	//建立与STCP层的连接
	while(1){
		printf("sip wait stcp to connect!\n");
		stcp_conn = accept(listenfd,(struct sockaddr *)&sip_addr,&sip_addr_len);
		printf("stcp connect!\n");
		seg_t seg;
		int destNode;
		while(getsegToSend(stcp_conn,&destNode,&seg)>0)
		{
			sip_pkt_t pkt;
			sip_hdr_t* h=&(pkt.header);
			h->src_nodeID=topology_getMyNodeID();
			h->dest_nodeID=destNode;
			h->length=sizeof(seg_t);
			h->type=SIP;
			printf("snd to son!\n");
			printPacket(&seg);
			memcpy(pkt.data,&seg,sizeof(seg_t));
			pthread_mutex_lock(routingtable_mutex);
			int next=routingtable_getnextnode(routingtable, destNode);
			pthread_mutex_unlock(routingtable_mutex);
			if(next==-1){
				printf("con't find routinfo!\n");
			}
			else son_sendpkt(next,&pkt,son_conn);
		}
		printf("STCP close,wait next!\n");
	}
  	return;
}

int main(int argc, char *argv[]) {
	printf("SIP layer is starting, pls wait...\n");

	//初始化全局变量
	nct = nbrcosttable_create();
	dv = dvtable_create();
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
	son_conn = -1;
	stcp_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到本地SON进程 
	son_conn = connectToSON();
	if(son_conn<0) {
		printf("can't connect to SON process\n");
		exit(1);		
	}
	
	//启动线程处理来自SON进程的进入报文 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("SIP layer is started...\n");
	printf("waiting for routes to be established\n");
	sleep(SIP_WAITTIME);
	routingtable_print(routingtable);

	//等待来自STCP进程的连接
	printf("waiting for connection from STCP process\n");
	waitSTCP(); 

}


