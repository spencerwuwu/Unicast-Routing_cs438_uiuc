#define MSG_SIZE 20

typedef struct LSP_pair {
    int neighbor;
    long cost;
    int sequence_number;
    struct LSP_pair *next;
} LSP_pair;

typedef struct LSP {
    int sender_id;
    int pair_num;
    int alive;
    int confirmed;
    struct LSP_pair *pair;
    struct LSP *next;
} LSP;

typedef struct LSP_topo {
    int target_id;
    long cost;
    int neighbor_id;
    struct LSP_topo *next;
    struct LSP_topo *prev;
} LSP_topo;

typedef struct LSDB {
    int my_ID;
    int lsp_num;
    LSP *lsp;
    LSP_topo *topo;
} LSDB;

typedef struct LSP_tentative_node {
    int target_id;
    long cost;
    int neighbor_id;
    struct LSP_tentative_node *next;
    struct LSP_tentative_node *prev;
} LSP_tentative_node;

typedef struct LSP_tentative {
    LSP_tentative_node *node;
    int size;
} LSP_tentative;

LSDB *init_LSDB(int ID);
LSP *init_local_LSP(int ID, FILE *init_cost);

void parse_initcost(int *neighbor, long *cost, char *msg);

/* Create msg for the index th pair in self's LSP.
 * Round back if index reaches pair_num
 */
char *create_msg(LSP *lsp, int *index);

/* Receive and update LSP or LSDB
 * If the message if originally sent by oneself, return 0.
 * Else return 1
 */
int receive_lsp(LSDB *db, LSP *my_LSP, char *msg);

void update_self_lsp(LSP *my_LSP, int sender_id, int neighbor, int sequence_num, long cost);

void update_LSDB(LSDB *my_db, LSDB *my_db, int sender_id, int neighbor, int sequence_num, long cost);

/* LSP requried function */
LSP_tentative *init_intative();
int parse_send_msg(char *buff);
int LSP_decide(int target);

int build_topo(LSDB *my_db);
void cleanup_topo(LSDB *my_db);
int check_node_alive(int id);
int check_node_confirmed(int id);
void make_node_confirmed(int id);
LSP *get_node(int id);
void tentative_update(LSP_tentative *tentative, int neighbor, long cost, int neighbor_id);

void pop_and_push_tentative(LSP_tentative *tentative, LSDB *my_db);
