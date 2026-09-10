#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LINE     80      
#define MAX_ARGS     (MAX_LINE/2 + 1)
#define MAX_HISTORY  10     

static char history[MAX_HISTORY][MAX_LINE];
static int  history_count = 0;  
static int  history_total = 0;   

static void add_history(const char *cmd)
{
    int idx = history_total % MAX_HISTORY;
    strncpy(history[idx], cmd, MAX_LINE - 1);
    history[idx][MAX_LINE - 1] = '\0';
    history_total++;
    if (history_count < MAX_HISTORY)
        history_count++;
}

static void safe_print(const char *s)
{
    write(STDOUT_FILENO, s, strlen(s));
}

static void print_history(void)
{
    char line[MAX_LINE + 16];
    int first_number = history_total - history_count + 1;
    int i;

    safe_print("\n");
    for (i = 0; i < history_count; i++) {
        int slot = (history_total - history_count + i) % MAX_HISTORY;
        snprintf(line, sizeof(line), "%d\t%s\n", first_number + i, history[slot]);
        safe_print(line);
    }
    safe_print("\n");
}

static const char *find_history(char letter)
{
    int i;
    if (history_count == 0)
        return NULL;

    if (letter == '\0') {
        int last_slot = (history_total - 1) % MAX_HISTORY;
        return history[last_slot];
    }

    for (i = 0; i < history_count; i++) {
        int slot = (history_total - 1 - i) % MAX_HISTORY;
        if (history[slot][0] == letter)
            return history[slot];
    }
    return NULL;
}

static void handle_SIGINT(int signo)
{
    (void)signo;
    print_history();
    safe_print("COMMAND->");
    fflush(NULL);
}

static int setup(char inputBuffer[], char *args[], int *background)
{
    ssize_t n;
    int i, len;

    *background = 0;

    n = read(STDIN_FILENO, inputBuffer, MAX_LINE - 1);

    if (n < 0) {
        if (errno == EINTR) {
            return 0;
        }
        perror("read");
        exit(1);
    }
    if (n == 0) {
        printf("\n");
        exit(0);
    }

    inputBuffer[n] = '\0';
    if (n > 0 && inputBuffer[n - 1] == '\n')
        inputBuffer[n - 1] = '\0';

    if (strlen(inputBuffer) == 0)
        return 0;

    if (inputBuffer[0] == 'r' &&
        (inputBuffer[1] == '\0' || inputBuffer[1] == ' ')) {

        char letter = '\0';
        if (inputBuffer[1] == ' ' && inputBuffer[2] != '\0')
            letter = inputBuffer[2];

        const char *found = find_history(letter);
        if (found == NULL) {
            fprintf(stderr, "Алдаа: тохирох команд түүхэнд олдсонгүй.\n");
            return -1;
        }

        printf("%s\n", found);

        strncpy(inputBuffer, found, MAX_LINE - 1);
        inputBuffer[MAX_LINE - 1] = '\0';
    }

    i = 0;
    char *token = strtok(inputBuffer, " \t");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t");
    }
    args[i] = NULL;

    if (i == 0)
        return 0;

    len = (int)strlen(args[i - 1]);
    if (len > 0 && args[i - 1][len - 1] == '&') {
        if (len == 1) {
            args[i - 1] = NULL;
            i--;
        } else {
            args[i - 1][len - 1] = '\0';
        }
        *background = 1;
    }
    args[i] = NULL;

    return (i == 0) ? 0 : 1;
}

static void rebuild_command_line(char *args[], char *out, size_t out_size)
{
    out[0] = '\0';
    for (int i = 0; args[i] != NULL; i++) {
        strncat(out, args[i], out_size - strlen(out) - 1);
        if (args[i + 1] != NULL)
            strncat(out, " ", out_size - strlen(out) - 1);
    }
}

int main(void)
{
    char inputBuffer[MAX_LINE];
    int  background;
    char *args[MAX_ARGS];

    struct sigaction sa;
    sa.sa_handler = handle_SIGINT;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    while (1) {
        int status;
        char command_line[MAX_LINE];

        printf("COMMAND->");
        fflush(stdout);

        status = setup(inputBuffer, args, &background);

        if (status <= 0) {
            continue;
        }

        rebuild_command_line(args, command_line, sizeof(command_line));
        add_history(command_line);

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            continue;
        }

        if (pid == 0) {
            execvp(args[0], args);
            fprintf(stderr, "shell: команд олдсонгүй: %s\n", args[0]);
            exit(1);
        } else {
            if (background == 0) {
                waitpid(pid, &status, 0);
            } else {
                printf("[background pid %d]\n", pid);
            }
        }
    }

    return 0;
}
