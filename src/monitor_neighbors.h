void hackyBroadcast(const char* buf, int length);
void* announceToNeighbors(void* unusedParam);
void listenForNeighbors();

void decrease_alive();

/* log file format */
void log_send(int target, char *buff);
void log_recv(char *buff);
void log_forward(int target, int next_hop, char *buff);
void log_failed(int target);
