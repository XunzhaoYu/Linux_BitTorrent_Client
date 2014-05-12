
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"

//这个函数动态创建距离矢量表.
//距离矢量表包含n+1个条目, 其中n是这个节点的邻居数,剩下1个是这个节点本身.
//距离矢量表中的每个条目是一个dv_t结构,它包含一个源节点ID和一个有N个dv_entry_t结构的数组, 其中N是重叠网络中节点总数.
//每个dv_entry_t包含一个目的节点地址和从该源节点到该目的节点的链路代价.
//距离矢量表也在这个函数中初始化.从这个节点到其邻居的链路代价使用提取自topology.dat文件中的直接链路代价初始化.
//其他链路代价被初始化为INFINITE_COST.
//该函数返回动态创建的距离矢量表.
dv_t* dvtable_create()
{
  	dv_t* head=malloc(sizeof(dv_t));
	dv_entry_t* k=malloc(sizeof(dv_entry_t));
	if(head==NULL||k==NULL){
		printf("heap full!\n");
		exit(-1);			
	}
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
	head->nodeID=k->nodeID=id;
	k->cost=0;
	head->dvEntry=k;
	head->next=NULL;
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
			dv_t* p=(dv_t*)malloc(sizeof(dv_t));
			dv_entry_t* q=(dv_entry_t*)malloc(sizeof(dv_entry_t));	
			if(p==NULL||q==NULL){
				printf("heap full!\n");
				exit(-1);			
			}
			p->nodeID=id;
			p->dvEntry=q;
			q->nodeID=d;
			q->cost=value;
			p->next=head;
			head=p;
		}
	}
  	return head;
}

//这个函数删除距离矢量表.
//它释放所有为距离矢量表动态分配的内存.
void dvtable_destroy(dv_t* dvtable)
{
	while(dvtable!=NULL){
		dv_t* temp=dvtable;
		dvtable=dvtable->next;
		free(temp);
	}
  return;
}

//这个函数设置距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,并且链路代价也被成功设置了,就返回1,否则返回-1.
int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost)
{
	dv_t* head=dvtable;
	dv_t* tail=NULL;
	while(head!=NULL){
		if(head->nodeID==fromNodeID){	
			dv_entry_t* p=head->dvEntry;
			if(toNodeID==p->nodeID){ 
				p->cost=cost;
				return 1;		
			}
		}
		tail=head;
		head=head->next;
	}
	dv_t* p=(dv_t*)malloc(sizeof(dv_t));
	dv_entry_t* q=(dv_entry_t*)malloc(sizeof(dv_entry_t));	
	if(p==NULL||q==NULL){
		printf("heap full!\n");
		exit(-1);			
	}
	p->nodeID=fromNodeID;
	p->dvEntry=q;
	q->nodeID=toNodeID;
	q->cost=cost;
	p->next=NULL;
	tail->next=p;
	return 1;
}

//这个函数返回距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,就返回链路代价,否则返回INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID)
{
	dv_t* head=dvtable;
	while(head!=NULL){
		if(head->nodeID==fromNodeID){	
			dv_entry_t* p=head->dvEntry;
			if(toNodeID==p->nodeID) 
				return p->cost;		
		}
		head=head->next;
	}
  	return INFINITE_COST;
}

//这个函数打印距离矢量表的内容.
void dvtable_print(dv_t* dvtable)
{
	printf("<<dv table start>>\n");
	dv_t* head=dvtable;
	while(head!=NULL){
		dv_entry_t* p=head->dvEntry;
		printf("from:%d to:%d cost:%d \n",head->nodeID,p->nodeID,p->cost);
		head=head->next;
	}
	printf("<<dv table end>>\n");
  return;
}
