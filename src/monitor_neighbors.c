#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
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
    //print_send(globalMyID, buf, length);
}

void* announceToNeighbors(void* unusedParam)
{
    char alive_msg[100];
    sprintf(alive_msg, "alive%03d", globalMyID);
    struct timespec sleepFor;
    sleepFor.tv_sec = 1;
    sleepFor.tv_nsec = 30 * 1000 * 1000; //30 ms

    struct timespec sleepInst;
    sleepInst.tv_sec = 0;
    sleepInst.tv_nsec = 3 * 1000 * 1000; //5 ms
    int index = 0;
    char *buff = NULL;
    LSP *lsp = NULL;
    // TODO
    while(1) {
        /*
        fprintf(stderr, "announce:");
        if (globalMyID == 5)
            print_db(my_db, my_LSP);
        */
        lsp = my_db->lsp;
        while (lsp) {
            do {
                pthread_mutex_lock(&mutex);
                buff = create_cost_msg(lsp, &index);
                pthread_mutex_unlock(&mutex);
                if (buff) {
                    hackyBroadcast(buff, strlen(buff) + 1);
                    free(buff);
                    buff = NULL;
                }
                nanosleep(&sleepInst, 0);
            } while (index != 0);
            /*
            pthread_mutex_lock(&mutex);
            buff = create_long_cost_msg(lsp);
            //if (globalMyID == 5) fprintf(stderr, "%s\n", buff);
            pthread_mutex_unlock(&mutex);
            hackyBroadcast(buff, strlen(buff) + 1);
            free(buff);
            buff = NULL;
            */
            lsp = lsp->next;
        }
        hackyBroadcast(alive_msg, strlen(alive_msg) + 1);
        //nanosleep(&sleepFor, 0);
        //sleep(1);

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
            if (heardFrom >= 0) {
                gettimeofday(&globalLastHeartbeat[heardFrom], 0);

                if (!is_neighbor(my_LSP, heardFrom)) {
                    update_self_lsp(my_db, my_LSP, heardFrom, 0, 1, 1);
                }
                if (!get_node(my_db, heardFrom)) {
                    init_lsp(my_db, heardFrom);
                }
                set_pair_alive(my_db, globalMyID, heardFrom);
            }
        }

        //Is it a packet from the manager? (see mp2 specification for more details)
        //send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
        if(!strncmp(recvBuf, "send", 4)) {
            //TODO send the requested message to the requested destination node
            // ...
            //print_recv(globalMyID, heardFrom, recvBuf);
            //print_send(globalMyID, recvBuf, bytesRecvd);
            int16_t target = (recvBuf[4] << 8) + (recvBuf[5] & 0xff);

            pthread_mutex_lock(&mutex);
            build_topo(my_db, my_LSP);
            pthread_mutex_unlock(&mutex);

            if (target == (int16_t)globalMyID) {
                log_recv(recvBuf, bytesRecvd);
            } else {
                int next_hop = LSP_decide(my_db, target);
                if (next_hop >= 0) {
                    unsigned char *send_buf = malloc(bytesRecvd + 1);
                    send_buf[0] = 'f';
                    int send_i = 1;
                    for ( ;send_i <= bytesRecvd; send_i++)
                        send_buf[send_i] = recvBuf[send_i - 1];
                    sendto(globalSocketUDP, send_buf, bytesRecvd + 1, 0, 
                            (struct sockaddr*)&globalNodeAddrs[next_hop], 
                            sizeof(globalNodeAddrs[next_hop]));
                    log_send(target, next_hop, recvBuf, bytesRecvd);
                } else {
                    log_failed(target);
                }
            }
        }else if(!strncmp(recvBuf, "fsend", 5)) {
            int16_t target = (recvBuf[5] << 8) + (recvBuf[6] & 0xff);

            pthread_mutex_lock(&mutex);
            build_topo(my_db, my_LSP);
            pthread_mutex_unlock(&mutex);
            if (target == (int16_t)globalMyID) {
                log_recv_new(recvBuf, bytesRecvd);
            } else {
                /*
                   pthread_mutex_lock(&mutex);
                   build_topo_backward(my_db, target);
                   pthread_mutex_unlock(&mutex);
                   int next_hop = LSP_decide_backward(my_db, target);
                   */
                int next_hop = LSP_decide(my_db, target);
                if (next_hop >= 0) {
                    sendto(globalSocketUDP, recvBuf, bytesRecvd, 0, 
                            (struct sockaddr*)&globalNodeAddrs[next_hop], 
                            sizeof(globalNodeAddrs[next_hop]));
                    log_forward_new(target, next_hop, recvBuf, bytesRecvd);
                } else {
                    log_failed(target);
                }
            }
        } else if(!strncmp(recvBuf, "cost", 4)) {
            //'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
            //TODO record the cost change (remember, the link might currently be down! in that case,
            //this is the new cost you should treat it as having once it comes back up.)
            // ...
            int16_t target = (recvBuf[4] << 8) + (recvBuf[5] & 0xff);
            int32_t cost = (recvBuf[6] << 24) + (recvBuf[7] << 16) + (recvBuf[8] << 8) + (recvBuf[9] & 0xff);
            pthread_mutex_lock(&mutex);
            update_self_lsp(my_db, my_LSP, target, -1, cost, 0);
            pthread_mutex_unlock(&mutex);
            print_db(my_db, my_LSP);

        } else if(!strncmp(recvBuf, "fcost", 5)) {
            pthread_mutex_lock(&mutex);
               receive_lsp(my_db, my_LSP, recvBuf);
            pthread_mutex_unlock(&mutex);
            /*
            pthread_mutex_lock(&mutex);
            receive_long_lsp(my_db, my_LSP, recvBuf, bytesRecvd);
            pthread_mutex_unlock(&mutex);
               */
        }
    }
    //(should never reach here)
    close(globalSocketUDP);
}
// TODO initial of cost 1

void *calculate_neighbor_alive() {
    struct timeval current;
    LSP *lsp;
    struct timespec sleepFor;
    sleepFor.tv_sec = 1;
    sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
    while (1) {
        //sleep(1);
        nanosleep(&sleepFor, 0);
        gettimeofday(&current, NULL); 
        pthread_mutex_lock(&mutex);
        LSP_pair *pair = my_LSP->pair;
        while (pair) {
            if (current.tv_sec - globalLastHeartbeat[pair->neighbor].tv_sec > 1) {
                if (pair->alive) { // if pair is alive and pair is lost for 1 sec
                    pair->alive = 0;
                    pair->sequence_number++;;
                }

                // set up for neighbor db
                lsp = get_node(my_db, pair->neighbor);
                LSP_pair *tmp = get_pair(lsp, globalMyID);
                if (pair) {
                    if (pair->alive) {
                        pair->alive = 0;
                        pair->sequence_number++;;
                    }
                }

            } else { // if pair is dead
                if (!pair->alive) {
                    pair->alive = 1;
                    pair->sequence_number++;;
                }

                // set up for neighbor db
                lsp = get_node(my_db, pair->neighbor);
                LSP_pair *tmp = get_pair(lsp, globalMyID);
                if (pair) {
                    if (!pair->alive) {
                        pair->alive = 1;
                        pair->sequence_number++;;
                    }
                }

            }
            pair = pair->next;
        }
        pthread_mutex_unlock(&mutex);
    }
}

void log_send(int target, int next, unsigned char *buff, int length) {
    fprintf(stderr, "%d: ", globalMyID);
    fprintf(log_file, "sending packet dest %d nexthop %d message ", target, next);
    fprintf(stderr, "sending packet dest %d nexthop %d message ", target, next);
    int i = 6;
    for ( ; i < length; i++) {
        fprintf(log_file, "%c", buff[i]);
        fprintf(stderr, "%c", buff[i]);
    }
    fprintf(log_file, "\n");
    fprintf(stderr, "\n");
    fflush(log_file);
}

void log_recv(char *buff, int length) {
    fprintf(log_file, "receive packet message ");
    //fprintf(stderr, "%d: ", globalMyID);
    //fprintf(stderr, "receive packet message ");
    int i = 6;
    for ( ; i < length; i++) {
        fprintf(log_file, "%c", buff[i]);
        //fprintf(stderr, "%c", buff[i]);
    }
    fprintf(log_file, "\n");
    //fprintf(stderr, "\n");
    fflush(log_file);
}

void log_recv_new(char *buff, int length) {
    fprintf(log_file, "receive packet message ");
    //fprintf(stderr, "%d: ", globalMyID);
    //fprintf(stderr, "receive packet message ");
    int i = 7;
    for ( ; i < length; i++) {
        fprintf(log_file, "%c", buff[i]);
        //fprintf(stderr, "%c", buff[i]);
    }
    fprintf(log_file, "\n");
    //fprintf(stderr, "\n");
    fflush(log_file);
}

void log_forward(int target, int next_hop, unsigned char *buff, int length) {
    fprintf(log_file, "forward packet dest %d nexthop %d message ", target, next_hop);
    //fprintf(stderr, "%d: ", globalMyID);
    //fprintf(stderr, "forward packet dest %d nexthop %d message ", target, next_hop);
    int i = 6;
    for ( ; i < length; i++) {
        fprintf(log_file, "%c", buff[i]);
        //fprintf(stderr, "%c", buff[i]);
    }
    fprintf(log_file, "\n");
    //fprintf(stderr, "\n");
    fflush(log_file);
}

void log_forward_new(int target, int next_hop, unsigned char *buff, int length) {
    fprintf(log_file, "forward packet dest %d nexthop %d message ", target, next_hop);
    //fprintf(stderr, "%d: ", globalMyID);
    //fprintf(stderr, "forward packet dest %d nexthop %d message ", target, next_hop);
    int i = 7;
    for ( ; i < length; i++) {
        fprintf(log_file, "%c", buff[i]);
        //fprintf(stderr, "%c", buff[i]);
    }
    fprintf(log_file, "\n");
    //fprintf(stderr, "\n");
    fflush(log_file);
}

void log_failed(int target) {
    fprintf(log_file, "unreachable dest %d\n", target);
    //fprintf(stderr, "%d: ", globalMyID);
    //fprintf(stderr, "unreachable dest %d\n", target);
    fflush(log_file);
}
