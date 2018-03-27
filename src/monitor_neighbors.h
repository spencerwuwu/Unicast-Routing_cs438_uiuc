void hackyBroadcast(const char* buf, int length);
void* announceToNeighbors(void* unusedParam);
void listenForNeighbors();

void calculate_neighbor_alive();

/* log file format */
void log_send(int target, unsigned char *buff, int length);
void log_recv(char *buff, int length);
void log_forward(int target, int next_hop, unsigned char *buff, int length);
void log_failed(int target);
