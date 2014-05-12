//ÎÄŒþÃû: server/stcp_server.c
//
//ÃèÊö: ÕâžöÎÄŒþ°üº¬STCP·þÎñÆ÷œÓ¿ÚÊµÏÖ. 
//
//ŽŽœšÈÕÆÚ: 2013Äê1ÔÂ

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
//STCP APIÊµÏÖ
//
/*********************************************************************/

// Õâžöº¯Êý³õÊŒ»¯TCB±í, œ«ËùÓÐÌõÄ¿±êŒÇÎªNULL. Ëü»¹Õë¶ÔTCPÌ×œÓ×ÖÃèÊö·ûconn³õÊŒ»¯Ò»žöSTCP²ãµÄÈ«ŸÖ±äÁ¿, 
// žÃ±äÁ¿×÷Îªsip_sendsegºÍsip_recvsegµÄÊäÈë²ÎÊý. ×îºó, Õâžöº¯ÊýÆô¶¯seghandlerÏß³ÌÀŽŽŠÀíœøÈëµÄSTCP¶Î.
// ·þÎñÆ÷Ö»ÓÐÒ»žöseghandler.
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

// Õâžöº¯Êý²éÕÒ·þÎñÆ÷TCB±íÒÔÕÒµœµÚÒ»žöNULLÌõÄ¿, È»ºóÊ¹ÓÃmalloc()ÎªžÃÌõÄ¿ŽŽœšÒ»žöÐÂµÄTCBÌõÄ¿.
// žÃTCBÖÐµÄËùÓÐ×Ö¶Î¶Œ±»³õÊŒ»¯, ÀýÈç, TCB state±»ÉèÖÃÎªCLOSED, ·þÎñÆ÷¶Ë¿Ú±»ÉèÖÃÎªº¯Êýµ÷ÓÃ²ÎÊýserver_port. 
// TCB±íÖÐÌõÄ¿µÄË÷ÒýÓŠ×÷Îª·þÎñÆ÷µÄÐÂÌ×œÓ×ÖID±»Õâžöº¯Êý·µ»Ø, ËüÓÃÓÚ±êÊ¶·þÎñÆ÷¶ËµÄÁ¬œÓ. 
// Èç¹ûTCB±íÖÐÃ»ÓÐÌõÄ¿¿ÉÓÃ, Õâžöº¯Êý·µ»Ø-1.
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

		p->next_seqNum =rand()%65535;			        //新段准备使用的下一个序号 
		p->sendbufMutex = malloc(sizeof(pthread_mutex_t));	//发送缓冲区互斥量
		if(p->sendbufMutex==NULL){
			printf("heap full!\n");
			exit(-1);
		}
  		pthread_mutex_init(p->sendbufMutex,NULL);
 		p->sendBufHead = NULL; 	    				//发送缓冲区头
  		p->sendBufunSent = NULL;	    			//发送缓冲区中的第一个未发送段
 		p->sendBufTail = NULL;	    				//发送缓冲区尾
 		p->unAck_segNum = 0;            			//已发送但未收到确认段的数量

		p->recvBuf=malloc(RECEIVE_BUF_SIZE);
		if(p->recvBuf==NULL){
			printf("heap full!\n");
			exit(-1);
		}
		p->usedBufLen=0;
		p->recvbufMutex=malloc(sizeof(pthread_mutex_t));
		if(p->recvbufMutex==NULL){
			printf("heap full!\n");
			exit(-1);
		}
		pthread_mutex_init(p->recvbufMutex,NULL);
		tcbTab[i]=p;
		break;
	}
	if(i==MAX_TRANSPORT_CONNECTIONS) return -1;
	return i;
}

// Õâžöº¯ÊýÊ¹ÓÃsockfd»ñµÃTCBÖžÕë, ²¢œ«Á¬œÓµÄstate×ª»»ÎªLISTENING. ËüÈ»ºóÆô¶¯¶šÊ±Æ÷œøÈëÃŠµÈŽýÖ±µœTCB×ŽÌ¬×ª»»ÎªCONNECTED 
// (µ±ÊÕµœSYNÊ±, seghandler»áœøÐÐ×ŽÌ¬µÄ×ª»»). žÃº¯ÊýÔÚÒ»žöÎÞÇîÑ­»·ÖÐµÈŽýTCBµÄstate×ª»»ÎªCONNECTED,  
// µ±·¢ÉúÁË×ª»»Ê±, žÃº¯Êý·µ»Ø1. Äã¿ÉÒÔÊ¹ÓÃ²»Í¬µÄ·œ·šÀŽÊµÏÖÕâÖÖ×èÈûµÈŽý.
int stcp_server_accept(int sockfd) 
{
	server_tcb_t* p=tcbTab[sockfd];
	p->state=LISTENING;
	while(p->state!=CONNECTED) ;
	return 1;
}

// œÓÊÕÀŽ×ÔSTCP¿Í»§¶ËµÄÊýŸÝ. Õâžöº¯ÊýÃ¿žôRECVBUF_POLLING_INTERVALÊ±Œä
// ŸÍ²éÑ¯œÓÊÕ»º³åÇø, Ö±µœµÈŽýµÄÊýŸÝµœŽï, ËüÈ»ºóŽæŽ¢ÊýŸÝ²¢·µ»Ø1. Èç¹ûÕâžöº¯ÊýÊ§°Ü, Ôò·µ»Ø-1.
int stcp_recv(int sockfd, void* buf, unsigned int length) 
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
	pthread_mutex_lock(p->recvbufMutex);
	memcpy(buf,p->recvBuf,length);
	int i;
	for(i=length;i<p->usedBufLen;i++) p->recvBuf[i-length]=p->recvBuf[i];
	p->usedBufLen-=length;
	pthread_mutex_unlock(p->recvbufMutex);
	return 1;
}

// Õâžöº¯Êýµ÷ÓÃfree()ÊÍ·ÅTCBÌõÄ¿. Ëüœ«žÃÌõÄ¿±êŒÇÎªNULL, ³É¹ŠÊ±(ŒŽÎ»ÓÚÕýÈ·µÄ×ŽÌ¬)·µ»Ø1,
// Ê§°ÜÊ±(ŒŽÎ»ÓÚŽíÎóµÄ×ŽÌ¬)·µ»Ø-1.
int stcp_server_close(int sockfd) 
{
	if(tcbTab[sockfd]==NULL){ 
		printf("ERROR:in stcp_server_close()!\n");
		return -1;
	}
	free(tcbTab[sockfd]->recvBuf);
	free(tcbTab[sockfd]->sendbufMutex);
	free(tcbTab[sockfd]->recvbufMutex);
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

// ÕâÊÇÓÉstcp_server_init()Æô¶¯µÄÏß³Ì. ËüŽŠÀíËùÓÐÀŽ×Ô¿Í»§¶ËµÄœøÈëÊýŸÝ. seghandler±»ÉèŒÆÎªÒ»žöµ÷ÓÃsip_recvseg()µÄÎÞÇîÑ­»·, 
// Èç¹ûsip_recvseg()Ê§°Ü, ÔòËµÃ÷µœSIPœø³ÌµÄÁ¬œÓÒÑ¹Ø±Õ, Ïß³Ìœ«ÖÕÖ¹. žùŸÝSTCP¶ÎµœŽïÊ±Á¬œÓËùŽŠµÄ×ŽÌ¬, ¿ÉÒÔ²ÉÈ¡²»Í¬µÄ¶¯×÷.
// Çë²é¿Ž·þÎñ¶ËFSMÒÔÁËœâžü¶àÏžœÚ.
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
					pthread_mutex_lock(t->recvbufMutex);//lock
					if(t->usedBufLen+h->length<RECEIVE_BUF_SIZE){
						memcpy((t->recvBuf+t->usedBufLen),data,h->length);
						t->usedBufLen+=h->length;
						t->expect_seqNum+=h->length;
						temp.header.ack_num=t->expect_seqNum;
						sip_sendseg(sip_conn,t->client_nodeID,&temp);
					}
					else printf("recvBuffer full! packet loss!\n");
					pthread_mutex_unlock(t->recvbufMutex);
				}
				else{ 
					temp.header.ack_num=t->expect_seqNum;
					sip_sendseg(sip_conn,t->client_nodeID,&temp);
				}
			}
			else if(h->type == DATAACK)//接收到ack
        		{
				if(t->sendBufHead==NULL) break;
				pthread_mutex_lock(t->sendbufMutex);
	    			unsigned int affirm = h->ack_num;
      			        segBuf_t *temp;
				printf("1seq_num:%d , affirm:%d \n",t->sendBufHead->seg.header.seq_num,affirm);
				printBuffer(t);
	    			while(t->sendBufHead!=NULL&&t->sendBufHead->seg.header.seq_num < affirm)
	    			{
				//printf("2seq_num:%d , affirm:%d \n",t->sendBufHead->seg.header.seq_num,affirm);
	 			        temp = t->sendBufHead;
	      				t->sendBufHead = t->sendBufHead->next;
	      				free(temp);
					t->unAck_segNum--;
	      				if(t->sendBufHead == NULL)//如果缓冲区中所有的段都被成功接收
	      				{
						t->sendBufTail = NULL;
						t->sendBufunSent=NULL;
						printf("send buffer empty!\n");
						pthread_join(timer,NULL);
						break;
				        }
				}
				struct timeval starttime;
	    			while(t->sendBufunSent!=NULL&&t->unAck_segNum < GBN_WINDOW)
				{
     					sip_sendseg(sip_conn,t->server_nodeID,&t->sendBufunSent->seg);
      					gettimeofday(&starttime,0);
      					t->sendBufunSent->sentTime = starttime.tv_sec * 1000000 + starttime.tv_usec;
      					t->unAck_segNum ++;
      					t->sendBufunSent = t->sendBufunSent->next;
    				}
				pthread_mutex_unlock(t->sendbufMutex);
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


