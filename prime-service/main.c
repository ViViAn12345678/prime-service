#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#define PORT 9090
#define MAX_TASKS 100
#define MAX_DIGITS 20

typedef struct {
    char id[32];
    int digits;
    int completed;
    unsigned long long result;
    pthread_t thread;
} task_t;

task_t tasks[MAX_TASKS];
int task_count = 0;
pthread_mutex_t tasks_mutex = PTHREAD_MUTEX_INITIALIZER;

// Función para verificar si un número es primo (optimizada para enteros < 2^64)
int is_prime(unsigned long long n) {
    if (n < 2) return 0;
    if (n % 2 == 0 && n != 2) return 0;
    for (unsigned long long i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}

// Función para generar el primer número primo con 'digits' dígitos
unsigned long long generate_prime(int digits) {
    unsigned long long start = 1;
    for (int i = 1; i < digits; i++) start *= 10;
    unsigned long long end = start * 10 - 1;

    // Semilla solo una vez
    static int seeded = 0;
    if (!seeded) {
        srand(time(NULL) ^ pthread_self());
        seeded = 1;
    }

    for (int attempts = 0; attempts < 1000000; attempts++) {
        unsigned long long candidate = start + rand() % (end - start + 1);
        if (is_prime(candidate)) return candidate;
    }

    return 0; // No encontrado tras varios intentos
}


void *prime_thread(void *arg) {
    task_t *task = (task_t *)arg;
    task->result = generate_prime(task->digits);
    pthread_mutex_lock(&tasks_mutex);
    task->completed = 1;
    pthread_mutex_unlock(&tasks_mutex);
    return NULL;
}

void handle_client(int client_fd) {
    char buffer[256];
    int read_size;

    read_size = recv(client_fd, buffer, sizeof(buffer)-1, 0);
    if (read_size <= 0) {
        close(client_fd);
        return;
    }
    buffer[read_size] = '\0';

    char *cmd = strtok(buffer, " \n\r");
    if (!cmd) {
        send(client_fd, "Error: comando no reconocido\n", 29, 0);
        close(client_fd);
        return;
    }

    if (strcmp(cmd, "generate") == 0) {
        char *digits_str = strtok(NULL, " \n\r");
        if (!digits_str) {
            send(client_fd, "Error: falta número de dígitos\n", 31, 0);
            close(client_fd);
            return;
        }
        int digits = atoi(digits_str);
        if (digits <= 0 || digits > MAX_DIGITS) {
            send(client_fd, "Error: dígitos inválidos (1-20)\n", 32, 0);
            close(client_fd);
            return;
        }

        pthread_mutex_lock(&tasks_mutex);
        if (task_count >= MAX_TASKS) {
            pthread_mutex_unlock(&tasks_mutex);
            send(client_fd, "Error: número máximo de tareas alcanzado\n", 42, 0);
            close(client_fd);
            return;
        }
        // Crear ID simple: task<ID>
        char id[32];
        snprintf(id, sizeof(id), "task%d", task_count+1);

        task_t *task = &tasks[task_count++];
        strncpy(task->id, id, sizeof(task->id));
        task->digits = digits;
        task->completed = 0;
        task->result = 0;

        pthread_create(&task->thread, NULL, prime_thread, task);
        pthread_mutex_unlock(&tasks_mutex);

        char reply[64];
        snprintf(reply, sizeof(reply), "Task ID: %s\n", id);
        send(client_fd, reply, strlen(reply), 0);
    }
    else if (strcmp(cmd, "status") == 0) {
        char *id = strtok(NULL, " \n\r");
        if (!id) {
            send(client_fd, "Error: falta task_id\n", 21, 0);
            close(client_fd);
            return;
        }
        pthread_mutex_lock(&tasks_mutex);
        int found = 0;
        for (int i = 0; i < task_count; i++) {
            if (strcmp(tasks[i].id, id) == 0) {
                found = 1;
                if (tasks[i].completed) {
                    char reply[128];
                    snprintf(reply, sizeof(reply), "Completed: %llu\n", tasks[i].result);
                    send(client_fd, reply, strlen(reply), 0);
                } else {
                    send(client_fd, "Pending\n", 8, 0);
                }
                break;
            }
        }
        pthread_mutex_unlock(&tasks_mutex);
        if (!found) {
            send(client_fd, "Task not found\n", 15, 0);
        }
    }
    else {
        send(client_fd, "Error: comando no reconocido\n", 29, 0);
    }

    close(client_fd);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Prime service running on port %d\n", PORT);

    while (1) {
        addr_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        handle_client(client_fd);
    }

    return 0;
}
