#ifndef __CLIENT_H__
#define __CLIENT_H__

typedef struct Protocol{
	u32 magicNum;
	u8 service;
	u8 data[DATASIZE];
}P; //protocol

typedef struct Msg{
	u8 from[NAMESIZE];
	u8 to[NAMESIZE];
	struct tm timeinfo;
	u8 content[MSGSIZE];
}Msg;

typedef struct Login{
	char name[NAMESIZE];
	int addr;
}Login;

typedef struct Friend_List{
	int num;
	char fl[DATASIZE-4];
}FL;

typedef struct Friend{
	bool isUsed;
	char name[NAMESIZE];
}Friend;

typedef struct Link{
	Msg info;
	bool isUsed;
}Link;


u32 sockfd;
Friend friendsList[MAXSIZE];
Link mailBox[MAILBOXSIZE];
char buffer[30],cid[NAMESIZE];
P snd,rcv;


#endif
