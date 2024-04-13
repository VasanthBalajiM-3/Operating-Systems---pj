#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 100

// Function to display the shell prompt
void display_prompt() {
    printf("$ ");
    fflush(stdout);
}

// Function to execute a command with pipes and handle redirections
void execute_command_with_pipes(char *command) {
    char *cmds[MAX_ARGS];
    char *token;
    int num_cmds = 0;
    int background = 0;
    int input_redirection = 0;
    int output_redirection = 0;
    char *input_file = NULL;
    char *output_file = NULL;

    // Parse the command into individual commands based on pipe ('|') symbol
    token = strtok(command, "|");
    while (token != NULL && num_cmds < MAX_ARGS) {
        cmds[num_cmds++] = token;
        token = strtok(NULL, "|");
    }

    // Check if the last command is a background process (ends with '&')
    char *last_cmd = cmds[num_cmds - 1];
    if (last_cmd[strlen(last_cmd) - 1] == '&') {
        cmds[num_cmds - 1][strlen(last_cmd) - 1] = '\0'; // Remove '&' from last command
        background = 1;
    }

    // Check for input and output redirection
    for (int i = 0; i < num_cmds; i++) {
        if (strstr(cmds[i], "<") != NULL) {
            input_redirection = 1;
            char *input_token = strtok(cmds[i], "<");
            cmds[i] = input_token;
            input_file = strtok(NULL, " \t\n");
        }
        if (strstr(cmds[i], ">") != NULL) {
            output_redirection = 1;
            char *output_token = strtok(cmds[i], ">");
            cmds[i] = output_token;
            output_file = strtok(NULL, " \t\n");
        }
    }

    // Create pipes
    int pipes[num_cmds - 1][2];
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Execute each command in a separate process connected by pipes
    pid_t last_pid;
    for (int i = 0; i < num_cmds; i++) {
        // Parse each command into arguments
        char *args[MAX_ARGS];
        int arg_count = 0;
        token = strtok(cmds[i], " \t\n");
        while (token != NULL && arg_count < MAX_ARGS - 1) {
            args[arg_count++] = token;
            token = strtok(NULL, " \t\n");
        }
        args[arg_count] = NULL;

        // Fork a new process
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process
            // Handle input redirection
            if (input_redirection && i == 0) {
                int fd = open(input_file, O_RDONLY);
                if (fd == -1) {
                    perror(input_file);
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            // Handle output redirection
            if (output_redirection && i == num_cmds - 1) {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror(output_file);
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            // Connect pipes
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO); // Redirect input from previous pipe
            }
            if (i < num_cmds - 1) {
                dup2(pipes[i][1], STDOUT_FILENO); // Redirect output to next pipe
            }

            // Close all pipe descriptors
            for (int j = 0; j < num_cmds - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Execute the command
            if (execvp(args[0], args) == -1) {
                perror(args[0]);
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
        perror("cd");
    }
}


int main() {
    char input[MAX_INPUT_SIZE];

    // Use readline for advanced input handling with tab completion
    rl_bind_key('\t', rl_complete); // Bind tab key to readline completion function

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

    return 0;
}
