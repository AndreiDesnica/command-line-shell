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

    int is_background = 0;  // Track background execution
    int input_redirect = -1, output_redirect = -1; // File descriptors for I/O redirection
    char *input_file = NULL, *output_file = NULL;
    char *args[tokens->size + 1];
    int arg_index = 0;
    int pipe_positions[10]; // Store positions of `|`
    int pipe_count = 0;

    // Scan tokens for special operators: |, <, >, &
    for (int i = 0; i < tokens->size; i++) {
        if (tokens->items[i] == NULL) continue;

        // Detect background execution
        if (strcmp(tokens->items[i], "&") == 0) {
            is_background = 1;
            tokens->items[i] = NULL; // Remove `&` from command
            break;
        }
            // Detect input redirection
        else if (strcmp(tokens->items[i], "<") == 0 && i + 1 < tokens->size) {
            if (tokens->items[i + 1] != NULL) {
                input_file = tokens->items[i + 1];
                tokens->items[i] = NULL;
                tokens->items[i + 1] = NULL;
            }
            i++; // Skip next token (file name)
        }
            // Detect output redirection
        else if (strcmp(tokens->items[i], ">") == 0 && i + 1 < tokens->size) {
            if (tokens->items[i + 1] != NULL) {
                output_file = tokens->items[i + 1];
                tokens->items[i] = NULL;
                tokens->items[i + 1] = NULL;
            }
            i++; // Skip next token (file name)
        }
            // Detect piping
        else if (strcmp(tokens->items[i], "|") == 0) {
            pipe_positions[pipe_count++] = i;
            tokens->items[i] = NULL; // Break commands at `|`
        }
        else {
            args[arg_index++] = tokens->items[i]; // Store normal arguments
        }
    }

    args[arg_index] = NULL; // Null-terminate argument list

    // If no command left, exit early
    if (arg_index == 0 && pipe_count == 0) {
        printf("Error: No command to execute\n");
        return;
    }

    // Handle Piping (`|`)
    if (pipe_count > 0) {
        int pipes[pipe_count][2];

        // Create pipes
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
            }
            else if (pid == 0) { // Child process
                if (i > 0) dup2(pipes[i - 1][0], STDIN_FILENO); // Read from previous pipe
                if (i < pipe_count) dup2(pipes[i][1], STDOUT_FILENO); // Write to next pipe

                for (int j = 0; j < pipe_count; j++) { // Close unused pipes
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                char *pipe_args[tokens->size + 1];
                int cmd_index = 0;
                for (int j = start; j < (i < pipe_count ? pipe_positions[i] : tokens->size); j++) {
                    pipe_args[cmd_index++] = tokens->items[j];
                }
                pipe_args[cmd_index] = NULL;

                execvp(pipe_args[0], pipe_args);
                perror("execvp failed");
                exit(1);
            }
            start = pipe_positions[i] + 1;
        }

        for (int i = 0; i < pipe_count; i++) {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }

        for (int i = 0; i <= pipe_count; i++) wait(NULL); // Wait for all processes
        return;
    }

    // Standard execution (no piping)
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        exit(1);
    }
    else if (pid == 0) { // Child process
        if (input_file) { // Handle input redirection
            input_redirect = open(input_file, O_RDONLY);
            if (input_redirect < 0) {
                perror("Input redirection failed");
                exit(1);
            }
            dup2(input_redirect, STDIN_FILENO);
            close(input_redirect);
        }

        if (output_file) { // Handle output redirection
            output_redirect = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (output_redirect < 0) {
                perror("Output redirection failed");
                exit(1);
            }
            dup2(output_redirect, STDOUT_FILENO);
            close(output_redirect);
        }

        execvp(args[0], args);
        perror("execvp failed");
        exit(1);
    }
    else { // Parent process
        if (!is_background) {
            waitpid(pid, NULL, 0); // Wait for foreground processes
        } else {
            printf("[Background PID %d]\n", pid); // Print background job PID
        }
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
