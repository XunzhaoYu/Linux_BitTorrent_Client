//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 
//
//创建日期: 2013年


#include "topology.h"
#include "netdb.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "../common/constants.h"
#include "unistd.h"
extern Topo_Table* tlt;

void insert_topoTable(Topo_Table** tt,char* host1,char* host2,int cost) {
	if(*tt == NULL) {
		(*tt) = (Topo_Table* )malloc(sizeof(Topo_Table));
		(*tt)->tl.nodeID = topology_getNodeIDfromname(host1);
		(*tt)->next = NULL;
		(*tt)->tl.cost = 0;
		(*tt)->tl.To = (Topo_List* )malloc(sizeof(Topo_Table));
		(*tt)->tl.To->nodeID = topology_getNodeIDfromname(host2);
		(*tt)->tl.To->cost = cost;
		(*tt)->tl.To->To = NULL;
	}
	else {
		int fromNode = topology_getNodeIDfromname(host1);
		int toNode = topology_getNodeIDfromname(host2);
		if((*tt)->next == NULL) {
			if(fromNode < (*tt)->tl.nodeID) {
				Topo_Table* p = (Topo_Table* )malloc(sizeof(Topo_Table));
				p->tl.nodeID = fromNode;
				p->tl.cost = 0;
				p->next = (*tt);
				p->tl.To = (Topo_List* )malloc(sizeof(Topo_Table));
				p->tl.To->nodeID = toNode;
				p->tl.To->cost = cost;
				p->tl.To->To = NULL;
				(*tt) = p;
			}
			else if(fromNode == (*tt)->tl.nodeID) {
				Topo_List* lt = (*tt)->tl.To;
				while(lt->To!=NULL && lt->nodeID!=toNode) {
					lt = lt->To;
				}
				if(lt->To == NULL && lt->nodeID!=toNode) {
					lt->To = (Topo_List* )malloc(sizeof(Topo_Table));
					lt = lt->To;
					lt->nodeID = toNode;
					lt->cost = cost;
					lt->To = NULL;
				}
				else {
					lt->cost = cost;
				}
			}
			else if(fromNode > (*tt)->tl.nodeID) {
				Topo_Table* p = (Topo_Table* )malloc(sizeof(Topo_Table));
				p->tl.nodeID = fromNode;
				p->tl.cost = 0;
				p->next = NULL;
				p->tl.To = (Topo_List* )malloc(sizeof(Topo_Table));
				p->tl.To->nodeID = toNode;
				p->tl.To->cost = cost;
				p->tl.To->To = NULL;
				(*tt)->next = p;
			}
		}
		else {
			Topo_Table* p = *tt;
			while(fromNode > p->tl.nodeID) {
				p = p->next;
			}
			if(p == NULL) {
				p = *tt;
				while(p->next != NULL)
					p = p->next;
				p->next = (Topo_Table* )malloc(sizeof(Topo_Table));
				p = p->next;
				p->tl.nodeID = fromNode;
				p->tl.cost = 0;
				p->next = NULL;
				p->tl.To = (Topo_List* )malloc(sizeof(Topo_Table));
				p->tl.To->nodeID = toNode;
				p->tl.To->cost = cost;
				p->tl.To->To = NULL;
			}
			else {
				if(fromNode == p->tl.nodeID) {
					Topo_List* lt = p->tl.To;
					while(lt->To!=NULL && lt->nodeID!=toNode) {
						lt = lt->To;
					}
					if(lt->To == NULL && lt->nodeID!=toNode) {
						lt->To = (Topo_List* )malloc(sizeof(Topo_Table));
						lt = lt->To;
						lt->nodeID = toNode;
						lt->cost = cost;
						lt->To = NULL;
					}
					else {
						lt->cost = cost;
					}
				}
				else {
					Topo_Table* q = (Topo_Table* )malloc(sizeof(Topo_Table));
					q->tl.nodeID = fromNode;
					q->tl.cost = 0;
					q->tl.To = (Topo_List* )malloc(sizeof(Topo_Table));
					q->tl.To->nodeID = toNode;
					q->tl.To->cost = cost;
					q->tl.To->To = NULL;
					if(p == *tt) {				
						q->next = (*tt);
						(*tt) = q;
					}
					else {
						Topo_Table* s = *tt;
						while(s->next != p)
							s = s->next;
						s->next = q;
						q->next = p;
					}
				}
			}
		}
	
	}
}

void create_topoTable(Topo_Table** tt) {
	if(*tt != NULL)
		return;
	FILE *fp;
	char buf[100],host1[20],host2[20],cost[10];
	char *p;
	memset(buf,0,100);
	memset(host1,0,20);
	memset(host2,0,20);
	memset(cost,0,10);
	if((fp=fopen("./topology/topology.dat","r")) == NULL) {
		printf("Unable to open topology.dat!\n");
		exit(-1);
	}
	do {
		p = fgets(buf,100,fp);
		int i=0;
		while(buf[i] != ' ') {
			host1[i] = buf[i];
			i++;
		}
		int j=i+1;
		while(buf[j] != ' ') {
			host2[j-i-1] = buf[j];
			j++;
		}
		i = j+1;
		while(buf[i] != '\0') {
			cost[i-j-1] = buf[i];
			i++;
		}
		int c = atoi(cost);
		insert_topoTable(tt,host1,host2,c);
		insert_topoTable(tt,host2,host1,c);
		
	}while(p!=NULL);
	fclose(fp);
}


//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) 
{
	if(strcmp(hostname,"csnetlab_1") == 0) {
		return 185;
	}
	else if(strcmp(hostname,"csnetlab_2") == 0) {
		return 186;
	}
	else if(strcmp(hostname,"csnetlab_3") == 0) {
		return 187;
	}
	else if(strcmp(hostname,"csnetlab_4") == 0) {
		return 188;
	}
	printf("No such host: %s!\n",hostname);
	return -1;
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr)
{
	if(addr == NULL) {
		printf("addr is NULL!\n");
		return -1;
	}
	if(addr->s_addr == inet_addr("114.212.190.185")) {
		return 185;
	}
	else if(addr->s_addr == inet_addr("114.212.190.186")) {
		return 186;
	}
	else if(addr->s_addr == inet_addr("114.212.190.187")) {
		return 187;
	}
	else if(addr->s_addr == inet_addr("114.212.190.188")) {
		return 188;
	}
	return -1;
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
{
	  char temp[30];
	  memset(temp,0,30);
	  if(gethostname(temp,30) != 0) {
	  	printf("unable to get host name!\n");
	  	return -1;
	  }
	  return topology_getNodeIDfromname(temp);
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{
	create_topoTable(&tlt);
	int hostID = topology_getMyNodeID();
	int nbnum = 0;
	Topo_Table* p = tlt;
	while(p->tl.nodeID != hostID) {
		p = p->next;
	}
	if(p==NULL) {
		printf("My Node ID no exist!\n");
		return -1;
	}
	Topo_List* r = p->tl.To;
	while(r!=NULL) {
		nbnum++;
		r = r->To;
	}
	return nbnum;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{ 
	create_topoTable(&tlt);
	Topo_Table* p = tlt;
	int nodenum = 0;
	while(p!=NULL) {
		nodenum++;
		p=p->next;
	}
	return nodenum;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID. 
int* topology_getNodeArray()
{
	int n = topology_getNodeNum();
	int *alID = (int *)malloc(n*sizeof(int));
	int i = 0;
	Topo_Table* p = tlt;
	while(p!=NULL) {
		alID[i] = p->tl.nodeID;
		i++;
		p=p->next;
	}
	return alID;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray()
{
	int n = topology_getNbrNum();
	int *nbID = (int *)malloc(n*sizeof(int));
	int hostID = topology_getMyNodeID();
	Topo_Table* p = tlt;
	while(p->tl.nodeID != hostID) {
		p = p->next;
	}
	if(p==NULL) {
		printf("My Node ID no exist!\n");
		return NULL;
	}
	Topo_List* r = p->tl.To;
	int i = 0;
	while(r!=NULL) {
		nbID[i] = r->nodeID;
		r = r->To;
		i++;
	}
	return nbID;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
	create_topoTable(&tlt);
	Topo_Table* p = tlt;
	while(p->tl.nodeID != fromNodeID) {
		p = p->next;
	}
	if(p == NULL) {
		printf("No such fromNodeID: %d\n",fromNodeID);
		return 0;
	}
	Topo_List* r = p->tl.To;
	while(toNodeID != r->nodeID)
		r = r->To;
	if(r != NULL)
		return r->cost;
	else
		return INFINITE_COST;
}

in_addr_t topology_getIP(int NodeID) {
	if(NodeID == 185) {
		return inet_addr("114.212.190.185");
	}
	else if(NodeID == 186) {
		return inet_addr("114.212.190.186");
	}
	else if(NodeID == 187) {
		return inet_addr("114.212.190.187");
	}
	else if(NodeID == 188) {
		return inet_addr("114.212.190.188");
	}
	return 0;
}

