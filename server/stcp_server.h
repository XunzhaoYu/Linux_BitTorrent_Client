// ÎÄŒþÃû: stcp_server.h
//
// ÃèÊö: ÕâžöÎÄŒþ°üº¬·þÎñÆ÷×ŽÌ¬¶šÒå, Ò»Ð©ÖØÒªµÄÊýŸÝœá¹¹ºÍ·þÎñÆ÷STCPÌ×œÓ×ÖœÓ¿Ú¶šÒå. ÄãÐèÒªÊµÏÖËùÓÐÕâÐ©œÓ¿Ú.
//
// ŽŽœšÈÕÆÚ: 2013Äê1ÔÂ
//

#ifndef STCPSERVER_H
#define STCPSERVER_H

#include <pthread.h>
#include "../common/seg.h"
#include "../common/constants.h"

//FSM中使用的服务器状态
#define	CLOSED 1
#define	LISTENING 2
#define	CONNECTED 3
#define	CLOSEWAIT 4

typedef struct segBuf {
        seg_t seg;
        unsigned int sentTime;
        struct segBuf* next;
} segBuf_t;

//服务器传输控制块. 一个STCP连接的服务器端使用这个数据结构记录连接信息.
typedef struct server_tcb {
	unsigned int server_nodeID;     //服务器节点ID, 类似IP地址, 本实验未使用
	unsigned int server_portNum;    //服务器端口号
	unsigned int client_nodeID;     //客户端节点ID, 类似IP地址, 本实验未使用
	unsigned int client_portNum;    //客户端端口号
	unsigned int state;         	//服务器状态
	
	unsigned int next_seqNum;       //下一个要发送的序号
	segBuf_t* sendBufHead;          //指向发送缓冲区头部的指针
	segBuf_t* sendBufunSent;        //指向发送缓冲区正要发送的段的位置的指针
	segBuf_t* sendBufTail;          //指向发送缓冲区尾部的指针
	unsigned int unAck_segNum;      //尚未接收到ack的段数目
	pthread_mutex_t* sendbufMutex;  //指向一个互斥量的指针, 该互斥量用于对发送缓冲区的访问
	
	unsigned int expect_seqNum;     //期待接收的下一个数据序号	
	char* recvBuf;                  //指向接收缓冲区的指针
	unsigned int  usedBufLen;       //接收缓冲区中已接收数据的大小
	pthread_mutex_t* recvbufMutex;  //指向一个互斥量的指针, 该互斥量用于对接收缓冲区的访问
} server_tcb_t;


server_tcb_t* tcbTab[MAX_TRANSPORT_CONNECTIONS];
int sip_conn;
pthread_t handler;

//
//  ÓÃÓÚ·þÎñÆ÷¶ËÓŠÓÃ³ÌÐòµÄSTCPÌ×œÓ×ÖAPI. 
//  ===================================
//
//  ÎÒÃÇÔÚÏÂÃæÌá¹©ÁËÃ¿žöº¯Êýµ÷ÓÃµÄÔ­ÐÍ¶šÒåºÍÏžœÚËµÃ÷, µ«ÕâÐ©Ö»ÊÇÖžµŒÐÔµÄ, ÄãÍêÈ«¿ÉÒÔžùŸÝ×ÔŒºµÄÏë·šÀŽÉèŒÆŽúÂë.
//
//  ×¢Òâ: µ±ÊµÏÖÕâÐ©º¯ÊýÊ±, ÄãÐèÒª¿ŒÂÇFSMÖÐËùÓÐ¿ÉÄÜµÄ×ŽÌ¬, Õâ¿ÉÒÔÊ¹ÓÃswitchÓïŸäÀŽÊµÏÖ.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void stcp_server_init(int conn);

// Õâžöº¯Êý³õÊŒ»¯TCB±í, œ«ËùÓÐÌõÄ¿±êŒÇÎªNULL. Ëü»¹Õë¶ÔTCPÌ×œÓ×ÖÃèÊö·ûconn³õÊŒ»¯Ò»žöSTCP²ãµÄÈ«ŸÖ±äÁ¿, 
// žÃ±äÁ¿×÷Îªsip_sendsegºÍsip_recvsegµÄÊäÈë²ÎÊý. ×îºó, Õâžöº¯ÊýÆô¶¯seghandlerÏß³ÌÀŽŽŠÀíœøÈëµÄSTCP¶Î.
// ·þÎñÆ÷Ö»ÓÐÒ»žöseghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_sock(unsigned int server_port);

// Õâžöº¯Êý²éÕÒ·þÎñÆ÷TCB±íÒÔÕÒµœµÚÒ»žöNULLÌõÄ¿, È»ºóÊ¹ÓÃmalloc()ÎªžÃÌõÄ¿ŽŽœšÒ»žöÐÂµÄTCBÌõÄ¿.
// žÃTCBÖÐµÄËùÓÐ×Ö¶Î¶Œ±»³õÊŒ»¯, ÀýÈç, TCB state±»ÉèÖÃÎªCLOSED, ·þÎñÆ÷¶Ë¿Ú±»ÉèÖÃÎªº¯Êýµ÷ÓÃ²ÎÊýserver_port. 
// TCB±íÖÐÌõÄ¿µÄË÷ÒýÓŠ×÷Îª·þÎñÆ÷µÄÐÂÌ×œÓ×ÖID±»Õâžöº¯Êý·µ»Ø, ËüÓÃÓÚ±êÊ¶·þÎñÆ÷¶ËµÄÁ¬œÓ. 
// Èç¹ûTCB±íÖÐÃ»ÓÐÌõÄ¿¿ÉÓÃ, Õâžöº¯Êý·µ»Ø-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_accept(int sockfd);

// Õâžöº¯ÊýÊ¹ÓÃsockfd»ñµÃTCBÖžÕë, ²¢œ«Á¬œÓµÄstate×ª»»ÎªLISTENING. ËüÈ»ºóÆô¶¯¶šÊ±Æ÷œøÈëÃŠµÈŽýÖ±µœTCB×ŽÌ¬×ª»»ÎªCONNECTED 
// (µ±ÊÕµœSYNÊ±, seghandler»áœøÐÐ×ŽÌ¬µÄ×ª»»). žÃº¯ÊýÔÚÒ»žöÎÞÇîÑ­»·ÖÐµÈŽýTCBµÄstate×ª»»ÎªCONNECTED,  
// µ±·¢ÉúÁË×ª»»Ê±, žÃº¯Êý·µ»Ø1. Äã¿ÉÒÔÊ¹ÓÃ²»Í¬µÄ·œ·šÀŽÊµÏÖÕâÖÖ×èÈûµÈŽý.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_recv(int sockfd, void* buf, unsigned int length);

// œÓÊÕÀŽ×ÔSTCP¿Í»§¶ËµÄÊýŸÝ. Çë»ØÒäSTCPÊ¹ÓÃµÄÊÇµ¥ÏòŽ«Êä, ÊýŸÝŽÓ¿Í»§¶Ë·¢ËÍµœ·þÎñÆ÷¶Ë.
// ÐÅºÅ/¿ØÖÆÐÅÏ¢(ÈçSYN, SYNACKµÈ)ÔòÊÇË«ÏòŽ«µÝ. Õâžöº¯ÊýÃ¿žôRECVBUF_POLLING_INTERVALÊ±Œä
// ŸÍ²éÑ¯œÓÊÕ»º³åÇø, Ö±µœµÈŽýµÄÊýŸÝµœŽï, ËüÈ»ºóŽæŽ¢ÊýŸÝ²¢·µ»Ø1. Èç¹ûÕâžöº¯ÊýÊ§°Ü, Ôò·µ»Ø-1.
//
// ×¢Òâ: stcp_server_recvÔÚ·µ»ØÊýŸÝžøÓŠÓÃ³ÌÐòÖ®Ç°, Ëü×èÈûµÈŽýÓÃ»§ÇëÇóµÄ×ÖœÚÊý(ŒŽlength)µœŽï·þÎñÆ÷.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_close(int sockfd);

// Õâžöº¯Êýµ÷ÓÃfree()ÊÍ·ÅTCBÌõÄ¿. Ëüœ«žÃÌõÄ¿±êŒÇÎªNULL, ³É¹ŠÊ±(ŒŽÎ»ÓÚÕýÈ·µÄ×ŽÌ¬)·µ»Ø1,
// Ê§°ÜÊ±(ŒŽÎ»ÓÚŽíÎóµÄ×ŽÌ¬)·µ»Ø-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void* seghandler(void* arg);

// ÕâÊÇÓÉstcp_server_init()Æô¶¯µÄÏß³Ì. ËüŽŠÀíËùÓÐÀŽ×Ô¿Í»§¶ËµÄœøÈëÊýŸÝ. seghandler±»ÉèŒÆÎªÒ»žöµ÷ÓÃsip_recvseg()µÄÎÞÇîÑ­»·, 
// Èç¹ûsip_recvseg()Ê§°Ü, ÔòËµÃ÷µœSIPœø³ÌµÄÁ¬œÓÒÑ¹Ø±Õ, Ïß³Ìœ«ÖÕÖ¹. žùŸÝSTCP¶ÎµœŽïÊ±Á¬œÓËùŽŠµÄ×ŽÌ¬, ¿ÉÒÔ²ÉÈ¡²»Í¬µÄ¶¯×÷.
// Çë²é¿Ž·þÎñ¶ËFSMÒÔÁËœâžü¶àÏžœÚ.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void serverFSM(server_tcb_t* t,stcp_hdr_t* h,char* data);
//
#endif
