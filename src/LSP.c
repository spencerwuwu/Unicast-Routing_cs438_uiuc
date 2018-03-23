#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>

#include "LSP.h"

extern pthread_mutex_t mutex;

LSDB *init_LSDB(int ID) {
    LSDB *db = malloc(sizeof(LSDB));
    db->my_ID = ID;
    db->lsp = NULL;
    db->topo = NULL;
    return db;
}

LSP *init_local_LSP(int ID, FILE *init_cost) {
    LSP *lsp = malloc(sizeof(LSP));
    lsp->sender_id = ID;
    lsp->alive = 1;
    lsp->next = NULL;
    lsp->pair = NULL;
    lsp->pair_num = 0;

    int pair_num = 0;
    char *buff = NULL;
    size_t size;
    int neighbor = 0;
    long cost = 0;
    LSP_pair *pair = lsp->pair;
    LSP_pair *prev_pair = NULL;
    while (getline(&buff, &size, init_cost) > 0) {
        pair_num++;
        parse_initcost(&neighbor, &cost, buff);
        pair = malloc(sizeof(LSP_pair));
        pair->neighbor = neighbor;
        pair->cost = cost;
        pair->sequence_number = 0;
        pair->prev = prev_pair;
        pair->next = NULL;
        if (prev_pair) prev_pair->next = pair;
        prev_pair = pair;
        pair = pair->next;
    }
    return lsp;
}

void parse_initcost(int *neighbor, long *cost, char *msg) {
    int neighbor_p = 0;
    long cost_p = 0;
    int i = 0, flag = 0;
    while (msg[i] && msg[i] != '\n') {
        if (flag == 0) {
            if (msg[i] != ' ') {
                flag = 1;
            } else {
                neighbor_p = *neighbor * 10 + msg[i] - '0';
            }
        } else {
            cost_p = cost_p * 10 + msg[i] - '0';
        }
        i++;
    }
    *neighbor = neighbor_p;
    *cost = cost_p;
}

/**
 * message format
 * "msg_type,sender_id,neighbor,cost,sequence_num"
 *
 */
char *create_cost_msg(LSP *lsp, int *index) {
    if (lsp->pair_num == 0) return NULL;
    *index = *index + 1;
    if (*index == lsp->pair_num) *index = 0;
    LSP_pair *target = lsp->pair;
    int i = 0;
    for ( ; i < *index; i++) {
        target = target->next;
    }

    char *buff = calloc(sizeof(char), MSG_SIZE);
    sprintf(buff, "cost%d,%d,%ld,%d", lsp->sender_id, target->neighbor, target->cost, target->sequence_number);
    return buff;
}

int receive_lsp(LSDB *my_db, LSP *my_LSP, char *msg) {
    int sender_id, neighbor, sequence_num;
    long cost;
    sscanf(msg, "cost%d,%d,%ld,%d", &sender_id, &neighbor, &cost, &sequence_num);

    // If the message is originally sent by self, exit
    if (sender_id == my_LSP->sender_id) return 0;

    // If the neighbor is self, mean it is a neighbor
    if (neighbor == my_LSP->sender_id) {
        pthread_mutex_lock(&mutex);
        update_self_lsp(my_LSP, sender_id, neighbor, sequence_num, cost);
        pthread_mutex_unlock(&mutex);
    }
    pthread_mutex_lock(&mutex);
    set_alive(my_db, sender_id);
    update_LSDB(my_db, sender_id, neighbor, sequence_num, cost);
    pthread_mutex_unlock(&mutex);
    return 1;
}
void set_alive(LSDB *my_db, int id) {
    LSP *lsp = my_db->lsp;
    while (lsp) {
        if (lsp->sender_id == id) {
            lsp->alive = ALIVE;
        }
        lsp = lsp->next;
    }
}

void update_self_lsp(LSP *my_LSP, int sender_id, int neighbor, int sequence_num, long cost) {

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

void update_LSDB(LSDB *my_db, int sender_id, int neighbor, int sequence_num, long cost) {

    LSP *lsp = my_db->lsp;
    LSP *prev_lsp = NULL;
    LSP_pair *pair = NULL, *prev_pair = NULL;

    while (lsp) {
        if (lsp->sender_id == sender_id) {
            lsp->alive = ALIVE;
            pair = lsp->pair;
            while (pair) {
                if (pair->neighbor == neighbor) {
                    if (sequence_num > pair->sequence_number || 
                            (sequence_num == pair->sequence_number) && (sender_id < lsp->sender_id)) {
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
    lsp->sender_id = sender_id;
    lsp->pair_num = 1;
    lsp->next = NULL;
    if (prev_lsp) prev_lsp->next = lsp;
    lsp->pair = malloc(sizeof(LSP_pair));
    lsp->pair->neighbor = neighbor;
    lsp->pair->cost = cost;
    lsp->pair->sequence_number = sequence_num;
    lsp->pair->next = NULL;
}

int is_neighbor(LSP *my_LSP, int target) {
    LSP_pair *pair = my_LSP->pair;
    while (pair) {
        if (pair->neighbor == target) return 1;
        pair = pair->next;
    }
    return 0;
}

int LSP_decide(LSDB *my_db, int target) {
    LSP_topo *topo = my_db->topo;
    while (topo) {
        if (topo->target_id == target) return topo->neighbor_id;
        topo = topo->next;
    }
    return -1;
}

LSP_tentative *init_tentative() {
    LSP_tentative *tentative = malloc(sizeof(LSP_tentative));
    tentative->node = NULL;
    return tentative;
}

void tentative_update(LSDB *my_db, LSP_tentative *tentative, int neighbor, long cost, int neighbor_id) {
    LSP_tentative_node *node = tentative->node;
    LSP_tentative_node *prev = NULL;
    while (node) {
        if (node->target_id == neighbor) {
            if (cost < node->cost ||
                    (cost == node->cost && neighbor_id < node->cost)) {
                node->cost = cost;
                node->neighbor_id = neighbor_id;
                return;
            } else {
                prev = node;
                node = node->next;
            }
        }
    }
    // a new target
    node = malloc(sizeof(LSP_tentative_node));
    node->target_id = neighbor;
    node->cost = cost;
    node->neighbor_id = neighbor_id;
    node->prev = prev;
    if (prev) prev->next = node;
    node->next = NULL;
}

void pop_and_push_tentative(LSP_tentative *tentative, LSDB *my_db) {
    // find the least
    LSP_tentative_node *node = tentative->node;
    LSP_tentative_node *least = node;
    while (node) {
        if (node->cost < least->cost) least = node;
        node = node->next;
    }
    // add to confirmed
    LSP_topo *topo = my_db->topo;
    LSP_topo *prev_topo = NULL;
    while (topo) {
        prev_topo = topo;
        topo = topo->next;
    }
    topo = malloc(sizeof(topo));
    topo->prev = prev_topo;
    topo->next = NULL;
    topo->target_id = least->target_id;
    topo->cost = least->cost;
    topo->neighbor_id = least->neighbor_id;
    if (prev_topo) prev_topo->next = topo;

    // popup least
    if (least->prev) least->prev->next = least->next;
    if (least->next) least->next->prev = least->prev;
    make_node_confirmed(my_db, least->target_id);
    free(least);
    least = NULL;

    // Update the nodes of target to tentative
    LSP *lsp = get_node(my_db, topo->target_id);
    LSP_pair *pair = lsp->pair;
    while (pair) {
        if (check_node_alive(my_db, pair->neighbor) && 
                !check_node_confirmed(my_db, pair->neighbor)) 
            tentative_update(my_db, tentative, pair->neighbor, pair->cost, topo->neighbor_id);
        pair = pair->next;
    }
}

void build_topo(LSDB *my_db, LSP *my_LSP) {
    if (my_db->topo) cleanup_topo(my_db);
    LSP_topo *topo = my_db->topo;

    int tentative_num = 0, confirmed_num = 0;
    LSP_tentative *tentative = init_tentative();

    // Init with local
    struct LSP_pair *pair = my_LSP->pair;
    while (pair) {
        if (check_node_alive(my_db, pair->neighbor)) {
            tentative_update(my_db, tentative, pair->neighbor, pair->cost, pair->neighbor);
        }
        pair = pair->next;
    }

    while (!tentative_end(my_db, tentative)) {
        // Find the least and add to confirmed
        pop_and_push_tentative(tentative, my_db);
    }

}

int tentative_end(LSDB *my_db, LSP_tentative *tentative) {
    LSP_tentative_node *node = tentative->node;
    while (node) {
        if (!check_node_confirmed(my_db, node->target_id)) return 0;
        node = node->next;
    }
    return 1;
}

void cleanup_topo(LSDB *my_db) {
    // Free the original topo
    LSP_topo *topo = my_db->topo;
    LSP_topo *next = topo->next;
    while (next) {
        free(topo);
        topo = NULL;
        topo = next;
        next = topo->next;
    }
    if (topo) {
        free(topo);
        topo = NULL;
    }
    // reset LSP->confirmed
    LSP *lsp = my_db->lsp;
    while (lsp) {
        lsp->confirmed = 0;
        lsp = lsp->next;
    }
}

int check_node_alive(LSDB *my_db, int id) {
    LSP *lsp = my_db->lsp;
    while (lsp) {
        if (lsp->sender_id == id) {
            if (lsp->alive) return 1;
            else return 0;
        }
        lsp = lsp->next;
    }
    return 0;
}

void make_node_confirmed(LSDB *my_db, int id) {
    LSP *lsp = my_db->lsp;
    while (lsp) {
        if (lsp->sender_id == id) {
            lsp->confirmed = 1;
            return;
        }
        lsp = lsp->next;
    }
}

int check_node_confirmed(LSDB *my_db, int id) {
    LSP *lsp = my_db->lsp;
    while (lsp) {
        if (lsp->sender_id == id) {
            if (lsp->confirmed) return 1;
            else return 0;
        }
        lsp = lsp->next;
    }
    return 0;
}

LSP *get_node(LSDB *my_db, int id) {
    LSP *lsp = my_db->lsp;
    while (lsp) {
        if (lsp->sender_id == id) {
            return lsp;
        }
        lsp = lsp->next;
    }
    return NULL;
}

/* Debug section */
