#ifndef __SERVER_H__
#define __SERVER_H__

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

typedef struct User{
	char name[NAMESIZE];
	int addr;
	u32 sock;
	bool isUsed;
}User;

typedef struct Log{
	char name[NAMESIZE];
	int addr;
}Log;

typedef struct Book{
	char name[NAMESIZE];
	int pos; //users[] Subscript
	bool isUsed;
}Book;

typedef struct Friend_List{
	int num;
	char fl[DATASIZE-4];
}FL;

User users[MAXSIZE]; //record client's sockaddr_in and sockfd
Book book[MAXSIZE]; //record online users' name and users[] subscript
pthread_mutex_t mutexUsers,mutexBook;

#endif
