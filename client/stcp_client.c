//ÎÄŒþÃû: client/stcp_client.c
//
//ÃèÊö: ÕâžöÎÄŒþ°üº¬STCP¿Í»§¶ËœÓ¿ÚÊµÏÖ 
//
//ŽŽœšÈÕÆÚ: 2013Äê1ÔÂ

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <assert.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include "../topology/topology.h"
#include "stcp_client.h"
#include "../common/seg.h"


/*********************************************************************/
//
//STCP APIÊµÏÖ
//
/*********************************************************************/

void printBuffer(client_tcb_t* t)
{
	printf("<buffer>\n");
	printf("start seg seqNum:%d\n...\n",t->sendBufHead->seg.header.seq_num);
	printf("end seg seqNum:%d\n",t->sendBufTail->seg.header.seq_num);
	printf("<buffer>\n");
}


// Õâžöº¯Êý³õÊŒ»¯TCB±í, œ«ËùÓÐÌõÄ¿±êŒÇÎªNULL.  
// Ëü»¹Õë¶ÔTCPÌ×œÓ×ÖÃèÊö·ûconn³õÊŒ»¯Ò»žöSTCP²ãµÄÈ«ŸÖ±äÁ¿, žÃ±äÁ¿×÷Îªsip_sendsegºÍsip_recvsegµÄÊäÈë²ÎÊý.
// ×îºó, Õâžöº¯ÊýÆô¶¯seghandlerÏß³ÌÀŽŽŠÀíœøÈëµÄSTCP¶Î. ¿Í»§¶ËÖ»ÓÐÒ»žöseghandler.
void stcp_client_init(int conn) 
{
    	memset(clientTcbTab,0,sizeof(clientTcbTab));
  	sip_conn = conn;
  	if(pthread_create(&handler,NULL,seghandler,NULL))
  	{
		printf("ERROR:in stcp_client_init()!\n");
		exit(-1);	
  	}
  	return;
}

// Õâžöº¯Êý²éÕÒ¿Í»§¶ËTCB±íÒÔÕÒµœµÚÒ»žöNULLÌõÄ¿, È»ºóÊ¹ÓÃmalloc()ÎªžÃÌõÄ¿ŽŽœšÒ»žöÐÂµÄTCBÌõÄ¿.
// žÃTCBÖÐµÄËùÓÐ×Ö¶Î¶Œ±»³õÊŒ»¯. ÀýÈç, TCB state±»ÉèÖÃÎªCLOSED£¬¿Í»§¶Ë¶Ë¿Ú±»ÉèÖÃÎªº¯Êýµ÷ÓÃ²ÎÊýclient_port. 
// TCB±íÖÐÌõÄ¿µÄË÷ÒýºÅÓŠ×÷Îª¿Í»§¶ËµÄÐÂÌ×œÓ×ÖID±»Õâžöº¯Êý·µ»Ø, ËüÓÃÓÚ±êÊ¶¿Í»§¶ËµÄÁ¬œÓ. 
// Èç¹ûTCB±íÖÐÃ»ÓÐÌõÄ¿¿ÉÓÃ, Õâžöº¯Êý·µ»Ø-1.
int stcp_client_sock(unsigned int client_port) 
{
  	int index = 0;
  	//获取第一个NULL条目
  	while(clientTcbTab[index] != NULL)
   	 index ++;
  	if(index == MAX_TRANSPORT_CONNECTIONS) 
    		return -1;
  	//创建新的TCB条目
  	clientTcbTab[index] = (client_tcb_t*)malloc(sizeof(client_tcb_t));
  	if(clientTcbTab[index] == NULL)
  	{
		printf("heap full!\n");
		exit(-1);
	  }
	  //初始化TCB
	unsigned int ip=topology_getMyNodeID();
	if(!ip){
		printf("%s\n",ERRORIP);
		return -1;
	}
	clientTcbTab[index]->client_nodeID=ip;
	clientTcbTab[index]->client_portNum = client_port;			//客户端端口号
	clientTcbTab[index]->state = CLOSED;   	   				//客户端状态

	clientTcbTab[index]->next_seqNum =rand()%65535;			        //新段准备使用的下一个序号 
	clientTcbTab[index]->sendbufMutex = malloc(sizeof(pthread_mutex_t));	//发送缓冲区互斥量
	if(clientTcbTab[index]->sendbufMutex==NULL){
		printf("heap full!\n");
		exit(-1);
	}
  	pthread_mutex_init(clientTcbTab[index]->sendbufMutex,NULL);
 	clientTcbTab[index]->sendBufHead = NULL; 	    			//发送缓冲区头
  	clientTcbTab[index]->sendBufunSent = NULL;	    			//发送缓冲区中的第一个未发送段
 	clientTcbTab[index]->sendBufTail = NULL;	    			//发送缓冲区尾
 	clientTcbTab[index]->unAck_segNum = 0;            			//已发送但未收到确认段的数量

	clientTcbTab[index]->recvBuf=malloc(RECEIVE_BUF_SIZE);
		if(clientTcbTab[index]->recvBuf==NULL){
			printf("heap full!\n");
			exit(-1);
		}
	clientTcbTab[index]->usedBufLen=0;
	clientTcbTab[index]->recvbufMutex=malloc(sizeof(pthread_mutex_t));
	if(clientTcbTab[index]->recvbufMutex==NULL){
		printf("heap full!\n");
		exit(-1);
	}
	pthread_mutex_init(clientTcbTab[index]->recvbufMutex,NULL);
  	return index;
}

// Õâžöº¯ÊýÓÃÓÚÁ¬œÓ·þÎñÆ÷. ËüÒÔÌ×œÓ×ÖID, ·þÎñÆ÷œÚµãIDºÍ·þÎñÆ÷µÄ¶Ë¿ÚºÅ×÷ÎªÊäÈë²ÎÊý. Ì×œÓ×ÖIDÓÃÓÚÕÒµœTCBÌõÄ¿.  
// Õâžöº¯ÊýÉèÖÃTCBµÄ·þÎñÆ÷œÚµãIDºÍ·þÎñÆ÷¶Ë¿ÚºÅ,  È»ºóÊ¹ÓÃsip_sendseg()·¢ËÍÒ»žöSYN¶Îžø·þÎñÆ÷.  
// ÔÚ·¢ËÍÁËSYN¶ÎÖ®ºó, Ò»žö¶šÊ±Æ÷±»Æô¶¯. Èç¹ûÔÚSYNSEG_TIMEOUTÊ±ŒäÖ®ÄÚÃ»ÓÐÊÕµœSYNACK, SYN ¶Îœ«±»ÖØŽ«. 
// Èç¹ûÊÕµœÁË, ŸÍ·µ»Ø1. ·ñÔò, Èç¹ûÖØŽ«SYNµÄŽÎÊýŽóÓÚSYN_MAX_RETRY, ŸÍœ«state×ª»»µœCLOSED, ²¢·µ»Ø-1.
int stcp_client_connect(int sockfd, int nodeID, unsigned int server_port) 
{
  client_tcb_t *tp;  
  tp = clientTcbTab[sockfd];
  if(tp == NULL)
    return -1;
  tp->server_portNum = server_port;
  tp->server_nodeID=nodeID;
  tp->state = SYNSENT;

  seg_t seg;
  seg.header.src_port = tp->client_portNum;		//源端口号  
  seg.header.dest_port = server_port;       		//目的端口号
  seg.header.seq_num = tp->next_seqNum;         	//序号
  seg.header.ack_num = 0;         			//确认号，无用
  seg.header.length = 0;    				//段数据长度
  seg.header.type = SYN;     				//段类型 

  int count = 0;
  struct timeval starttime,endtime;
  long long int timeuse = 0;

  while(count != SYN_MAX_RETRY)
  {
    sip_sendseg(sip_conn,nodeID,&seg);
    printf("client:建立连接中，发送SYN给server. \n");
    gettimeofday(&starttime,0);
	timeuse = 0;
    //等待状态变为CONNECTED，或计时器timeuse超时
    while((tp->state != CONNECTED) && (timeuse < SYN_TIMEOUT))
    {
      gettimeofday(&endtime,0);
      timeuse = 1000*(1000000*(endtime.tv_sec - starttime.tv_sec) + endtime.tv_usec - starttime.tv_usec);
    }
    if(tp->state == CONNECTED)//在重传次数内收到SYNACK,状态改变为CONNECTED
    {
      printf("client:建立连接成功，从server处收到SYNACK. \n");
      return 1;
    }
    else//超时，初始化计时器，准备进入下一次循环来重传
    {
      printf("client:建立连接失败，准备超时重传SYN. \n");
      count ++;
    } 
  }
  //重传超过SYN_MAX_RETRY次
  tp->state = CLOSED;
  return -1;
}

// ·¢ËÍÊýŸÝžøSTCP·þÎñÆ÷. Õâžöº¯ÊýÊ¹ÓÃÌ×œÓ×ÖIDÕÒµœTCB±íÖÐµÄÌõÄ¿.
// È»ºóËüÊ¹ÓÃÌá¹©µÄÊýŸÝŽŽœšsegBuf, œ«ËüžœŒÓµœ·¢ËÍ»º³åÇøÁŽ±íÖÐ.
// Èç¹û·¢ËÍ»º³åÇøÔÚ²åÈëÊýŸÝÖ®Ç°Îª¿Õ, Ò»žöÃûÎªsendbuf_timerµÄÏß³ÌŸÍ»áÆô¶¯.
// Ã¿žôSENDBUF_ROLLING_INTERVALÊ±Œä²éÑ¯·¢ËÍ»º³åÇøÒÔŒì²éÊÇ·ñÓÐ³¬Ê±ÊÂŒþ·¢Éú. 
// Õâžöº¯ÊýÔÚ³É¹ŠÊ±·µ»Ø1£¬·ñÔò·µ»Ø-1. 
// stcp_client_sendÊÇÒ»žö·Ç×èÈûº¯Êýµ÷ÓÃ.
// ÒòÎªÓÃ»§ÊýŸÝ±»·ÖÆ¬Îª¹Ì¶šŽóÐ¡µÄSTCP¶Î, ËùÒÔÒ»ŽÎstcp_client_sendµ÷ÓÃ¿ÉÄÜ»á²úÉú¶àžösegBuf
// ±»ÌíŒÓµœ·¢ËÍ»º³åÇøÁŽ±íÖÐ. Èç¹ûµ÷ÓÃ³É¹Š, ÊýŸÝŸÍ±»·ÅÈëTCB·¢ËÍ»º³åÇøÁŽ±íÖÐ, žùŸÝ»¬¶¯Ž°¿ÚµÄÇé¿ö,
// ÊýŸÝ¿ÉÄÜ±»Ž«ÊäµœÍøÂçÖÐ, »òÔÚ¶ÓÁÐÖÐµÈŽýŽ«Êä.
int stcp_send(int sockfd, void* data, unsigned int length) 
{
   client_tcb_t *tp;  
   tp = clientTcbTab[sockfd];
  if(tp == NULL)
    return -1;

  if(tp->state != CONNECTED){
	printf("state no in CONNECTED!\n");
    	return -1;
  }

  int rest=length;

  while(rest){
	segBuf_t* segBuf = (segBuf_t*)malloc(sizeof(segBuf_t));//********
  	segBuf->seg.header.src_port = tp->client_portNum;		
  	segBuf->seg.header.dest_port = tp->server_portNum;
	int sendLen;
	if(rest<MAX_SEG_LEN) sendLen=rest;
	else sendLen=MAX_SEG_LEN;
 
 	segBuf->seg.header.seq_num = tp->next_seqNum;         	//序号
 	tp->next_seqNum = tp->next_seqNum + sendLen;
  	segBuf->seg.header.ack_num = 0;         		//确认号，暂未改为全双工
  	segBuf->seg.header.length = sendLen;   			//段数据长度
  	segBuf->seg.header.type = DATA;     			//段类型 

  	memcpy(segBuf->seg.data,(char *)data,sendLen);
	data+=sendLen;
  	segBuf->sentTime = 0;
  	segBuf->next = NULL;
  
  	struct timeval starttime;
  	//如果缓冲区为空，启动sendBuf_timer线程用于轮询超时
  	if(tp->sendBufHead == NULL){//插入缓冲区链表(作为头结点)
		pthread_mutex_lock(tp->sendbufMutex);
    		tp->sendBufHead = segBuf;
    		tp->sendBufunSent = segBuf;
    		tp->sendBufTail = segBuf; 
		pthread_mutex_unlock(tp->sendbufMutex); 
    		sip_sendseg(sip_conn,tp->server_nodeID,&segBuf->seg);
    		printf("client:发送报文... \n");
    		gettimeofday(&starttime,0);
    		segBuf->sentTime = starttime.tv_sec * 1000000 + starttime.tv_usec;
    		tp->unAck_segNum = 1;
    		pthread_create(&timer,NULL,sendBuf_timer,(void *)tp);
    		tp->sendBufunSent = NULL;
  	}
  	//如果缓冲区非空，从第一个未发送的段开始发送，直到发送但未被确认的段数达到GBN_WINDOW，或是直到已将缓冲区最后一个待发段发送出去
  	else{
    		pthread_mutex_lock(tp->sendbufMutex);
    		tp->sendBufTail->next = segBuf;
    		tp->sendBufTail = segBuf; 
		if(tp->sendBufunSent==NULL) tp->sendBufunSent=segBuf;
	
		//printf("get 198!\n");
    		while(tp->unAck_segNum < GBN_WINDOW){
     			 sip_sendseg(sip_conn,tp->server_nodeID,&tp->sendBufunSent->seg);
      			gettimeofday(&starttime,0);
      			tp->sendBufunSent->sentTime = starttime.tv_sec * 1000000 + starttime.tv_usec;
      			tp->unAck_segNum ++;
      			tp->sendBufunSent = tp->sendBufunSent->next;
      			if(tp->sendBufunSent == NULL)//已经发送完缓冲区的最后一个待发段
        			break;
    		}
		pthread_mutex_unlock(tp->sendbufMutex);
  	}
	rest-=sendLen;
  }
	return 1;
}

// Õâžöº¯ÊýÓÃÓÚ¶Ï¿ªµœ·þÎñÆ÷µÄÁ¬œÓ. ËüÒÔÌ×œÓ×ÖID×÷ÎªÊäÈë²ÎÊý. Ì×œÓ×ÖIDÓÃÓÚÕÒµœTCB±íÖÐµÄÌõÄ¿.  
// Õâžöº¯Êý·¢ËÍFIN¶Îžø·þÎñÆ÷. ÔÚ·¢ËÍFINÖ®ºó, stateœ«×ª»»µœFINWAIT, ²¢Æô¶¯Ò»žö¶šÊ±Æ÷.
// Èç¹ûÔÚ×îÖÕ³¬Ê±Ö®Ç°state×ª»»µœCLOSED, Ôò±íÃ÷FINACKÒÑ±»³É¹ŠœÓÊÕ. ·ñÔò, Èç¹ûÔÚŸ­¹ýFIN_MAX_RETRYŽÎ³¢ÊÔÖ®ºó,
// stateÈÔÈ»ÎªFINWAIT, stateœ«×ª»»µœCLOSED, ²¢·µ»Ø-1.
int stcp_client_disconnect(int sockfd) 
{
  client_tcb_t *tp;
  tp = clientTcbTab[sockfd];
  if(tp == NULL)
    return -1;
  
  seg_t seg;
  seg.header.src_port = tp->client_portNum;		//源端口号  
  seg.header.dest_port = tp->server_portNum;       	//目的端口号
  seg.header.seq_num = 0;         			//序号,无用
  seg.header.ack_num = 0;         			//确认号，无用
  seg.header.length = 0;    				//段数据长度
  seg.header.type = FIN;     				//段类型 

  int count = 0;
  struct timeval starttime,endtime;
  long long int timeuse = 0;
  tp->state = FINWAIT;
  while(count != FIN_MAX_RETRY)
  {
    sip_sendseg(sip_conn,tp->server_nodeID,&seg);
    printf("client:断开连接中，发送FIN给server. \n");
    gettimeofday(&starttime,0);
	timeuse = 0;
    //等待状态变为CLOSED，或计时器timeuse超时
    while((tp->state != CLOSED) && (timeuse < FIN_TIMEOUT))
    {
      gettimeofday(&endtime,0);
      timeuse = 1000*(1000000*(endtime.tv_sec - starttime.tv_sec) + endtime.tv_usec - starttime.tv_usec);
    }
    if(tp->state == CLOSED)//在重传次数内收到FINACK,状态改变为CLOSED
    {
      printf("client:断开连接成功，从server处收到FINACK. \n");
      return 1;
    }
    else//超时，初始化计时器，准备进入下一次循环来重传
    {
       printf("client:断开连接失败，准备超时重传FIN. \n");
       count ++; 
    }
  }
  //重传超过FIN_MAX_RETRY次
  tp->state = CLOSED;
  return -1;
}

// Õâžöº¯Êýµ÷ÓÃfree()ÊÍ·ÅTCBÌõÄ¿. Ëüœ«žÃÌõÄ¿±êŒÇÎªNULL, ³É¹ŠÊ±(ŒŽÎ»ÓÚÕýÈ·µÄ×ŽÌ¬)·µ»Ø1,
// Ê§°ÜÊ±(ŒŽÎ»ÓÚŽíÎóµÄ×ŽÌ¬)·µ»Ø-1.
int stcp_client_close(int sockfd) 
{
  client_tcb_t *tp;
  tp = clientTcbTab[sockfd];
  if(tp == NULL)
    return -1;
  free(tp->recvBuf);
  free(tp->sendbufMutex);
  free(tp->recvbufMutex);
  free(tp);

  tp = NULL;
  return 1;
}

// ÕâÊÇÓÉstcp_client_init()Æô¶¯µÄÏß³Ì. ËüŽŠÀíËùÓÐÀŽ×Ô·þÎñÆ÷µÄœøÈë¶Î. 
// seghandler±»ÉèŒÆÎªÒ»žöµ÷ÓÃsip_recvseg()µÄÎÞÇîÑ­»·. Èç¹ûsip_recvseg()Ê§°Ü, ÔòËµÃ÷µœSIPœø³ÌµÄÁ¬œÓÒÑ¹Ø±Õ,
// Ïß³Ìœ«ÖÕÖ¹. žùŸÝSTCP¶ÎµœŽïÊ±Á¬œÓËùŽŠµÄ×ŽÌ¬, ¿ÉÒÔ²ÉÈ¡²»Í¬µÄ¶¯×÷. Çë²é¿Ž¿Í»§¶ËFSMÒÔÁËœâžü¶àÏžœÚ.
void *seghandler(void* arg) {
  seg_t seg;
  int nodeID;
  while(1)
  {
    while(sip_recvseg(sip_conn,&nodeID,&seg));
	//printPacket(&seg);
    stcp_hdr_t* hdr=&seg.header;
    client_tcb_t* tcb;
    int i;
    for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++) 
      if(clientTcbTab[i]->client_portNum == hdr->dest_port&&
	clientTcbTab[i]->server_portNum==hdr->src_port&&
	clientTcbTab[i]->server_nodeID==nodeID)
      {
	tcb=clientTcbTab[i];
	break;			
      }
      if(i==MAX_TRANSPORT_CONNECTIONS)
	printf("ERROR:tcb not found!\n");
      else{
	printf("get into fsm!\n");
	clientFSM(tcb,hdr,seg.data);
	}
  }
  return 0;
}


//ÕâžöÏß³Ì³ÖÐøÂÖÑ¯·¢ËÍ»º³åÇøÒÔŽ¥·¢³¬Ê±ÊÂŒþ. Èç¹û·¢ËÍ»º³åÇø·Ç¿Õ, ËüÓŠÒ»Ö±ÔËÐÐ.
//Èç¹û(µ±Ç°Ê±Œä - µÚÒ»žöÒÑ·¢ËÍµ«ÎŽ±»È·ÈÏ¶ÎµÄ·¢ËÍÊ±Œä) > DATA_TIMEOUT, ŸÍ·¢ÉúÒ»ŽÎ³¬Ê±ÊÂŒþ.
//µ±³¬Ê±ÊÂŒþ·¢ÉúÊ±, ÖØÐÂ·¢ËÍËùÓÐÒÑ·¢ËÍµ«ÎŽ±»È·ÈÏ¶Î. µ±·¢ËÍ»º³åÇøÎª¿ÕÊ±, ÕâžöÏß³Ìœ«ÖÕÖ¹.
void* sendBuf_timer(void* clienttcb)
{
  client_tcb_t *tp;
  tp = (client_tcb_t *)clienttcb;
  if(tp == NULL)
    return NULL;

  struct timeval starttime,endtime;
  unsigned int counttime = 0;
  while(tp->sendBufHead !=NULL)//一直运行到发送缓冲区为空为止
  {
	//pthread_mutex_lock(tp->sendbufMutex);
    gettimeofday(&endtime,0);
    counttime = endtime.tv_sec * 1000000 + endtime.tv_usec;
	int start=tp->sendBufHead->sentTime;
    if((counttime - start)*1000 < DATA_TIMEOUT)//没有超时，等待SENDBUF_POLLING_INTERVAL
    {
	struct timeval wait;
	if(SENDBUF_POLLING_INTERVAL<1000000000)
	{
	  wait.tv_sec = 0;
	  wait.tv_usec = SENDBUF_POLLING_INTERVAL/1000;
	}
	else
	{	
	  wait.tv_sec = SENDBUF_POLLING_INTERVAL/1000000000;
	  wait.tv_usec = ((SENDBUF_POLLING_INTERVAL%1000000000)/1000);
	}
	select(0, NULL, NULL, NULL, &wait);
    }
    else//超时，开始回退N帧
    {
	pthread_mutex_lock(tp->sendbufMutex);
      printf("超时，开始回退N帧. \n");
      segBuf_t * temp;
      temp = tp->sendBufHead;
	printPacket(&temp->seg);
      while(temp != tp->sendBufunSent)
      {
      	sip_sendseg(sip_conn,tp->server_nodeID,&temp->seg);
        gettimeofday(&starttime,0);
        temp->sentTime = starttime.tv_sec * 1000000 + starttime.tv_usec;
        temp = temp->next;
      }
	pthread_mutex_unlock(tp->sendbufMutex);
    }  
	//pthread_mutex_unlock(tp->sendbufMutex);
  }
	//pthread_detach(pthread_self());
  return NULL;
}

void clientFSM(client_tcb_t* t,stcp_hdr_t* h,char* data)
{
	printf("fsm type:%d\n",h->type);
  switch(t->state)
  {
    case CLOSED:
          printf("ERROR:client is closed!\n");
    	  break;
    case SYNSENT:
	  if(h->type == SYNACK)
	  {
	    t->state = CONNECTED;
	    //displaySeg(h);
	  }
	  else printf("ERROR:client is waiting for SYNACK!\n");
    	  break;
    case CONNECTED:
	  if(h->type == DATAACK)//接收到ack
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
	    while(t->sendBufunSent!=NULL&&t->unAck_segNum < GBN_WINDOW){
     			sip_sendseg(sip_conn,t->server_nodeID,&t->sendBufunSent->seg);
      			gettimeofday(&starttime,0);
      			t->sendBufunSent->sentTime = starttime.tv_sec * 1000000 + starttime.tv_usec;
      			t->unAck_segNum ++;
      			t->sendBufunSent = t->sendBufunSent->next;
    	    }
		pthread_mutex_unlock(t->sendbufMutex);
          }
	  else if(h->type==DATA)//接收到data
	  {	
		seg_t temp;
		temp.header.length=0;	
		temp.header.src_port=h->dest_port;
		temp.header.dest_port=h->src_port;
		temp.header.type=DATAACK;
		if(t->expect_seqNum==h->seq_num){
			pthread_mutex_lock(t->recvbufMutex);//lock
			if(t->usedBufLen+h->length<RECEIVE_BUF_SIZE)
			{
				memcpy((t->recvBuf+t->usedBufLen),data,h->length);
				t->usedBufLen+=h->length;
				t->expect_seqNum+=h->length;
				temp.header.ack_num=t->expect_seqNum;
				sip_sendseg(sip_conn,t->client_nodeID,&temp);
			}
			else 
				printf("recvBuffer full! packet loss!\n");
			pthread_mutex_unlock(t->recvbufMutex);
		}
		else
		{ 
			temp.header.ack_num=t->expect_seqNum;
			sip_sendseg(sip_conn,t->client_nodeID,&temp);
		}
	  }
	  else printf("ERROR:in CONNECTED;\n");
    	  break;
    case FINWAIT:
	  if(h->type == FINACK)
	  {
	    t->state = CLOSED;
	    //displaySeg(h);
	  }

	  else printf("ERROR:client is waiting for FINACK!\n");
    	  break;
    default:printf("state error!\n");
  }
  return;
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

