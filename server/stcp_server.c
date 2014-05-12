//文件名: server/stcp_server.c
//
//描述: 这个文件包含STCP服务器接口实现. 
//
//创建日期: 2013年1月

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/select.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <assert.h>
#include "stcp_server.h"
#include "../topology/topology.h"
#include "../common/constants.h"


/*********************************************************************/
//
//STCP API实现
//
/*********************************************************************/

// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对TCP套接字描述符conn初始化一个STCP层的全局变量, 
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
void stcp_server_init(int conn) 
{
	memset(tcbTab,0,sizeof(tcbTab));
	sip_conn=conn;
	if(pthread_create(&handler,NULL,seghandler,NULL)){
		printf("ERROR:in stcp_server_init()!\n");
		exit(-1);	
	}
  	return;
}

// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
int stcp_server_sock(unsigned int server_port) 
{
	int i;
	for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++) if(tcbTab[i]==NULL){
		server_tcb_t* p=malloc(sizeof(server_tcb_t));
		if(p==NULL){
			printf("heap full!\n");
			exit(-1);
		}
		unsigned int ip=topology_getMyNodeID();
		if(!ip){
			printf("ip not found!\n");
			return -1;
		}
		p->server_nodeID=ip;
		p->server_portNum=server_port;
		p->state=CLOSED;
		p->recvBuf=malloc(RECEIVE_BUF_SIZE);
		if(p->recvBuf==NULL){
			printf("heap full!\n");
			exit(-1);
		}
		p->usedBufLen=0;
		p->bufMutex=malloc(sizeof(pthread_mutex_t));
		if(p->bufMutex==NULL){
			printf("heap full!\n");
			exit(-1);
		}
		pthread_mutex_init(p->bufMutex,NULL);
		tcbTab[i]=p;
		break;
	}
	if(i==MAX_TRANSPORT_CONNECTIONS) return -1;
	return i;
}

// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后启动定时器进入忙等待直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
int stcp_server_accept(int sockfd) 
{
	server_tcb_t* p=tcbTab[sockfd];
	p->state=LISTENING;
	while(p->state!=CONNECTED) ;
	return 1;
}

// 接收来自STCP客户端的数据. 这个函数每隔RECVBUF_POLLING_INTERVAL时间
// 就查询接收缓冲区, 直到等待的数据到达, 它然后存储数据并返回1. 如果这个函数失败, 则返回-1.
int stcp_server_recv(int sockfd, void* buf, unsigned int length) 
{
	server_tcb_t* p=tcbTab[sockfd];
	while(1){
		if(p->usedBufLen>=length) break;
		long long int timeuse = 0;
		struct timeval starttime,endtime;
		gettimeofday(&starttime,0);
		while(timeuse < RECVBUF_POLLING_INTERVAL){
      			gettimeofday(&endtime,0);
      			timeuse =1000000*(endtime.tv_sec - starttime.tv_sec) + endtime.tv_usec - starttime.tv_usec;
			timeuse/=1000000;
    		}
	}
	pthread_mutex_lock(p->bufMutex);
	memcpy(buf,p->recvBuf,length);
	int i;
	for(i=length;i<p->usedBufLen;i++) p->recvBuf[i-length]=p->recvBuf[i];
	p->usedBufLen-=length;
	pthread_mutex_unlock(p->bufMutex);
	return 1;
}

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
int stcp_server_close(int sockfd) 
{
	if(tcbTab[sockfd]==NULL){ 
		printf("ERROR:in stcp_server_close()!\n");
		return -1;
	}
	free(tcbTab[sockfd]->recvBuf);
	free(tcbTab[sockfd]->bufMutex);
	free(tcbTab[sockfd]);
	tcbTab[sockfd]=NULL;
	return 1;
}

void *closeWait(void* arg)
{
	sleep(CLOSEWAIT_TIMEOUT);
	((server_tcb_t*)arg)->state=CLOSED;
	((server_tcb_t*)arg)->usedBufLen=0;
	pthread_detach(pthread_self());
	return NULL;
}

// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明到SIP进程的连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
void* seghandler(void* arg) 
{
	seg_t seg;
	int nodeID;
	while(1){
		while(sip_recvseg(sip_conn,&nodeID,&seg));
		printf("receive packet:\n");
		printPacket(&seg);
		stcp_hdr_t* hdr=&(seg.header);
		server_tcb_t* tcb;
		int i;
		for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++) if(tcbTab[i]!=NULL){
			//printf("port:%d\n",hdr->dest_port);
			if(tcbTab[i]->server_portNum==hdr->dest_port&&
			   tcbTab[i]->client_portNum==hdr->src_port&&
			   tcbTab[i]->client_nodeID==nodeID
			){
				tcb=tcbTab[i];
			//	printf("tcb:%d\n",tcb->server_portNum);
				break;			
			}
		}	
		if(i==MAX_TRANSPORT_CONNECTIONS){
			for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++) if(tcbTab[i]!=NULL){
				//printf("port:%d\n",hdr->dest_port);
				if(tcbTab[i]->server_portNum==hdr->dest_port){
					tcb=tcbTab[i];
					tcb->client_nodeID=nodeID;
				//	printf("tcb:%d\n",tcb->server_portNum);
					break;			
				}
			}
			if(i==MAX_TRANSPORT_CONNECTIONS)
				printf("ERROR:tcb not found!\n");
			else serverFSM(tcb,hdr,seg.data);
		}
		else
			serverFSM(tcb,hdr,seg.data);
	}
	return 0;
}

void serverFSM(server_tcb_t* t,stcp_hdr_t* h,char* data)
{
	seg_t temp;
	temp.header.length=0;
	switch(t->state){
		case CLOSED:
			printf("ERROR:CLOSED state fault!\n");
			break;
		case LISTENING:
			if(h->type==SYN){
				temp.header.src_port=h->dest_port;
				temp.header.dest_port=h->src_port;
				temp.header.type=SYNACK;
				sip_sendseg(sip_conn,t->client_nodeID,&temp);
				t->state=CONNECTED;
				t->expect_seqNum=h->seq_num;
				t->client_portNum=h->src_port;
				printf("server port: %d receive SYN from client port:%d.\n",t->server_portNum,t->client_portNum);
			}
			else printf("ERROR:LISTENING state fault!\n");
			break;
		case CONNECTED:
			if(h->type==SYN){
				temp.header.src_port=h->dest_port;
				temp.header.dest_port=h->src_port;
				temp.header.type=SYNACK;
				sip_sendseg(sip_conn,t->client_nodeID,&temp);
			}
			else if(h->type==FIN){
				printf("server port: %d receive FIN from client port:%d.\n",t->server_portNum,t->client_portNum);
				temp.header.src_port=h->dest_port;
				temp.header.dest_port=h->src_port;
				temp.header.type=FINACK;
				sip_sendseg(sip_conn,t->client_nodeID,&temp);
				//printf("send FINACK:\n");
				//printPacket(&temp);
				t->state=CLOSEWAIT;
				pthread_t thread;
				if(pthread_create(&thread,NULL,closeWait,(void*)t)){
					printf("ERROR:create thread!\n");
					exit(-1);
				}
			}
			else if(h->type==DATA){
				
				temp.header.src_port=h->dest_port;
				temp.header.dest_port=h->src_port;
				temp.header.type=DATAACK;
				if(t->expect_seqNum==h->seq_num){
					pthread_mutex_lock(t->bufMutex);//lock
					if(t->usedBufLen+h->length<RECEIVE_BUF_SIZE){
						memcpy((t->recvBuf+t->usedBufLen),data,h->length);
						t->usedBufLen+=h->length;
						t->expect_seqNum+=h->length;
						temp.header.ack_num=t->expect_seqNum;
						sip_sendseg(sip_conn,t->client_nodeID,&temp);
					}
					else printf("recvBuffer full! packet loss!\n");
					pthread_mutex_unlock(t->bufMutex);
				}
				else{ 
					temp.header.ack_num=t->expect_seqNum;
					sip_sendseg(sip_conn,t->client_nodeID,&temp);
				}
			}
			else printf("ERROR:CONNECTED state fault!\n");
			break;
		case CLOSEWAIT:
			if(h->type==FIN){
				//printf("server port: %d receive FIN from client port:%d.\n",t->server_portNum,t->client_portNum);
				temp.header.src_port=h->dest_port;
				temp.header.dest_port=h->src_port;
				temp.header.type=FINACK;
				sip_sendseg(sip_conn,t->client_nodeID,&temp);
				//printf("send FINACK:\n");
				//printPacket(&temp);
				printf("server port: %d receive FIN from client port:%d.\n",t->server_portNum,t->client_portNum);
			}
			else printf("ERROR:CLOSEWAIT state fault!\n");
			break;
		default: printf("ERROR:unexpected state!\n");
	}
}

/*unsigned char ip_search(void)
{
    int sfd, intr;
    struct ifreq buf[16];
    struct ifconf ifc;
    sfd = socket (AF_INET, SOCK_DGRAM, 0); 
    if (sfd < 0)
        return 0;
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = (caddr_t)buf;
    if (ioctl(sfd, SIOCGIFCONF, (char *)&ifc))
        return 0;
    intr = ifc.ifc_len / sizeof(struct ifreq);
    while (intr-- > 0 && ioctl(sfd, SIOCGIFADDR, (char *)&buf[intr]));
    close(sfd);
    return (buf[intr].ifr_addr-> sin_addr).S_un_b.s_b1;
}*/


