
#include "seg.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>

//STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
//参数sip_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t发送成功,就返回1,否则返回-1.
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segPtr)
{
   	char* head="!&";
	char* tail="!#";
	segPtr->header.checksum=0;
	segPtr->header.checksum=checksum(segPtr);
	sendseg_arg_t temp;
	temp.nodeID=dest_nodeID;
	memcpy(&(temp.seg),segPtr,sizeof(seg_t));
	/*printf("<<<<<<<<<<<<<<<<<<send>>>>>>>>>>>>>>>>>>>>>>>>\n");
	printPacket(segPtr);
	printf("<<<<<<<<<<<<<<<<<<send>>>>>>>>>>>>>>>>>>>>>>>>\n");*/
	if(send(sip_conn,head,2*sizeof(char),0) < 0) {
		printf("head send fail!\n");
		return -1;
	}
	if(send(sip_conn,&temp,sizeof(sendseg_arg_t),0) < 0) {
		printf("segPtr send fail!\n");
		return -1;
	}
	if(send(sip_conn,tail,2*sizeof(char),0) < 0) {
		printf("tail send fail!\n");
		return -1;
	}
	return 1;
}

//STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
//参数sip_conn是STCP进程和SIP进程之间连接的TCP描述符.
//当接收到段时, 使用seglost()来判断该段是否应被丢弃并检查校验和.
//如果成功接收到sendseg_arg_t就返回0, 否则返回1.
int sip_recvseg(int sip_conn, int* src_nodeID, seg_t* segPtr)
{
	if(segPtr == NULL) {
		printf("segPtr can't be NULL!\n");
		exit(-1);
	}
	//printf("<<<<<<<<<<<<<<<<<<recv>>>>>>>>>>>>>>>>>>>>>>>>\n");
	//printPacket(segPtr);
	char temp;
	sendseg_arg_t rseg;
	while(1) {
		do{
			recv(sip_conn,&temp,sizeof(char),0);
		}while(temp != '!');
		recv(sip_conn,&temp,sizeof(char),0);
		if(temp == '&')
			break;
	}
	if(recv(sip_conn,&rseg,sizeof(sendseg_arg_t),0) < 0)
		return 1;
	while(1) {
		do{
			recv(sip_conn,&temp,sizeof(char),0);
		}while(temp != '!');
		recv(sip_conn,&temp,sizeof(char),0);
		if(temp == '#')
			break;
	}
	*src_nodeID=rseg.nodeID;
	memcpy(segPtr,&(rseg.seg),sizeof(seg_t));
	if(seglost(segPtr)){
		return 1;
	}
	if(checkchecksum(segPtr)==1){
		printf("receive seg\n");
		return 0;
	}
	else{ 
		printPacket(segPtr);
		printf("checksum error!\n");
		return 1;
	}
}

//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr)
{
  	char temp;
	sendseg_arg_t sendseg;
	while(1) {
		do {
			if(recv(stcp_conn,&temp,sizeof(char),0) < 0) {
				printf("can't receive \"!\"\n");
				return -1;
			}
		}while(temp != '!');
		if(recv(stcp_conn,&temp,sizeof(char),0) < 0) {
			return -1;
		}
		if(temp == '&') {
			break;
		}
	}
	if(recv(stcp_conn,&sendseg,sizeof(sendseg_arg_t),0) < 0) {
		printf("receive packet fail!\n");
		return -1;
	}
	while(1) {
		do {
			if(recv(stcp_conn,&temp,sizeof(char),0) < 0) {
				printf("can't receive \"!\"\n");
				return -1;
			}
		}while(temp != '!');
		if(recv(stcp_conn,&temp,sizeof(char),0) < 0) {
			return -1;
		}
		if(temp == '#') {
			break;
		}
	}
	memcpy(segPtr,&(sendseg.seg),sizeof(seg_t));
	*dest_nodeID=sendseg.nodeID;
	return 1;
}

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr)
{
 	char* head="!&";
	char* tail="!#";
	sendseg_arg_t temp;
	temp.nodeID=src_nodeID;
	memcpy(&(temp.seg),segPtr,sizeof(seg_t));
	/*printf("<<<<<<<<<<<<<<<<<<send>>>>>>>>>>>>>>>>>>>>>>>>\n");
	printPacket(segPtr);
	printf("<<<<<<<<<<<<<<<<<<send>>>>>>>>>>>>>>>>>>>>>>>>\n");*/
	if(send(stcp_conn,head,2*sizeof(char),0) < 0) {
		printf("head send fail!\n");
		return -1;
	}
	if(send(stcp_conn,&temp,sizeof(sendseg_arg_t),0) < 0) {
		printf("sip_pkt_t send fail!\n");
		return -1;
	}
	if(send(stcp_conn,tail,2*sizeof(char),0) < 0) {
		printf("tail send fail!\n");
		return -1;
	}
	return 1;
}

// 一个段有PKT_LOST_RATE/2的可能性丢失, 或PKT_LOST_RATE/2的可能性有着错误的校验和.
// 如果数据包丢失了, 就返回1, 否则返回0. 
// 即使段没有丢失, 它也有PKT_LOST_RATE/2的可能性有着错误的校验和.
// 我们在段中反转一个随机比特来创建错误的校验和.
int seglost(seg_t* segPtr)
{
  	int random = rand()%100;
	if(random<PKT_LOSS_RATE*100) {
		//50%可能性丢失段
		if(rand()%2==0) {
			printf("seg lost!!!\n");
			return 1;
		}
		//50%可能性是错误的校验和
		else {
				printf("checksum lost!\n");
			//获取数据长度
			int len = sizeof(stcp_hdr_t)+segPtr->header.length;
			//获取要反转的随机位
			int errorbit = rand()%(len*8);
			//反转该比特
			char* temp = (char*)segPtr;
			temp = temp + errorbit/8;
			*temp = *temp^(1<<(errorbit%8));
			return 0;
		}
	}
	return 0;
}

//这个函数计算指定段的校验和.
//校验和计算覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment)
{
 	if(segment == NULL) {
		printf("segment in NULL!\n");
		return 0;
	}
	unsigned long cksum = 0;
	int size = sizeof(stcp_hdr_t)+segment->header.length;
	unsigned short* data = (unsigned short*)(segment); 
	while(size > 1) {
		cksum+=*data;
		data++;
		size-=sizeof(unsigned short);
	}
	if(size) {
		cksum+=(*(unsigned char*)data);
	}
	cksum = (cksum>>16) + (cksum&0xffff);
	cksum += (cksum>>16);
	return (unsigned short)(~cksum);
}

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1.
int checkchecksum(seg_t* segment)
{
 	unsigned short cksum = checksum(segment);
	//printf("sksum:%x\n",cksum);
	if(cksum == 0)
		return 1;
	else
		return -1;
}
void printPacket(seg_t* segPtr) {
	printf("========================================\n");
	if(segPtr == NULL) {
		printf("segPtr is NULL!\n");
	}
	else {
		printf("Packet Information:\n");
		printf("Source Port: %d\n",segPtr->header.src_port);
		printf("Destination Port: %d\n",segPtr->header.dest_port);
		printf("Sequence Number: %d\n",segPtr->header.seq_num);
		printf("Ack Number: %d\n",segPtr->header.ack_num);
		printf("Data Length: %d\n",segPtr->header.length);
		printf("Packet Type: ");
		
		switch(segPtr->header.type) {
			case(SYN):
				printf("SYN\n");
				break;
			case(SYNACK):
				printf("SYNACK\n");
				break;
			case(FIN):
				printf("FIN\n");
				break;
			case(FINACK):
				printf("FINACK\n");
				break;
			case(DATA):
				printf("DATA\n");
				break;
			case(DATAACK):
				printf("DATAACK\n");
				break;
			default:
				printf("Unexpected type!\n");
		}
		printf("Receive Window: %d\n", segPtr->header.rcv_win);
		printf("Checksum: %d\n",segPtr->header.checksum);
		/*if(segPtr->header.length!=0) {
			int i=0;
			printf("Data:");
			while(i<segPtr->header.length) {
				printf(" %.2x",segPtr->data[i]);
				i++;
			}
		}
		else
			printf("Data: NULL\n");*/
	}
	printf("======================================\n");
}
