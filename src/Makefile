#all: vec ls

all: ls manager_send

vec: main.c monitor_neighbors.c
	gcc -pthread -o vec_router main.c monitor_neighbors.c

ls: main.c LSP.c monitor_neighbors.c
	gcc -pthread -std=gnu11 -o ls_router LSP.c main.c monitor_neighbors.c

manager_send: manager_send.c
	gcc -pthread -o manager_send manager_send.c

.PHONY: clean
clean:
	rm *.o vec_router ls_router
