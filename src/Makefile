#all: vec ls
all: ls

vec: main.c monitor_neighbors.c
	gcc -pthread -o vec_router main.c monitor_neighbors.c

ls: main.c LSP.c monitor_neighbors.c
	gcc -pthread -o ls_router LSP.c main.c monitor_neighbors.c

.PHONY: clean
clean:
	rm *.o vec_router ls_router
