#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

typedef struct {
    int digits;
    char id[64];
    int completed;
    char result[1024];
} Task;

Task task;

int is_prime(unsigned long num) {
    if (num < 2) return 0;
    for (unsigned long i = 2; i * i <= num; i++) {
        if (num % i == 0) return 0;
    }
    return 1;
}

void *generate_prime(void *arg) {
    int digits = *((int *)arg);
    unsigned long start = 1;
    for (int i = 1; i < digits; i++) start *= 10;
    unsigned long end = start * 10;
    for (unsigned long i = start; i < end; i++) {
        if (is_prime(i)) {
            snprintf(task.result, sizeof(task.result), "%lu", i);
            break;
        }
    }
    task.completed = 1;
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 2 && argc != 3) {
        printf("Usage: %s <generate|status> [digits|id]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "generate") == 0 && argc == 3) {
        task.digits = atoi(argv[2]);
        strcpy(task.id, "task123");
        task.completed = 0;
        pthread_t tid;
        pthread_create(&tid, NULL, generate_prime, &task.digits);
        printf("Task ID: %s\n", task.id);
    } else if (strcmp(argv[1], "status") == 0 && argc == 3) {
        if (strcmp(argv[2], task.id) == 0) {
            if (task.completed) {
                printf("Status: completed\nPrime: %s\n", task.result);
            } else {
                printf("Status: pending\n");
            }
        } else {
            printf("Task not found.\n");
        }
    } else {
        printf("Invalid arguments.\n");
        return 1;
    }
    return 0;
}
