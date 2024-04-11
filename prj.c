#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_INPUT_SIZE 1024

// Function to display the shell prompt
void display_prompt() {
    printf("$ ");
    fflush(stdout);
}

// Function to execute a command
void execute_command(char *command) {
    char *args[MAX_INPUT_SIZE];
    int background = 0;

    // Parse the command into arguments
    char *token = strtok(command, " \t\n");
    int i = 0;
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;

    // Check if the last argument is '&'
    if (i > 0 && strcmp(args[i - 1], "&") == 0) {
        args[i - 1] = NULL; // Remove '&' from arguments
        background = 1;     // Mark as background process
    }

    // Fork a new process
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
    } else if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process
        if (!background) {
            // Wait for the child process to complete in foreground
            int status;
            waitpid(pid, &status, 0);
        } else {
            // Print the PID of the background process
            printf("[%d]\n", pid);
        }
    }
}

int main() {
    char input[MAX_INPUT_SIZE];

    while (1) {
        // Display shell prompt
        display_prompt();

        // Read input from user
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; // Exit if EOF is encountered (Ctrl+D)
        }

        // Remove newline character if present
        input[strcspn(input, "\n")] = '\0';

        // Check for built-in commands
        if (strcmp(input, "exit") == 0) {
            break; // Exit the shell
        }

        // Execute the command
        execute_command(input);
    }

    return 0;
}