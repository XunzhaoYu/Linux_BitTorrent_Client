//文件名: topology/topology.h
//
//描述: 这个文件声明一些用于解析拓扑文件的辅助函数 
//
//创建日期: 2013年

#ifndef TOPOLOGY_H 
#define TOPOLOGY_H
#include <netdb.h>

typedef struct TopologyList {
	int nodeID;
	int cost;
	struct TopologyList *To;
}Topo_List;

typedef struct TopologyTable {
	Topo_List tl;
	struct TopologyTable* next;
}Topo_Table;

Topo_Table* tlt;

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname); 

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr);

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID();

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum(); 

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数. 
int topology_getNodeNum();

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID.  
int* topology_getNodeArray(); 

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray(); 

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID);

void create_topoTable(Topo_Table** tt);
void insert_topoTable(Topo_Table** tt,char* host1,char* host2,int cost);
in_addr_t topology_getIP(int NodeID);
#endif
