#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>

#include "service.h"
#include "const.h"
#include "types.h"
#include "server.h"


void login(P rcv,P snd,int sockfd)
{
	Log* L=(Log*)rcv.data;
	int i;
	for(i=0;i<MAXSIZE;i++) if(book[i].isUsed&&!strncmp(book[i].name,L->name,NAMESIZE)){
		snd.magicNum=htonl(MAGICNUM);
		snd.service=SERVICE_ERROR;
		strncpy((char*)snd.data,"ID is already in use!",sizeof(snd.data));
		stcp_send(sockfd,&snd,sizeof(snd));
		break;
	}

	if(i==MAXSIZE){ // ID unused
		pthread_mutex_lock(&mutexBook); //lock book[]

		for(i=0;i<MAXSIZE;i++) if(users[i].isUsed){
			if(L->addr==users[i].addr){
				strncpy(users[i].name,L->name,sizeof(users[i].name)); //establish contact between name and address 

				int j;
				snd.magicNum=htonl(MAGICNUM);
				snd.service=SERVICE_FRIENDS_LOGIN;
				strncpy((char*)snd.data,L->name,NAMESIZE);
				for(j=0;j<MAXSIZE;j++)if(book[j].isUsed)
					stcp_send(users[book[j].pos].sock,&snd,sizeof(snd)); //tell all users L->name online


				snd.service=SERVICE_ACK;
				for(j=0;j<MAXSIZE;j++) if(!book[j].isUsed){
					strncpy(book[j].name,L->name,NAMESIZE);
					book[j].pos=i;
					book[j].isUsed=true;
					break;
				}
				stcp_send(sockfd,&snd,sizeof(snd)); //return ACK to confirm


				snd.service=SERVICE_FRIENDS_LIST;
				int start=0;
				FL* F=(FL*)snd.data;
				F->num=0;
				for(j=0;j<MAXSIZE;j++) if(book[j].isUsed){
					F->num++;
					strncpy(F->fl+start,book[j].name,strlen(book[j].name));
					start+=strlen(book[j].name);
					if(start<sizeof(F->fl)){
						stcp_send(sockfd,&snd,sizeof(snd)); //send friendsList
						F->num=0;
						start=0;
					}
				}  
				break;
			}
		}
		pthread_mutex_unlock(&mutexBook); //unlock book
	}
}

void sendMsg(P rcv,P snd,int sockfd)
{
	Msg* M=(Msg*)rcv.data;
	int i,pos;
	snd.magicNum=htonl(MAGICNUM);
	printf("to: %s\n",M->to);
	for(i=0;i<MAXSIZE;i++) if(book[i].isUsed&&!strncmp((char*)M->to,book[i].name,NAMESIZE)){ //find socket
		pos=book[i].pos;
		break;				
	}
	if(i==MAXSIZE){ //not found
		snd.service=SERVICE_ERROR;
		strncpy((char*)snd.data,"Username does not exist!",DATASIZE);
		stcp_send(sockfd,&snd,sizeof(snd));
	}
	else
		stcp_send(users[pos].sock,&rcv,sizeof(rcv));
}

void broadcast(P rcv,P snd)
{
	Msg* M=(Msg*)snd.data;
	int i;
	snd.magicNum=htonl(MAGICNUM);
	snd.service=SERVICE_SEND;
	memcpy(snd.data,rcv.data,DATASIZE);
	for(i=0;i<MAXSIZE;i++) if(book[i].isUsed&&strncmp(book[i].name,(char*)M->from,NAMESIZE)){
		strncpy((char*)M->to,book[i].name,NAMESIZE);
		stcp_send(users[book[i].pos].sock,&snd,sizeof(snd));
	}
}

void logout(P rcv,P snd,int sockfd)
{
	Log* L=(Log*)rcv.data;
	int i,j;
	for(i=0;i<MAXSIZE;i++) if(!strncmp(book[i].name,L->name,NAMESIZE)){
		book[i].isUsed=false;
		snd.magicNum=htonl(MAGICNUM);
		snd.service=SERVICE_LOGOUT_ACK;
		stcp_send(sockfd,&snd,sizeof(snd)); //return ACK confirm
		break;
	}

	snd.magicNum=htonl(MAGICNUM);
	snd.service=SERVICE_FRIENDS_LOGOUT;
	strncpy((char*)snd.data,L->name,NAMESIZE);
	for(j=0;j<MAXSIZE;j++)if(book[j].isUsed)
		stcp_send(users[book[j].pos].sock,&snd,sizeof(snd)); //tell all users L->name offline
}

void myExit(P rcv,P snd,int sockfd)
{
	Log* L=(Log*)rcv.data;
	int i;
	for(i=0;i<MAXSIZE;i++) if(users[i].isUsed)
		if(L->addr==users[i].addr){
			users[i].isUsed=false;	//remove 
			snd.magicNum=htonl(MAGICNUM);
			snd.service=SERVICE_EXIT_ACK;
			//send(sockfd,&snd,sizeof(snd),0); //return ACK to confirm
			stcp_send(sockfd,&snd,sizeof(snd));
			break;
		}
}




void* handle(void* t){
	int n,sockfd=(int)t;
	P rcv,snd;
	while((n=stcp_recv(sockfd,&rcv,sizeof(rcv)))>0){
		if(ntohl(rcv.magicNum)==MAGICNUM){
			if(rcv.service==SERVICE_LOGIN){
				login(rcv,snd,sockfd);
			}
			else if(rcv.service==SERVICE_SEND){
				sendMsg(rcv,snd,sockfd);
			}
			else if(rcv.service==SERVICE_BROADCAST){
				broadcast(rcv,snd);
			}
			else if(rcv.service==SERVICE_LOGOUT){
				logout(rcv,snd,sockfd);
			}
			else if(rcv.service==SERVICE_EXIT){
				myExit(rcv,snd,sockfd);
				pthread_exit(NULL);
			}
		}

	}
	if(n<0)	printf("Read error!!\n");
	pthread_exit(NULL);
}

int main()
{
	//用于丢包率的随机数种子
	srand(time(NULL));

	sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("can not connect to the local SIP process\n");
		return ;
	}

	stcp_server_init(sip_conn);
	
	pthread_t threads[NUMTHREADS];
	int threadNum=0;
	
	pthread_mutex_init(&mutexUsers,NULL);
	pthread_mutex_init(&mutexBook,NULL);
	
	while(1){
		int connfd= stcp_server_sock(SERV_PORT);
		int clientID;
		stcp_server_accept(connfd,&clientID);
		
		printf("received request...\n");
		int k;
		for(k=0;k<MAXSIZE;k++) if(!users[k].isUsed) break;
		if(k!=MAXSIZE){
			pthread_mutex_lock(&mutexUsers);
			memset(&users[k],0,sizeof(User));//bzero for memcmp
			users[k].isUsed=true;
			users[k].sock=connfd;
			users[k].addr=clientID;
			pthread_mutex_unlock(&mutexUsers);
			int rc=pthread_create(&threads[threadNum++],NULL,handle,(void *)connfd);
			if(threadNum>=NUMTHREADS){
				printf("ERROR: Excessive application link!\n"); 
				exit(-1);
			}
			if(rc){
				printf("ERROR: return code from pthread_create() is %d\n",rc);
				exit(-1);
			}

			printf("Thread created for handle it!\n");
		}
		else printf("Server full load!!\n");
	}
	//close(listenfd);
	disconnectToSIP(sip_conn);
}
