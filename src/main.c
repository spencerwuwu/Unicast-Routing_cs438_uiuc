#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "LSP.h"
#include "monitor_neighbors.h"


void listenForNeighbors();
void* announceToNeighbors(void* unusedParam);


int globalMyID = 0;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
struct sockaddr_in globalNodeAddrs[256];

// link state database
LSDB *my_db = NULL;
LSP *my_LSP = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

FILE *init_cost = NULL;
FILE *log_file = NULL;
 
int main(int argc, char** argv)
{
	if(argc != 4)
	{
		fprintf(stderr, "Usage: %s mynodeid initialcostsfile logfile\n\n", argv[0]);
		exit(1);
	}
	
	
	//initialization: get this process's node ID, record what time it is, 
	//and set up our sockaddr_in's for sending to the other nodes.
	globalMyID = atoi(argv[1]);
	int i;
	for(i=0;i<256;i++)
	{
		gettimeofday(&globalLastHeartbeat[i], 0);
		
		char tempaddr[100];
		sprintf(tempaddr, "10.1.1.%d", i);
		memset(&globalNodeAddrs[i], 0, sizeof(globalNodeAddrs[i]));
		globalNodeAddrs[i].sin_family = AF_INET;
		globalNodeAddrs[i].sin_port = htons(7777);
		inet_pton(AF_INET, tempaddr, &globalNodeAddrs[i].sin_addr);
	}
	
	
	//TODO: read and parse initial costs file. default to cost 1 if no entry for a node. file may be empty.
	
	
	//socket() and bind() our socket. We will do all sendto()ing and recvfrom()ing on this one.
	if((globalSocketUDP=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		exit(1);
	}
	char myAddr[100];
	struct sockaddr_in bindAddr;
	sprintf(myAddr, "10.1.1.%d", globalMyID);	
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(7777);
	inet_pton(AF_INET, myAddr, &bindAddr.sin_addr);
	if(bind(globalSocketUDP, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		close(globalSocketUDP);
		exit(1);
	}
	
    // Init local LSP, LSDB, file I/O
    init_cost = fopen(argv[2], "r");
    log_file = fopen(argv[3], "w");
    fclose(log_file);
    log_file = fopen(argv[3], "a+");
    my_LSP = init_local_LSP(globalMyID, init_cost);
    my_db = init_LSDB(globalMyID);
    my_db->lsp = my_LSP;
    
	
	//start threads... feel free to add your own, and to remove the provided ones.
	pthread_t announcerThread;
	pthread_create(&announcerThread, 0, announceToNeighbors, (void*)0);
	
	
	pthread_t checkAlive;
	pthread_create(&checkAlive, 0, calculate_neighbor_alive, (void*)0);
	
	
	//good luck, have fun!
	listenForNeighbors();
	
	
	
}
