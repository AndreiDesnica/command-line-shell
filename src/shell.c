#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "lexer.h"

// Function to display the shell prompt in the format: USER@MACHINE:PWD>
void display_prompt() {
    char *user = getenv("USER");
    char host[256];
    gethostname(host, sizeof(host));
    char *pwd = getenv("PWD");

    printf("%s@%s:%s> ", user, host, pwd);
    fflush(stdout);
}

// Function to handle internal commands like 'exit' and 'cd'
int handle_internal_commands(tokenlist *tokens) {
    if (tokens->size == 0) return 0;

    if (strcmp(tokens->items[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }

    if (strcmp(tokens->items[0], "cd") == 0) {
        if (tokens->size < 2) {
            fprintf(stderr, "cd: missing argument\n");
        } else {
            if (chdir(tokens->items[1]) != 0) {
                perror("cd failed");
            }
        }
        return 1;
    }
    return 0;
}

// Function to execute a command, handling input/output redirection
void execute_command(tokenlist *tokens) {
    if (handle_internal_commands(tokens)) return;
    if (tokens->size == 0) return;

    int pipe_positions[10]; // Store positions of `|`
    int pipe_count = 0;
    char *args[tokens->size + 1];
    int arg_index = 0;

    // Identify pipe positions
    for (int i = 0; i < tokens->size; i++) {
        if (tokens->items[i] != NULL && strcmp(tokens->items[i], "|") == 0) {
            pipe_positions[pipe_count++] = i;
            tokens->items[i] = NULL; // Break commands at `|`
        }
    }

    // If no pipes exist, execute normally
    if (pipe_count == 0) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            exit(1);
        } else if (pid == 0) {
            // Convert tokens to argument list
            for (int i = 0; i < tokens->size; i++) {
                if (tokens->items[i] != NULL) {
                    args[arg_index++] = tokens->items[i];
                }
            }
            args[arg_index] = NULL;

            if (execvp(args[0], args) == -1) {
                perror("execvp failed");
                exit(1);
            }
        } else {
            wait(NULL);
            return;
        }
    }

    // Piping logic
    int pipes[pipe_count][2];

    for (int i = 0; i < pipe_count; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe failed");
            exit(1);
        }
    }

    int start = 0;
    for (int i = 0; i <= pipe_count; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork failed");
            exit(1);
        } else if (pid == 0) { // Child process
            // If not first command, set input from previous pipe
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            // If not last command, set output to next pipe
            if (i < pipe_count) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // Close all pipe ends in child
            for (int j = 0; j < pipe_count; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Extract arguments for this command
            int cmd_index = 0;
            for (int j = start; j < (i < pipe_count ? pipe_positions[i] : tokens->size); j++) {
                args[cmd_index++] = tokens->items[j];
            }
            args[cmd_index] = NULL;

            if (execvp(args[0], args) == -1) {
                perror("execvp failed");
                exit(1);
            }
        }

        start = pipe_positions[i] + 1;
    }

    // Close all pipe ends in parent
    for (int i = 0; i < pipe_count; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all child processes
    for (int i = 0; i <= pipe_count; i++) {
        wait(NULL);
    }
}

int main() {
    while (1) {
        display_prompt();
        char *input = get_input();
        tokenlist *tokens = get_tokens(input);

        execute_command(tokens);

        free(input);
        free_tokens(tokens);
    }
    return 0;
}
