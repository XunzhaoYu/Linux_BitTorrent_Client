
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"

//makehash()是由路由表使用的哈希函数.
//它将输入的目的节点ID作为哈希键,并返回针对这个目的节点ID的槽号作为哈希值.
int makehash(int node)
{
  return node % MAX_ROUTINGTABLE_SLOTS;
}

//这个函数动态创建路由表.表中的所有条目都被初始化为NULL指针.
//然后对有直接链路的邻居,使用邻居本身作为下一跳节点创建路由条目,并插入到路由表中.
//该函数返回动态创建的路由表结构.
routingtable_t* routingtable_create()
{
/*<<<<<<< HEAD
	
  	routingtable_t* table=malloc(sizeof(routingtable_t));
	if(table==NULL){
		printf("head full!\n");
		exit(-1);
	}
	memset(table,0,sizeof(routingtable_t));
	FILE* fp;
	if((fp=fopen("topology.dat","r"))==NULL){
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
	while(fscanf(fp,"%s%s%d",src,dest,value)!=EOF){
		int s=topology_getNodeIDfromname(src);
		int d=topology_getNodeIDfromname(dest);
		if(d==id){
			int temp=s;
			s=d;
			d=temp;	
		}
		if(s==id){
			routingtable_entry_t* p=malloc(sizeof(routingtable_entry_t));	
			if(p==NULL){
				printf("heap full!\n");
				exit(-1);			
			}
			p->destNodeID=p->nextNodeID=d;
			p->next=table.hash[makehash(d)];
			table.hash[makehash(d)]=p;
		}
	}
  	return table;*/
	routingtable_t* rt = (routingtable_t*)malloc(sizeof(routingtable_t));
	memset(rt,0,sizeof(routingtable_t));
	int nbr_num = topology_getNbrNum();
	int *nbr_ID = topology_getNbrArray();
	int i;
	for(i=0 ; i<nbr_num; i++)
		routingtable_setnextnode(rt,nbr_ID[i],nbr_ID[i]);
	return rt;
}

//这个函数删除路由表.
//所有为路由表动态分配的数据结构将被释放.
void routingtable_destroy(routingtable_t* routingtable)
{
	routingtable_entry_t** p=routingtable->hash;
	int i;
	for(i=0;i<MAX_ROUTINGTABLE_SLOTS;i++)if(p[i]!=NULL){
		while(p[i]!=NULL){
			routingtable_entry_t* temp=p[i];
			p[i]=p[i]->next;
			free(temp);			
		}	
	} 
	free(routingtable);
  	return;
/*=======
	int key;
	routingtable_entry_t *temp;
	for(key=0 ;key<MAX_ROUTINGTABLE_SLOTS ;key++)
	{
		while(routingtable->hash[key] !=NULL)
		{
			temp = routingtable->hash[key];		
			routingtable->hash[key] = routingtable->hash[key]->next;
			free(temp);
		}	
	}
	free(routingtable);
	return;
>>>>>>> f0f6961d63e6b7b707199d7631c0c16efa908200*/
}

//这个函数使用给定的目的节点ID和下一跳节点ID更新路由表.
//如果给定目的节点的路由条目已经存在, 就更新已存在的路由条目.如果不存在, 就添加一条.
//路由表中的每个槽包含一个路由条目链表, 这是因为可能有冲突的哈希值存在(不同的哈希键, 即目的节点ID不同, 可能有相同的哈希值, 即槽号相同).
//为在哈希表中添加一个路由条目:
//首先使用哈希函数makehash()获得这个路由条目应被保存的槽号.
//然后将路由条目附加到该槽的链表中.
void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID)
{
/*<<<<<<< HEAD
	int pos=makehash(destNodeID);
	routingtable_entry_t* p=routingtable->hash[pos];
	while(p!=NULL)if(p->destNodeID==destNodeID){
		p->nextNodeID=nextNodeID;
		break;	
	}
	if(p==NULL){
		routingtable_entry_t* q=(routingtable_entry_t*)malloc(sizeof(routingtable_entry_t*)));
		if(q==NULL){
			printf("heap full!\n");
			exit(-1);
		}
		q->destNodeID=destNodeID;
		q->nextNodeID=nextNodeID;
		q->next=p;
		p=q;
	}
 	return;
=======*/
	int exist = routingtable_getnextnode(routingtable,destNodeID);
	int key = makehash(destNodeID);
	if(exist == -1)//如果不存在，添加一条路由条目
	{
		//新建条目并初始化
		routingtable_entry_t* new_entry = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
		new_entry->destNodeID = destNodeID;
		new_entry->nextNodeID = nextNodeID;
		new_entry->next = NULL;
		//插入到hash链表中
		new_entry->next = routingtable->hash[key];
		routingtable->hash[key] = new_entry;
	}
	else//如果存在，更新已存在的路由条目
	{
		routingtable_entry_t *temp = routingtable->hash[key];
		while(temp->destNodeID != destNodeID)
			temp = temp->next;
		temp->nextNodeID = nextNodeID;
	}
	return;
//>>>>>>> f0f6961d63e6b7b707199d7631c0c16efa908200
}

//这个函数在路由表中查找指定的目标节点ID.
//为找到一个目的节点的路由条目, 你应该首先使用哈希函数makehash()获得槽号,
//然后遍历该槽中的链表以搜索路由条目.如果发现destNodeID, 就返回针对这个目的节点的下一跳节点ID, 否则返回-1.
int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID)
{
	int key = makehash(destNodeID);	
	routingtable_entry_t *temp = routingtable->hash[key];
	while(temp !=NULL)
	{
		if(temp -> destNodeID == destNodeID)
			return temp -> nextNodeID;
		else
			temp = temp->next;
	}
	return -1;
}

//这个函数打印路由表的内容
void routingtable_print(routingtable_t* routingtable)
{
	printf("<<routingtable start>>\n");
	routingtable_entry_t** p=routingtable->hash;
	int i;
	printf("get here!\n");
	for(i=0;i<MAX_ROUTINGTABLE_SLOTS;i++)if(p[i]!=NULL){
		routingtable_entry_t* temp=p[i];
		while(temp!=NULL){ 
			printf("dest:%d next: %d\n",temp->destNodeID,temp->nextNodeID);
			temp=temp->next;
		}
	}
	printf("<<routingtable end>>\n");
 	return;
}

