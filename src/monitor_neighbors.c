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
        if(i != globalMyID) {
            //(although with a real broadcast you would also get the packet yourself)
            sendto(globalSocketUDP, buf, length, 0,
                    (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
        }
}

void* announceToNeighbors(void* unusedParam)
{
    char alive_msg[100];
    sprintf(alive_msg, "alive%d", globalMyID);
    struct timespec sleepFor;
    sleepFor.tv_sec = 0;
    sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms

    struct timespec sleepInst;
    sleepInst.tv_sec = 0;
    sleepInst.tv_nsec = 30 * 1000 * 1000; //30 ms
    int index = 0;
    char *buff = NULL;
    while(1) {
        while (index != 0) {
            pthread_mutex_lock(&mutex);
            buff = create_cost_msg(my_LSP, &index);
            pthread_mutex_unlock(&mutex);
            hackyBroadcast(buff, strlen(buff) + 1);
            free(buff);
            buff = NULL;
            if (index == 0) 
                hackyBroadcast(alive_msg, strlen(alive_msg) + 1);
            nanosleep(&sleepInst, 0);
        }
        nanosleep(&sleepFor, 0);
        pthread_mutex_lock(&mutex);
        decrease_alive();
        pthread_mutex_unlock(&mutex);
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
            int target = 0;
            int i = 4;
            for ( ; i < 6; i++) 
                target = target * 10 + recvBuf[i] - '0';
            int next_hop = LSP_decide(my_db, target);
            if (next_hop == globalMyID) {
                log_recv(recvBuf + 6);
            }
            if (next_hop >= 0) {
                if (is_neighbor(my_LSP, next_hop)) {
                    log_send(target, recvBuf + 6);
                } else {
                    log_forward(target, next_hop, recvBuf + 6);
                }
                sendto(globalSocketUDP, recvBuf, strlen(recvBuf) + 1, 0, 
                        (struct sockaddr*)&globalNodeAddrs[next_hop], 
                        sizeof(globalNodeAddrs[next_hop]));
            } else {
                log_failed(target);
            }
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
            if (receive_lsp(my_db, my_LSP, recvBuf))
                hackyBroadcast(recvBuf, 10);
            pthread_mutex_unlock(&mutex);
        } else if(!strncmp(recvBuf, "alive", 5)) {
            int target = 0;
            int i = 5;
            for ( ; i < 7; i++) 
                target = target * 10 + recvBuf[i] - '0';
            set_alive(my_db, target);
        }
    }
    //(should never reach here)
    close(globalSocketUDP);
}
// TODO initial of cost 1

void decrease_alive() {
    LSP *lsp = my_db->lsp;
    while (lsp) {
        if (lsp->alive != 0) lsp->alive--;
        lsp = lsp->next;
    }
    build_topo(my_db, my_LSP);
}

void log_send(int target, char *buff) {
    fprintf(log_file, "sending packet dest %d message %s\n", target, buff);
}

void log_recv(char *buff) {
    fprintf(log_file, "receive packet message %s\n", buff);
}

void log_forward(int target, int next_hop, char *buff) {
    fprintf(log_file, "forward packet dest %d nexthop %d message %s\n", target, next_hop, buff);
}

void log_failed(int target) {
    fprintf(log_file, "unreachable dest %d\n", target);
}
