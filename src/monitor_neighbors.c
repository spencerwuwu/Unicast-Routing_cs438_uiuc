#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <time.h>
#include "LSP.h"
#include "monitor_neighbors.h"

extern int globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];

extern LSDB *my_db;
extern LSP *my_LSP;
extern pthread_mutex_t mutex;
extern FILE *log_file;


//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void* announceToNeighbors(void* unusedParam)
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
    int index = 0;
    char *buff = NULL;
	while(1) {
        pthread_mutex_lock(&mutex);
        buff = create_msg(my_LSP, &index);
        pthread_mutex_unlock(&mutex);
		hackyBroadcast(buff, strlen(buff) + 1);
		nanosleep(&sleepFor, 0);
        free(buff);
        buff = NULL;
	}
}

void listenForNeighbors()
{
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[1000];

	int bytesRecvd;
	while(1)
	{
		theirAddrLen = sizeof(theirAddr);
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0, 
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
		
		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);
		
		short int heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
			
			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.
			
			//record that we heard from heardFrom just now.
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
		}
		
		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp(recvBuf, "send", 4)) {
			//TODO send the requested message to the requested destination node
			// ...
		} else if(!strncmp(recvBuf, "cost", 4)) {
		    //'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			// ...
            int neighbor = 0;
            long cost = 0;
            int i = 4;
            for ( ; i < 10; i++) {
                if (i < 6) { 
                    neighbor = neighbor * 10 + recvBuf[i] - '0';
                } else {
                    cost = cost * 10 + recvBuf[i] - '0';
                }
            }
            pthread_mutex_lock(&mutex);
            my_LSP->sequence_num++;
            update_self_lsp(my_LSP, 0, neighbor, my_LSP->sequence_num, cost);
            pthread_mutex_unlock(&mutex);
        } else if (!strncmp(recvBuf, "0,", 2)) {
            //TODO now check for the various types of packets you use in your own protocol
            //else if(!strncmp(recvBuf, "your other message types", ))
            // ... 
            if (receive_lsp(my_db, my_LSP, recvBuf)) {
                hackyBroadcast(recvBuf, strlen(recvBuf) + 1);
            }
        } else if(!strncmp(recvBuf, "forward", 7)) {
        } else if(!strncmp(recvBuf, "alive", 5)) {
        } else if(!strncmp(recvBuf, "pass", 4)) {
        } else if(!strncmp(recvBuf, "ack", 3)) {
        }
    }
    //(should never reach here)
    close(globalSocketUDP);
}
