//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API
//
//创建日期: 2013年1月

#include "neighbortable.h"
#include "../topology/topology.h"
#include "stdlib.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../common/constants.h"
#include "stdio.h"
#include <unistd.h>

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t* nt_create()
{
	tlt = NULL;
	create_topoTable(&tlt);
	nbr_entry_t* nbr = NULL;
	int n = topology_getNbrNum();
	int hostID = topology_getMyNodeID();
	nbr = (nbr_entry_t* )malloc(n*sizeof(nbr_entry_t));
	Topo_Table* p = tlt;
	while(p->tl.nodeID != hostID)
		p = p->next;
	if(p == NULL) {
		printf("Unable to find host in topology!\n");
		exit(-1);
	}
	Topo_List* r = p->tl.To;
	int i=0;
	while(i<n) {
		nbr[i].nodeID = r->nodeID;
		nbr[i].nodeIP = topology_getIP(r->nodeID);
		nbr[i].conn = -1;
		i++;
		r = r->To;
	}
	return nbr;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt)
{
	if(nt != NULL) {
		int i=0;
		int n = topology_getNbrNum();
		while(i<n) {
			if(nt[i].conn>=0) {
				close(nt[i].conn);
			}
			i++;
		}
		free(nt);
		nt = NULL;
	}
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{
	int n = topology_getNbrNum();
	int i = 0;
	while(i < n) {
		if(nt[i].nodeID == nodeID) {
			break;
		}
		i++;
	}
	if(i == n) {
		printf("No such nodeID: %d\n",nodeID);
		return -1;
	}
	nt[i].conn = conn;
	return 1;
}
