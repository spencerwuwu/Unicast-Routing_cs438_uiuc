#include <stdlib.h>
#include <stdio.h>

int main() {
    FILE *file = fopen("testtopo.txt", "w+");
    int i = 0;
    for (; i <= 55; i++) {
        if (i % 8 != 7) {
            printf("%d ", i);
            fprintf(file, "%d %d\n", i, i+1);
            fprintf(file, "%d %d\n", i, i+8);
        } else {
            fprintf(file, "%d %d\n", i, i+8);
            printf("%d\n", i);
        }
    }
    for (i = 56; i <= 62; i++) {
        printf("%d ", i);
        fprintf(file, "%d %d\n", i, i+1);
    }
    printf("63\n");

    fclose(file);
    return 0;
}
