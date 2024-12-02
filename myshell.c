#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_INPUT 256
#define MAX_ARGS 64

// Signal handler for Ctrl-C and Ctrl-Z
void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\nCaught SIGINT (Ctrl-C). Use 'exit' to quit.\nmyshell> ");
        fflush(stdout);
    } else if (sig == SIGQUIT) {
        printf("\nCaught SIGQUIT (Ctrl-Z). Continuing...\nmyshell> ");
        fflush(stdout);
    }
}

// Function to execute commands with redirection and pipes
void handle_redirection_and_pipe(char *command) {
    int fd;
    char *args[MAX_ARGS];
    char *token;
    int i = 0;
    int pipe_fd[2];
    int pipe_present = 0;

    // Check for pipe
    if (strchr(command, '|')) {
        pipe_present = 1;
        pipe(pipe_fd);
    }

    token = strtok(command, " ");
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    if (pipe_present) {
        // Handle pipe logic
        pid_t pid1 = fork();
        if (pid1 == 0) {  // Child process
            dup2(pipe_fd[1], STDOUT_FILENO);
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            execvp(args[0], args);
            perror("execvp failed");
            exit(1);
        } else {
            wait(NULL);
            pid_t pid2 = fork();
            if (pid2 == 0) {
                dup2(pipe_fd[0], STDIN_FILENO);
                close(pipe_fd[0]);
                close(pipe_fd[1]);
                execvp(args[i - 1], args + i - 1);  // Execute second command
                perror("execvp failed");
                exit(1);
            } else {
                close(pipe_fd[0]);
                close(pipe_fd[1]);
                wait(NULL);
            }
        }
    } else {
        // Check for redirection
        for (i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], ">") == 0) {
                fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                dup2(fd, STDOUT_FILENO);
                close(fd);
                args[i] = NULL;  // Remove redirection from arguments
            } else if (strcmp(args[i], "<") == 0) {
                fd = open(args[i + 1], O_RDONLY);
                dup2(fd, STDIN_FILENO);
                close(fd);
                args[i] = NULL;  // Remove redirection from arguments
            }
        }
        execvp(args[0], args);
        perror("execvp failed");
    }
}

// Function to execute commands
void execute_command(char *command) {
    pid_t pid;
    int background = 0;

    // Check for background execution
    if (command[strlen(command) - 1] == '&') {
        background = 1;
        command[strlen(command) - 1] = '\0';
    }

    pid = fork();
    if (pid == 0) {  // Child process
        handle_redirection_and_pipe(command);  // Handle redirection and pipes
        exit(0);
    } else if (pid > 0) {  // Parent process
        if (!background) {
            waitpid(pid, NULL, 0);  // Wait for child if not background
        }
    } else {
        perror("fork failed");
    }
}

// Main shell loop
int main() {
    char command[MAX_INPUT];

    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGQUIT, handle_signal);

    while (1) {
        printf("myshell> ");
        if (fgets(command, MAX_INPUT, stdin) == NULL) {
            printf("\nExiting shell...\n");
            break;
        }
        command[strcspn(command, "\n")] = '\0';  // Remove newline character

        // Exit command
        if (strcmp(command, "exit") == 0) {
            printf("Exiting shell...\n");
            break;
        }

        // Handle built-in commands like "cd"
        if (strncmp(command, "cd", 2) == 0) {
            char *path = command + 3;
            if (chdir(path) != 0) {
                perror("cd failed");
            }
            continue;
        }

        // Execute other commands
        execute_command(command);
    }

    return 0;
}

