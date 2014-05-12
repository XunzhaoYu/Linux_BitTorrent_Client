
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat. 
nbr_cost_entry_t* nbrcosttable_create()
{
  	nbr_cost_entry_t* head=NULL;
	FILE* fp;
	if((fp=fopen("./topology/topology.dat","r"))==NULL){
		printf("can't open file topology.dat!\n");
		return NULL;
	}
	unsigned int id=topology_getMyNodeID();
	if(!id){
		printf("ip not found!\n");
		return NULL;
	}
	char src[20],dest[20];
	int value;
	while(fscanf(fp,"%s%s%d",src,dest,&value)!=EOF){
		int s=topology_getNodeIDfromname(src);
		int d=topology_getNodeIDfromname(dest);
		if(d==id){
			int temp=s;
			s=d;
			d=temp;	
		}
		if(s==id){
			nbr_cost_entry_t* p=malloc(sizeof(nbr_cost_entry_t));	
			if(p==NULL){
				printf("heap full!\n");
				exit(-1);			
			}
			p->nodeID=d;
			p->cost=value;
			p->next=head;
			head=p;
		}
	}
  	return head;
}

//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
	while(nct!=NULL){
		nbr_cost_entry_t* temp=nct;
		nct=nct->next;
		free(temp);
	}
  	return;
}

//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
	while(nct!=NULL){
		if(nct->nodeID==nodeID)
			return nct->cost;
		nct=nct->next;	
	}
  	return INFINITE_COST;
}

//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t* nct)
{
	printf("<<toplogy table>>\n");
	while(nct!=NULL){
		printf("nodeID: %d cost:%d \n",nct->nodeID,nct->cost);
		nct=nct->next;	
	}
	printf("<<toplogy table end>>\n");
  	return;
}


