#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>

#include "LSP.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

LSDB *init_LSDB(int ID) {
    LSDB *db = malloc(sizeof(LSDB));
    db->my_ID = ID;
    db->lsp = NULL;
    return db;
}

LSP *init_local_LSP(int ID) {
    LSP *lsp = malloc(sizeof(LSP));
    lsp->sender_ID = ID;
    lsp->pair = NULL;
    lsp->pair_num = 0;
    lsp->alive = 1;
    lsp->next = NULL;
    return lsp;
}

/**
 * message format
 * "msg_type,sender_id,neighbor,cost,sequence_num"
 *
 */
char *create_msg(LSP *lsp, int *index) {
    if (lsp->pair_num == 0) return NULL;
    *index = *index + 1;
    if (*index == lsp->pair_num) *index = 0;
    LSP_pair *target = lsp->pair;
    int i = 0;
    for ( ; i < *index; i++) {
        target = target->next;
    }

    char *buff = calloc(sizeof(char), MSG_SIZE);
    sprintf(buff, "0,%d,%d,%ld,%d", lsp->sender_ID, target->neighbor, target->cost, target->sequence_number);
    return buff;
}

int receive_lsp(LSDB *my_db, LSP *my_LSP, char *msg) {
    int type, sender_id, neighbor, sequence_num;
    long cost;
    sscanf(msg, "%d,%d,%d,%ld,%d", &type, &sender_id, &neighbor, &cost, &sequence_num);

    // If the message is originally sent by self, exit
    if (sender_id == my_LSP->sender_ID) return 0;

    // If the neighbor is self, mean it is a neighbor
    if (neighbor == my_LSP->sender_ID) {
        pthread_mutex_lock(&mutex);
        update_self_lsp(my_LSP, msg);
        pthread_mutex_unlock(&mutex);
    } else {
        pthread_mutex_lock(&mutex);
        update_LSDB(my_db, msg);
        pthread_mutex_unlock(&mutex);
    }
    return 1;
}

void udpate_self_lsp(LSP *my_LSP, char *msg) {
    int type, sender_id, neighbor, sequence_num;
    long cost;
    sscanf(msg, "%d,%d,%d,%ld,%d", &type, &sender_id, &neighbor, &cost, &sequence_num);

    LSP_pair *target = my_LSP->pair;
    LSP_pair *prev = NULL;
    while (target) {
        if (target->neighbor == sender_id) {
            if (sequence_num >= target->sequence_number) {
                target->cost = cost;
                target->sequence_number = sequence_num;
            }
            return;
        }
        prev = target;
        target = target->next;
    }
    // If not found, means it's a new neighbor
    my_LSP->pair_num++;
    target = malloc(sizeof(LSP_pair));
    target->sequence_number = sequence_num;
    target->neighbor = sender_id;
    target->cost = cost;
    target->next = NULL;
    if (prev) prev->next = target;
}

void udpate_LSDB(LSDB *my_db, char *msg) {
    int type, sender_id, neighbor, sequence_num;
    long cost;
    sscanf(msg, "%d.%d,%d,%ld,%d", &type, &sender_id, &neighbor, &cost, &sequence_num);

    LSP *lsp = my_db->lsp;
    LSP *prev_lsp = NULL;
    LSP_pair *pair = NULL, *prev_pair = NULL;

    while (lsp) {
        if (lsp->sender_ID == sender_id) {
            lsp->alive = 1;
            pair = lsp->pair;
            while (pair) {
                if (pair->neighbor == neighbor) {
                    if (sequence_num > pair->sequence_number || 
                            (sequence_num == pair->sequence_number) && (sender_id < lsp->sender_ID)) {
                        pair->cost = cost;
                        pair->sequence_number = sequence_num;
                    }
                    return ; 
                }
                prev_pair = pair;
                pair = pair->next;
            }
            // Insert new pair
            pair = malloc(sizeof(LSP_pair));
            pair->neighbor = neighbor;
            pair->sequence_number = sequence_num;
            pair->cost = cost;
            pair->next = NULL;
            if (prev_pair) prev_pair->next = pair;
            return;
        }
        prev_lsp = lsp;
        lsp = lsp->next;
    }

    // Insert new lsp
    lsp = malloc(sizeof(LSP));
    lsp->sender_ID = sender_id;
    lsp->pair_num = 1;
    lsp->alive = 1;
    lsp->next = NULL;
    if (prev_lsp) prev_lsp->next = lsp;
    lsp->pair = malloc(sizeof(LSP_pair));
    lsp->pair->neighbor = neighbor;
    lsp->pair->cost = cost;
    lsp->pair->sequence_number = sequence_num;
    lsp->pair->next = NULL;
}

