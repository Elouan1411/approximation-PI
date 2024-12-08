#define _DEFAULT_SOURCE
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <wait.h>
#include <sys/types.h>

volatile int stop = 0;

double get_double_0_1() {
    return (double)random() / INT_MAX;
}

int compute_pi() {
    double x = get_double_0_1();
    double y = get_double_0_1();
    return (x * x + y * y < 1) ? 1 : 0;
}

void myClose(int fd, const char* m) {
    if (close(fd) == -1) {
        perror(m);
        exit(1);
    }
}

void handler(int sig) {
    (void)sig;
    stop = 1;
}

void install_signal_handler() {
    struct sigaction a1;
    a1.sa_flags = 0;
    sigemptyset(&a1.sa_mask);
    a1.sa_handler = handler;

    int res = sigaction(SIGINT, &a1, NULL);
    if (res == -1) {
        perror("sigaction");
        exit(1);
    }
}

void create_pipes_and_forks(int NPROC, int*** tube_p, int*** tube_n, pid_t** pid) {
    *tube_p = (int**)malloc(NPROC * sizeof(int*));
    *tube_n = (int**)malloc(NPROC * sizeof(int*));
    *pid = (pid_t*)malloc(NPROC * sizeof(pid_t));

    for (int i = 0; i < NPROC; i++) {
        (*tube_p)[i] = (int*)malloc(2 * sizeof(int));
        (*tube_n)[i] = (int*)malloc(2 * sizeof(int));
        if (pipe((*tube_p)[i]) == -1) {
            perror("pipe_p");
            exit(1);
        }
        if (pipe((*tube_n)[i]) == -1) {
            perror("pipe_n");
            exit(1);
        }
        (*pid)[i] = fork();
        if ((*pid)[i] == -1) {
            perror("fork");
            exit(1);
        }
    }
}


void process_child(int i, int** tube_p, int** tube_n) {
    myClose(tube_n[i][0], "close_n read son");
    myClose(tube_p[i][0], "close_p read son");
    srandom(getpid());
    unsigned long p = 0;
    unsigned long n = 0;
    while (!stop) {
        p += compute_pi();
        n++;
    }
    if (write(tube_p[i][1], &p, sizeof(unsigned long)) == -1) {
        perror("write");
        exit(1);
    }
    if (write(tube_n[i][1], &n, sizeof(unsigned long)) == -1) {
        perror("write_n");
        exit(1);
    }
    myClose(tube_p[i][1], "close_p write son");
    myClose(tube_n[i][1], "close_n write son");
    exit(0);
}

void read_and_close_pipes(int i, int** tube_p, int** tube_n, unsigned long* p_total, unsigned long* n_total) {
    unsigned long p_temp = 0;
    unsigned long n_temp = 0;

    if (read(tube_p[i][0], &p_temp, sizeof(unsigned long)) == -1) {
        perror("read");
        exit(1);
    }
    if (read(tube_n[i][0], &n_temp, sizeof(unsigned long)) == -1) {
        perror("read_n");
        exit(1);
    }

    myClose(tube_p[i][0], "close_p read father");
    myClose(tube_n[i][0], "close_n read father");
    *p_total += p_temp;
    *n_total += n_temp;
}

void wait_for_children(int NPROC, pid_t* pid) {
    for (int i = 0; i < NPROC; i++) {
        int status;
        if (waitpid(pid[i], &status, 0) == -1) {
            perror("waitpid");
            exit(1);
        }

        if (WIFEXITED(status)) {
            printf("%d : %d\n", pid[i], WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("%d : %d\n", pid[i], WTERMSIG(status));
        }
    }
}

double calculate_pi(unsigned long p_total, unsigned long n_total) {
    return 4 * ((double)(p_total) / (n_total));
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage %s NPROC", argv[0]);
        exit(1);
    }
    int NPROC = atoi(argv[1]);
    if (NPROC == 0) {
        fprintf(stderr, "0 process, calculation impossible");
        exit(1);
    }

    int** tube_p = NULL;
    int** tube_n = NULL;
    pid_t* pid = NULL;

    install_signal_handler();
    create_pipes_and_forks(NPROC, &tube_p, &tube_n, &pid);

    for (int i = 0; i < NPROC; i++) {
        if (pid[i] == 0) {
            process_child(i, tube_p, tube_n);
        }
    }

    unsigned long n_total = 0;
    unsigned long p_total = 0;
    for (int i = 0; i < NPROC; i++) {
        read_and_close_pipes(i, tube_p, tube_n, &p_total, &n_total);
        free(tube_p[i]);
        free(tube_n[i]);
    }

    free(tube_n);
    free(tube_p);

    wait_for_children(NPROC, pid);

    free(pid);

    double P = calculate_pi(p_total, n_total);
    printf("Pi approximation: %.15f\n", P);

    return 0;
}
