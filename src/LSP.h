#define MSG_SIZE 30 
#define ALIVE 30 

typedef struct LSP_pair {
    int neighbor;
    long cost;
    int sequence_number;
    struct LSP_pair *next;
    struct LSP_pair *prev;
} LSP_pair;

typedef struct LSP {
    int sender_id;
    int pair_num;
    int alive;
    int confirmed;
    struct LSP_pair *pair;
    struct LSP *prev;
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
} LSP_tentative;

LSDB *init_LSDB(int ID);
LSP *init_local_LSP(int ID, FILE *init_cost);

void parse_initcost(int *neighbor, long *cost, char *msg);

/* Create msg for the index th pair in self's LSP.
 * Round back if index reaches pair_num
 */
char *create_cost_msg(LSP *lsp, int *index);

/* Receive and update LSP or LSDB
 * If the message if originally sent by oneself, return 0.
 * Else return 1
 */
int receive_lsp(LSDB *db, LSP *my_LSP, char *msg);
int receive_neighbor_lsp(LSDB *my_db, LSP *my_LSP, char *msg);
void receive_cost(LSDB *db, LSP *my_LSP, char *msg);

void update_self_lsp(LSDB *my_db, LSP *my_LSP, int sender_id, int sequence_num, long cost);

void update_LSDB(LSDB *my_db, int sender_id, int neighbor, int sequence_num, long cost);

int is_neighbor(LSP *my_LSP, int target);

void set_alive(LSDB *my_db, int sender_ID);

/* LSP requried function */
int LSP_decide(LSDB *my_db, int target);

// Topo build entry
void build_topo(LSDB *my_db, LSP *my_LSP);
LSP_tentative *init_tentative();
void cleanup_topo(LSDB *my_db);
int check_node_alive(LSDB *my_db, int id);
int check_node_confirmed(LSDB *my_db, int id);
void make_node_confirmed(LSDB *my_db, int id);
LSP *get_node(LSDB *my_db, int id);
void tentative_update(LSDB *my_db, LSP_tentative *tentative, int neighbor, long cost, int neighbor_id);
void pop_and_push_tentative(LSP_tentative *tentative, LSDB *my_db);
int tentative_end(LSDB *my_db, LSP_tentative *tentative);

/* Debug function */
