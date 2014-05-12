//�ļ���: server/stcp_server.c
//
//����: ����ļ�����STCP�������ӿ�ʵ��. 
//
//��������: 2013��1��

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
//STCP APIʵ��
//
/*********************************************************************/

// ���������ʼ��TCB��, ��������Ŀ���ΪNULL. �������TCP�׽���������conn��ʼ��һ��STCP���ȫ�ֱ���, 
// �ñ�����Ϊsip_sendseg��sip_recvseg���������. ���, �����������seghandler�߳�����������STCP��.
// ������ֻ��һ��seghandler.
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

// ����������ҷ�����TCB�����ҵ���һ��NULL��Ŀ, Ȼ��ʹ��malloc()Ϊ����Ŀ����һ���µ�TCB��Ŀ.
// ��TCB�е������ֶζ�����ʼ��, ����, TCB state������ΪCLOSED, �������˿ڱ�����Ϊ�������ò���server_port. 
// TCB������Ŀ������Ӧ��Ϊ�����������׽���ID�������������, �����ڱ�ʶ�������˵�����. 
// ���TCB����û����Ŀ����, �����������-1.
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

// �������ʹ��sockfd���TCBָ��, �������ӵ�stateת��ΪLISTENING. ��Ȼ��������ʱ������æ�ȴ�ֱ��TCB״̬ת��ΪCONNECTED 
// (���յ�SYNʱ, seghandler�����״̬��ת��). �ú�����һ������ѭ���еȴ�TCB��stateת��ΪCONNECTED,  
// ��������ת��ʱ, �ú�������1. �����ʹ�ò�ͬ�ķ�����ʵ�����������ȴ�.
int stcp_server_accept(int sockfd) 
{
	server_tcb_t* p=tcbTab[sockfd];
	p->state=LISTENING;
	while(p->state!=CONNECTED) ;
	return 1;
}

// ��������STCP�ͻ��˵�����. �������ÿ��RECVBUF_POLLING_INTERVALʱ��
// �Ͳ�ѯ���ջ�����, ֱ���ȴ������ݵ���, ��Ȼ��洢���ݲ�����1. ����������ʧ��, �򷵻�-1.
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

// �����������free()�ͷ�TCB��Ŀ. ��������Ŀ���ΪNULL, �ɹ�ʱ(��λ����ȷ��״̬)����1,
// ʧ��ʱ(��λ�ڴ����״̬)����-1.
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

// ������stcp_server_init()�������߳�. �������������Կͻ��˵Ľ�������. seghandler�����Ϊһ������sip_recvseg()������ѭ��, 
// ���sip_recvseg()ʧ��, ��˵����SIP���̵������ѹر�, �߳̽���ֹ. ����STCP�ε���ʱ����������״̬, ���Բ�ȡ��ͬ�Ķ���.
// ��鿴�����FSM���˽����ϸ��.
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


