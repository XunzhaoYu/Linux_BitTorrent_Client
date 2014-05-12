#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

#include "service.h"
#include "const.h"
#include "types.h"
#include "client.h"

void init_mailBox(){
	memset(mailBox,0,sizeof(mailBox));
}

void init_FL(){
	memset(friendsList,0,sizeof(friendsList));
}

void stdin_flush(){
	u8 c;
	while((c=getchar())!=EOF&&c!='\n');
}

void print_mainInterface(){
	printf("%s Welcome to use MYIM!!!!\n"
		"Please select the service:\n"
		"1.send message.\n"
		"2.check message.\n"
		"3.online friends\n" 
		"4.Broadcast\n"
		"c(cls),(#)logout\n"
		"=============================\n",cid);
}

bool login()
{	
	fgets(cid,NAMESIZE,stdin);
	if(strchr(cid,'\n')==0) stdin_flush();
	else cid[strlen(cid)-1]='\0';

	memset(&snd,0,sizeof(snd));

	snd.magicNum=htonl(MAGICNUM);

	if(cid[0]=='e'&&cid[1]=='\0') snd.service=SERVICE_EXIT;
	else snd.service=SERVICE_LOGIN;

	Login* L=(Login*)snd.data;
	strncpy((char*)L->name,cid,strlen(cid));
	L->addr=topology_getMyNodeID();

	send(sockfd,&snd,sizeof(snd),0);

	if(stcp_recv(sockfd,&rcv,sizeof(rcv))==-1){
		perror("The server terminated prematurely");
		exit(1);
	}

	if(ntohl(rcv.magicNum)==MAGICNUM){
		if(rcv.service==SERVICE_ACK){ 
			init_FL();
			init_mailBox();
			return true;
		}
		else if(rcv.service==SERVICE_EXIT_ACK) exit(0);
		else{ 
			printf("%s\n",rcv.data);
			return false;
		}
	}
	return false;	
}

void sendMsg()
{
	memset(&snd,0,sizeof(snd));
				
	snd.magicNum=htonl(MAGICNUM);
	snd.service=SERVICE_SEND;
	Msg* message=(Msg*)snd.data;
	strncpy((char*)message->from,cid,NAMESIZE);

	printf("to:");
	scanf("%16s",message->to);
	stdin_flush();

	printf("\ncontent:");
	fgets((char *)message->content,MSGSIZE,stdin);
	if(strchr((char*)message->content,'\n')==0){ 
		stdin_flush();
		message->content[MSGSIZE-1]='\0';
	}

	time_t timep;
	time(&timep);
	memcpy(&(message->timeinfo),localtime(&timep),sizeof(struct tm));
	
	stcp_send(sockfd,&snd,sizeof(snd));
	
}

void checkMsg()
{
	int c,slct,count;
	struct tm tt;
	for(c=0,count=0;c<MAILBOXSIZE;c++) if(mailBox[c].isUsed){
		count++;
		Msg temp=mailBox[c].info;
		tt= temp.timeinfo;
		printf("%d: %d/%d/%d  %d:%d:%d    from:%s\n",
		count,1900+tt.tm_year,tt.tm_mon+1,tt.tm_mday,
		tt.tm_hour,tt.tm_min,tt.tm_sec,temp.from);
	}
	printf("select the message you wanna check:\n"
		"f(flush),r(back)\n"
		"=====================================\n");		
	while(scanf("%5s",buffer)!=0){
		stdin_flush();
	
		if(buffer[0]=='f'&&buffer[1]=='\0'){
			system("clear");
			for(c=0,count=0;c<MAILBOXSIZE;c++) if(mailBox[c].isUsed){
				count++;
				Msg temp=mailBox[c].info;
				tt= temp.timeinfo;
				printf("%d: %d/%d/%d  %d:%d:%d    from:%s\n",
					count,1900+tt.tm_year,tt.tm_mon+1,tt.tm_mday,
					tt.tm_hour,tt.tm_min,tt.tm_sec,temp.from);
			}
			printf("select the message you wanna check:\n"
				"f(flush),r(back)\n"
				"=====================================\n");		
		}
		else if(buffer[0]=='r'&&buffer[1]=='\0'){ 
			print_mainInterface();				
			break;
		}
		else if((slct=atoi((char*)buffer))>0&&slct<=count){	
			int i,cc=0;
			Msg temp;
			for(i=0;i<MAILBOXSIZE;i++) if(mailBox[i].isUsed){
				cc++; 
				if(cc==slct){ 
					temp=mailBox[i].info;
					tt= temp.timeinfo;
					break;
				}
			}
			printf("%d: %d/%d/%d  %d:%d:%d    from:%s\ncontent:%s\n",
				count,1900+tt.tm_year,tt.tm_mon+1,tt.tm_mday,
				tt.tm_hour,tt.tm_min,tt.tm_sec,temp.from,temp.content);
			mailBox[i].isUsed=false;
		}
		else printf("input error!!\n");
	}
}

void onlineFriends()
{
	bool flage=true;
	while(flage){
		system("clear");
		int i;
		for(i=0;i<MAXSIZE;i++) if(friendsList[i].isUsed)
			printf("%s\n",friendsList[i].name);
		printf("==============================\n"
			 "f(flush) r(back)\n"
			"=============================\n");
		char c;
		while(scanf("%c",&c)!=0){
			stdin_flush();
			if(c=='f') break;
			else if(c=='r'){
				flage=false;
				break;
			}
			else printf("input error!\n");
		}
	}
}

void broadcast()
{
	memset(&snd,0,sizeof(snd));
	snd.magicNum=htonl(MAGICNUM);
	snd.service=SERVICE_BROADCAST;
	Msg* message=(Msg*)snd.data;
	strncpy((char*)message->from,cid,NAMESIZE);
	printf("\ncontent:");
	fgets((char *)message->content,MSGSIZE,stdin);
	if(strchr((char*)message->content,'\n')==0){ 
		stdin_flush();
		message->content[MSGSIZE-1]='\0';
	}

	time_t timep;
	time(&timep);
	memcpy(&(message->timeinfo),localtime(&timep),sizeof(struct tm));

	stcp_send(sockfd,&snd,sizeof(snd));
}

void* rcvPkt(void* sock)
{

	while(1){
		if(stcp_recv(sockfd,sock,sizeof(P))==0){
			perror("The server terminated prematurely!");
			exit(4);
		}
		P* pp=sock;
		if(ntohl(pp->magicNum)==MAGICNUM){
			if(pp->service==SERVICE_SEND){
				int i;
				for(i=0;i<MAILBOXSIZE;i++) if(!mailBox[i].isUsed){
					mailBox[i].isUsed=true;
					memcpy(&mailBox[i].info,pp->data,sizeof(Msg));
					break;
				}
				printf("new message!\n");		
			}
			else if(pp->service==SERVICE_FRIENDS_LOGIN){
				printf("%s is online!\n",pp->data);
				int j;
				for(j=0;j<MAXSIZE;j++) if(!friendsList[j].isUsed){
					friendsList[j].isUsed=true;
					strncpy(friendsList[j].name,(char*)pp->data,NAMESIZE);
					break;
				}
			}
			else if(pp->service==SERVICE_FRIENDS_LOGOUT){
				printf("%s is offline!\n",pp->data);
				int j;
				for(j=0;j<MAXSIZE;j++) 
					if(friendsList[j].isUsed&&!strncmp(friendsList[j].name,(char*)pp->data,NAMESIZE))
						friendsList[j].isUsed=false;
			}
			else if(pp->service==SERVICE_FRIENDS_LIST){
				FL* F=(FL*)pp->data;
				int i,start=0;
				for(i=0;i<F->num;i++){
					int j;
					for(j=0;j<MAXSIZE;j++) if(!friendsList[j].isUsed){
						friendsList[j].isUsed=true;
						strncpy(friendsList[j].name,F->fl+start,NAMESIZE);
						start+=strlen(F->fl+start);
						break;
					}
				}
			}
			else if(pp->service==SERVICE_LOGOUT_ACK){
				pthread_exit(NULL);
			}
			else if(pp->service==SERVICE_ERROR){
				printf("%s\n",pp->data);
			}
		}
	}
}

int main(int argc,char **argv)
{
	//用于丢包率的随机数种子
	srand(time(NULL));

	//连接到SIP进程并获得TCP套接字描述符	
	sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("fail to connect to the local SIP process\n");
		exit(1);
	}

	//初始化stcp客户端
	stcp_client_init(sip_conn);
	sleep(STARTDELAY);

	char hostname[50];
	printf("Enter server name to connect:");
	scanf("%s",hostname);
	int server_nodeID = topology_getNodeIDfromname(hostname);
	if(server_nodeID == -1) {
		printf("host name error!\n");
		exit(1);
	} else {
		printf("connecting to node %d\n",server_nodeID);
	}


	pthread_t listenThread;


	sockfd = stcp_client_sock(CLIENTPORT);
	if(sockfd<0) {
		printf("fail to create stcp client sock");
		exit(1);
	}


	if(stcp_client_connect(sockfd,server_nodeID,SERV_PORT)<0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}
	printf("client connected to server, client port:%d, server port %d\n",CLIENTPORT,SERV_PORT);

	while(true){
		printf("Welcome to use MYIM!!!!\n"
			"Enter 'e' to exit!!\n"
			"ID:");

		while(!login()){
			printf("ID:");
		}

		int rc;
		if((rc=pthread_create(&listenThread,NULL,rcvPkt,(void*)&rcv))!=0){
			printf("ERROR; return code from pthread_create() is %d\n",rc);
			exit(-1);
		}
		
		print_mainInterface();

		while(scanf("%2s",buffer)!=0){
			stdin_flush();
			if(buffer[1]=='\0'){
				if(buffer[0]=='1'){
					system("clear");
					sendMsg();
					print_mainInterface();
				}
				else if(buffer[0]=='2'){
					system("clear");
					checkMsg();
				}
				else if(buffer[0]=='3'){
					onlineFriends();
					print_mainInterface();

				}
				else if(buffer[0]=='4'){
					system("clear");
					broadcast();
					print_mainInterface();
				}
				else if(buffer[0]=='c'){
					system("clear");
					print_mainInterface();
				}
				else if(buffer[0]=='#'){
					snd.magicNum=htonl(MAGICNUM);
					snd.service=SERVICE_LOGOUT;
					Login* L=(Login*)snd.data;
					strncpy(L->name,cid,strlen(cid));
					stcp_send(sockfd,&snd,sizeof(snd));
					pthread_join(listenThread,NULL);
					system("clear");
					break;
				}
				else printf("input error!!\n");
			}
			else{
				printf("input error!!\n");
			}
		}//end while
	}//end while
}
