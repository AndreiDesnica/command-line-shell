#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "lexer.h"

// Function to display the shell prompt in the format: USER@MACHINE:PWD>
void display_prompt() {
    char *user = getenv("USER"); // Get current user
    char host[256];
    gethostname(host, sizeof(host)); // Get machine hostname
    char *pwd = getenv("PWD"); // Get current working directory

    printf("%s@%s:%s> ", user, host, pwd); // Print formatted prompt
    fflush(stdout); // Ensure the prompt is displayed immediately
}

// Function to handle internal commands like 'exit' and 'cd'
int handle_internal_commands(tokenlist *tokens) {
    if (tokens->size == 0) return 0; // Ignore empty input

    // Handle 'exit' command
    if (strcmp(tokens->items[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }

    // Handle 'cd' command
    if (strcmp(tokens->items[0], "cd") == 0) {
        if (tokens->size < 2) {
            fprintf(stderr, "cd: missing argument\n");
        } else {
            if (chdir(tokens->items[1]) != 0) {
                perror("cd failed"); // Print error if directory change fails
            }
        }
        return 1; // Indicate that we handled an internal command
    }

    return 0; // Not an internal command
}

// Function to execute a command using fork() and execvp()
void execute_command(tokenlist *tokens) {
    if (handle_internal_commands(tokens)) return; // Check for internal commands

    if (tokens->size == 0) return; // Ignore empty input

    pid_t pid = fork(); // Create a child process

    if (pid == -1) {
        perror("fork failed"); // Error handling
        exit(1);
    } 
    else if (pid == 0) { // Child process
        char *args[tokens->size + 1]; // Convert token list to argument array
        for (int i = 0; i < tokens->size; i++) {
            args[i] = tokens->items[i]; // Copy tokens
        }
        args[tokens->size] = NULL; // Null-terminate the argument list

        // Execute the command using execvp() (execvp automatically searches in $PATH)
        if (execvp(args[0], args) == -1) {
            perror("execvp failed"); // Command execution failed
            exit(1);
        }
    } 
    else { // Parent process
        wait(NULL); // Wait for child process to finish
    }
}

int main() {
    while (1) {
        display_prompt(); // Show the shell prompt
        char *input = get_input(); // Get user input
        tokenlist *tokens = get_tokens(input); // Tokenize the input

        execute_command(tokens); // Execute the command

        free(input); // Free dynamically allocated memory
        free_tokens(tokens);
    }
    return 0; // Should never reach this point
}
