#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

// Better use of string splitting function is to have manually allocated arrays for the user input and the arguments
#define MAX_INPUT_SIZE 1024 // Maximum number of characters in input
#define MAX_ARGS 100 // Maximum number of arguments in a command

// Function to display the shell prompt
void display_prompt() {
    printf("$ ");
    fflush(stdout);
}

// Function to execute a command with pipes and handle redirections
void execute_command_with_pipes(char *command) {
    char *cmds[MAX_ARGS]; // Array to store individual commands
    char *token; // Token for string splitting
    int num_cmds = 0; // Number of commands
    int background = 0; // Flag to indicate background process
    int input_redirection = 0; // Flag to indicate input redirection
    int output_redirection = 0; // Flag to indicate output redirection
    char *input_file = NULL; // Input file for redirection
    char *output_file = NULL; // Output file for redirection

    // Parse the command into individual commands based on pipe ('|') symbol as the delimiter
    token = strtok(command, "|");
    while (token != NULL && num_cmds < MAX_ARGS) {
        cmds[num_cmds++] = token; // Store the command in the array
        token = strtok(NULL, "|"); // Get the next command
    }

    // Check if the last command is a background process (ends with '&')
    char *last_cmd = cmds[num_cmds - 1];
    if (last_cmd[strlen(last_cmd) - 1] == '&') {
        cmds[num_cmds - 1][strlen(last_cmd) - 1] = '\0'; // Remove '&' from last command
        background = 1; // Set background flag
    }

    // Check for input and output redirection
    for (int i = 0; i < num_cmds; i++) {
        if (strstr(cmds[i], "<") != NULL) {
            input_redirection = 1; // Set input redirection flag
            char *input_token = strtok(cmds[i], "<"); // Split the command at '<'
            cmds[i] = input_token; // Update the command
            input_file = strtok(NULL, " \t\n"); // Get the input file
        }
        // Check for output redirection
        if (strstr(cmds[i], ">") != NULL) {
            output_redirection = 1; // Set output redirection flag
            char *output_token = strtok(cmds[i], ">"); // Split the command at '>'
            cmds[i] = output_token; // Update the command
            output_file = strtok(NULL, " \t\n"); // Get the output file
        }
    }

    // Create pipes
    int pipes[num_cmds - 1][2]; // Array of pipe file descriptors
    for (int i = 0; i < num_cmds - 1; i++) { // Create pipes for each command
        if (pipe(pipes[i]) == -1) {
            perror("pipe"); // Error handling for pipe creation
            exit(EXIT_FAILURE);
        }
    }

    // Execute each command in a separate process connected by pipes
    pid_t last_pid;
    for (int i = 0; i < num_cmds; i++) {
        // Parse each command into arguments
        char *args[MAX_ARGS]; // Array to store arguments
        int arg_count = 0; // Number of arguments in the command, intialized to 0
        token = strtok(cmds[i], " \t\n"); // Split the command at spaces, tabs, and newline
        while (token != NULL && arg_count < MAX_ARGS - 1) { // Loop through the tokens
            args[arg_count++] = token; // Store the token as an argument
            token = strtok(NULL, " \t\n"); // Get the next token
        }
        args[arg_count] = NULL; // Set the last argument to NULL
        // Check for empty command and throw an error if no arguments are present
        if (arg_count == 0) {
            fprintf(stderr, "Error: Empty command.\n");
            exit(EXIT_FAILURE);
        }

        // Fork a new process
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork"); // Error handling for fork failure
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process
            // Handle input redirection
            if (input_redirection && i == 0) {
                int fd = open(input_file, O_RDONLY); // Open the input file for reading only
                if (fd == -1) {
                    perror(input_file); // Error handling for file open failure
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO); // Redirect standard input to the file
                close(fd); // Close the file descriptor
            }

            // Handle output redirection
            if (output_redirection && i == num_cmds - 1) {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644); // Open the output file for writing only, 
                // create if not exists, truncate if exists
                if (fd == -1) {
                    perror(output_file); // Error handling for file open failure
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO); // Redirect standard output to the file
                close(fd); // Close the file descriptor
            }

            // Connect pipes
            if (i > 0) { // For commands other than the first one
                dup2(pipes[i - 1][0], STDIN_FILENO); // Redirect input from previous pipe
            }
            if (i < num_cmds - 1) { // For commands other than the last one
                dup2(pipes[i][1], STDOUT_FILENO); // Redirect output to next pipe
            }

            // Close all pipe descriptors
            for (int j = 0; j < num_cmds - 1; j++) { // Loop through all pipes
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Execute the command
            if (execvp(args[0], args) == -1) {
                fprintf(stderr, "%s: command not found.\n", args[0]); // Error handling for command execution failure
                exit(EXIT_FAILURE);
            }
        } else {
            // Parent process
            if (i == num_cmds - 1) {
                // Last command
                last_pid = pid;
            }
        }
    }

    // Close all pipe descriptors in parent process
    for (int i = 0; i < num_cmds - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Print the PID of the last sub-command in background
    if (background) {
        printf("[%d]\n", last_pid);
    } else {
        // Wait for all child processes to complete, unless background process
        int status;
        waitpid(last_pid, &status, 0);
    }
}

// Function to change directory
void change_directory(char *path) {
    if (chdir(path) != 0) {
        perror(path);
    }
}


int main() {
    char input[MAX_INPUT_SIZE];

    // Main shell loop
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
        } else if (strncmp(input, "cd ", 3) == 0) {
            // Change directory if input starts with 'cd '
            char *path = input + 3; // Skip 'cd ' prefix
            change_directory(path);
            continue; // Skip executing further code after 'cd'
        }

        // Execute the command with pipes and redirections
        execute_command_with_pipes(input);
    }

    return 0; // Exit the shell
}