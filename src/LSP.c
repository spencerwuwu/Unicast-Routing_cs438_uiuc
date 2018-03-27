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
    LSP_pair *pair = NULL;
    LSP_pair *prev_pair = NULL;
    while (getline(&buff, &size, init_cost) > 0) {
        pair_num++;
        parse_initcost(&neighbor, &cost, buff);
        pair = malloc(sizeof(LSP_pair));
        pair->neighbor = neighbor;
        pair->cost = cost;
        pair->sequence_number = 1;
        pair->alive = 0;
        pair->prev = prev_pair;
        pair->next = NULL;
        if (pair_num == 1) lsp->pair = pair;
        if (prev_pair) prev_pair->next = pair;
        prev_pair = pair;
        pair = pair->next;
    }
    lsp->pair_num = pair_num;
    return lsp;
}

void parse_initcost(int *neighbor, long *cost, char *msg) {
    int neighbor_p = 0;
    long cost_p = 0;
    if (msg[strlen(msg)] == '\n') sscanf(msg, "%d %ld\n", &neighbor_p, &cost_p);
    else sscanf(msg, "%d %ld", &neighbor_p, &cost_p);
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
    sprintf(buff, "fcost%d,%d,%ld,%d,%d", lsp->sender_id, target->neighbor, target->cost, target->sequence_number,target->alive);
    return buff;
}

/* the whole receive functions are wrapped with mutex at 
 * monitor_neighbors.c
 */
void receive_cost(LSDB *my_db, LSP *my_LSP, char *msg) {
    int neighbor;
    long cost;
    int i = 4;
    for ( ; i < 6; i++) 
        neighbor = neighbor * 10 + msg[i] - '0';
    for ( ; i < 10; i++) 
        cost = cost * 10 + msg[i] - '0';
    update_self_lsp(my_db, my_LSP, neighbor, -1, cost, 0);
}

int receive_lsp(LSDB *my_db, LSP *my_LSP, char *msg) {
    int sender_id, neighbor, sequence_num;
    int alive;
    long cost;
    sscanf(msg, "fcost%d,%d,%ld,%d,%d", &sender_id, &neighbor, &cost, &sequence_num, &alive);

    if (neighbor == my_LSP->sender_id)
        update_self_lsp(my_db, my_LSP, sender_id, sequence_num, cost, 1);
    if (sender_id != my_LSP->sender_id) 
        update_LSDB(my_db, sender_id, neighbor, sequence_num, cost, alive);
    return 0;
}

void set_pair_alive(LSDB *my_db, int sender_id, int neighbor) {
    LSP *lsp = get_node(my_db, sender_id);
    LSP_pair *pair = lsp->pair;
    while (pair) {
        if (pair->neighbor == neighbor) { 
            pair->alive = 1;
        }
        pair = pair->next;
    }
}

void set_alive(LSDB *my_db, int id) {
    LSP *lsp = my_db->lsp;
    while (lsp) {
        if (lsp->sender_id == id) {
            lsp->alive = ALIVE;
            return;
        }
        lsp = lsp->next;
    }
}

void update_self_lsp(LSDB *my_db, LSP *my_LSP, int sender_id, int sequence_num, long cost, int alive) {

    LSP_pair *target = my_LSP->pair;
    LSP_pair *prev = NULL;
    while (target) {
        if (target->neighbor == sender_id) {
            if (sequence_num < 0) {
                target->cost = cost;
                target->sequence_number++;
            } else if (sequence_num > target->sequence_number 
                    || (sequence_num == target->sequence_number &&
                        sender_id < my_LSP->sender_id)) {
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
    if (sequence_num >= 0) target->sequence_number = sequence_num;
    else target->sequence_number = 0;
    target->neighbor = sender_id;
    target->cost = cost;
    target->next = NULL;
    target->prev = prev;
    target->alive = alive;
    if (prev) prev->next = target;
    if (!my_LSP->pair) my_LSP->pair = target;

    if (!get_node(my_db, sender_id)) {
        init_lsp(my_db, sender_id);
    }
}

void init_lsp(LSDB *my_db, int sender_id) {
    LSP *lsp = my_db->lsp;
    LSP *prev_lsp = NULL;
    while (lsp) {
        prev_lsp = lsp;
        lsp = lsp->next;
    }
    lsp = malloc(sizeof(LSP));
    lsp->sender_id = sender_id;
    lsp->pair_num = 0;
    lsp->alive = 0;
    lsp->confirmed = 0;
    lsp->pair = 0;
    lsp->prev = NULL;
    lsp->next = NULL;
    lsp->prev = prev_lsp;
    if (!my_db->lsp) my_db->lsp = lsp;
    if (prev_lsp) prev_lsp->next = lsp;
}

void update_LSDB(LSDB *my_db, int sender_id, int neighbor, int sequence_num, long cost, int alive) {

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
                            ((sequence_num == pair->sequence_number) && (sender_id < lsp->sender_id))) {
                        pair->cost = cost;
                        pair->sequence_number = sequence_num;
                    }
                    if (sequence_num > pair->sequence_number) alive = alive;
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
            pair->alive = alive;
            pair->next = NULL;
            if (!lsp->pair) lsp->pair = pair;
            if (prev_pair) prev_pair->next = pair;
            lsp->pair_num++;
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
    if (!my_db->lsp) my_db->lsp = lsp;
    if (prev_lsp) prev_lsp->next = lsp;
    lsp->pair = malloc(sizeof(LSP_pair));
    lsp->pair->neighbor = neighbor;
    lsp->pair->cost = cost;
    lsp->pair->alive = 1;
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
    tentative->end_flag = 0;
    return tentative;
}

void tentative_update(LSP_tentative *tentative, int target, long cost, int neighbor_id) {
    LSP_tentative_node *node = tentative->node;
    LSP_tentative_node *prev = NULL;
    while (node) {
        if (node->target_id == target) {
            if (cost < node->cost ||
                    (cost == node->cost && neighbor_id < node->neighbor_id)) {
                node->cost = cost;
                node->neighbor_id = neighbor_id;
            }
            return;
        }
        prev = node;
        node = node->next;
    }
    // a new target
    node = malloc(sizeof(LSP_tentative_node));
    node->target_id = target;
    node->cost = cost;
    node->neighbor_id = neighbor_id;
    node->prev = prev;
    if (!tentative->node) tentative->node = node;
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
    topo = malloc(sizeof(LSP_topo));
    topo->prev = prev_topo;
    topo->next = NULL;
    topo->target_id = least->target_id;
    topo->cost = least->cost;
    topo->neighbor_id = least->neighbor_id;
    if (!my_db->topo) my_db->topo = topo;
    if (prev_topo) prev_topo->next = topo;

    // popup least
    if (least->prev) least->prev->next = least->next;
    else tentative->node = least->next;

    if (least->next) least->next->prev = least->prev;
    make_node_confirmed(my_db, least->target_id);

    long least_cost = least->cost;
    if (!least->prev && !least->next) {
        free(tentative->node);
        tentative->node = NULL;
    } else {
        free(least);
        least = NULL;
    }

    // Update the nodes of target to tentative
    LSP *lsp = get_node(my_db, topo->target_id);
    LSP_pair *pair = lsp->pair;
    while (pair) {
        if (pair->neighbor != my_db->my_ID &&  pair->alive && 
                !check_node_confirmed(my_db, pair->neighbor))  {
            tentative_update(tentative, pair->neighbor, pair->cost + least_cost, topo->neighbor_id);
        }
        pair = pair->next;
    }
}

void build_topo(LSDB *my_db, LSP *my_LSP) {
    if (my_db->topo) cleanup_topo(my_db);

    LSP_tentative *tentative = init_tentative();

    // Init with local
    struct LSP_pair *pair = my_LSP->pair;
    while (pair) {
        if (pair->alive) {
            tentative_update(tentative, pair->neighbor, pair->cost, pair->neighbor);
        }
        pair = pair->next;
    }

    while (tentative->node) {
        // Find the least and add to confirmed
        pop_and_push_tentative(tentative, my_db);

    }

    free(tentative);

}



void cleanup_topo(LSDB *my_db) {
    // Free the original topo
    LSP_topo *topo = my_db->topo->next;
    LSP_topo *next;
    while (topo) {
        next = topo->next;
        free(topo);
        topo = NULL;
        topo = next;
    }
    free(my_db->topo);
    my_db->topo = NULL;
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
void print_send(int id, const char *buff, int len) {
    fprintf(stderr, "%3d: send ", id);
    int i = 0;
    for( ; i < len; i++) {
        fprintf(stderr, "%c", buff[i]);
    }
    fprintf(stderr, "\n");
}

void print_recv(int id, int from, const char *buff) {
    fprintf(stderr, "%3d f%3d: recv %s\n", id, from, buff);
}

void print_log(int id, const char *buff) {
    fprintf(stderr, "%3d: %s\n", id, buff);
}

void print_db(LSDB *my_db, LSP *my_LSP) {
    fprintf(stderr, "%3d: ", my_LSP->sender_id);
    LSP_pair *pair;

    LSP *lsp = my_db->lsp;
    while (lsp) {
        fprintf(stderr, " [%d:", lsp->sender_id);
        pair = lsp->pair;
        while (pair) {
            if (pair->alive)
                fprintf(stderr, " %d-%ld,", pair->neighbor, pair->cost);
            pair = pair->next;
        }
        fprintf(stderr, "],");
        lsp = lsp->next;
    }
    fprintf(stderr, "\n");
}
