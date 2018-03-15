typedef struct LSP_pair {
    int neighbor;
    long cost;
    struct LSP_pair *next;
    struct LSP_pair *prev;
} LSP_pair;

typedef struct LSP {
    int sender_ID;
    struct LSP_pair *head;
    int sequence_number;
    struct timespec send_time;
    struct LSP *next;
    struct LSP *prev;
} LSP;

typedef struct LSDB {
    int my_ID;
    struct LSP *head;
} LSDB;


LSDB *init_LSDB(int ID);
