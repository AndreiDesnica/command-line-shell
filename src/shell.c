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

    int input_redirect = -1;
    int output_redirect = -1;
    char *input_file = NULL;
    char *output_file = NULL;
    char *args[tokens->size + 1];
    int arg_index = 0;

    // Scan for redirection operators and remove them from tokens
    for (int i = 0; i < tokens->size; i++) {
        if (tokens->items[i] == NULL) continue; // Skip NULL tokens

        // Ensure token[i] is not NULL before comparing
        if (tokens->items[i] != NULL && strcmp(tokens->items[i], "<") == 0 && i + 1 < tokens->size) {
            if (tokens->items[i + 1] != NULL) {
                input_file = tokens->items[i + 1]; // Store input file name
                tokens->items[i] = NULL;  // Remove `<`
                tokens->items[i + 1] = NULL;  // Remove file name
            }
            i++; // Skip the next token (file name)
        }
        else if (tokens->items[i] != NULL && strcmp(tokens->items[i], ">") == 0 && i + 1 < tokens->size) {
            if (tokens->items[i + 1] != NULL) {
                output_file = tokens->items[i + 1]; // Store output file name
                tokens->items[i] = NULL;  // Remove `>`
                tokens->items[i + 1] = NULL;  // Remove file name
            }
            i++; // Skip the next token (file name)
        }
        else {
            args[arg_index++] = tokens->items[i]; // Keep valid commands
        }
    }

    args[arg_index] = NULL; // Null-terminate the argument list

    if (arg_index == 0) {
        printf("Error: No command to execute\n");
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        exit(1);
    }
    else if (pid == 0) { // Child process
        if (input_file) {
            input_redirect = open(input_file, O_RDONLY);
            if (input_redirect < 0) {
                perror("Input redirection failed");
                exit(1);
            }
            dup2(input_redirect, STDIN_FILENO);  // Redirect stdin to file
            close(input_redirect);
        }

        if (output_file) {
            output_redirect = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (output_redirect < 0) {
                perror("Output redirection failed");
                exit(1);
            }
            dup2(output_redirect, STDOUT_FILENO);  // Redirect stdout to file
            close(output_redirect);
        }

        // Execute the command
        if (execvp(args[0], args) == -1) {
            perror("execvp failed");
            exit(1);
        }
    }
    else { // Parent process
        wait(NULL); // Wait for child process to finish
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
