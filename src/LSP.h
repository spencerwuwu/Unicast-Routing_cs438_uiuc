#define MSG_SIZE 20

typedef struct LSP_pair {
    int neighbor;
    long cost;
    int sequence_number;
    struct LSP_pair *next;
} LSP_pair;

typedef struct LSP {
    int sender_ID;
    int pair_num;
    int alive;
    struct LSP_pair *pair;
    struct LSP *next;
} LSP;

typedef struct LSDB {
    int my_ID;
    LSP *lsp;
} LSDB;


LSDB *init_LSDB(int ID);
LSP *init_local_LSP(int ID);

/* Create msg for the index th pair in self's LSP.
 * Round back if index reaches pair_num
 */
char *create_msg(LSP *lsp, int *index);

/* Receive and update LSP or LSDB
 * If the message if originally sent by oneself, return 0.
 * Else return 1
 */
int receive_lsp(LSDB *db, LSP *my_LSP, char *msg);

void update_self_lsp(LSP *my_LSP, char *msg);

void update_LSDB(LSDB *my_db, char *msg);
