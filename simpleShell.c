#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <glob.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>

#include "token.h"
#include "command.h"

#define MAX_LINE 1024
#define PROMPT_MAX 100

char prompt[PROMPT_MAX];
volatile sig_atomic_t received_signal = 0;

// Forward Declaration
void changePrompt(char *new_prompt);
void printWorkingDirectory();
void executeCommand(Command *command, int numCommands);
void changeDirectory(char *path);
void redirectStdIo(Command *command);
void signal_handler(int sig);
void sigchld_handler(int sig);

int main()
{
    char userInput[MAX_LINE];
    char *tokens[MAX_ARGS];
    int tokenNum = 0;
    Command commands[MAX_NUM_COMMANDS];
    int numCommands = 0;

    // Signal
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    changePrompt(NULL); // Set the initial prompt

    while (1)
    {
        if (!received_signal) // No signal received
        {
            printf("%s", prompt);
        }
 
        int again = 1;
        char *linept;

        if (fgets(userInput, MAX_LINE, stdin) == NULL) // If EOF reached, to exit program
        {
            printf("\n");
            break;
        }

        if (userInput[strlen(userInput) - 1] == '\n') // Check for newline character
        {
            userInput[strlen(userInput) - 1] = '\0';
        }

        received_signal = 0; // Reset signal flag
        linept = userInput;

        while (again)
        {
            again = 0;
            if (linept == NULL)
            {
                if (errno == EINTR) 
                {
                    again = 1;
                }
            }
        }

        tokenNum = tokenise(userInput, tokens); // Tokenise input
        
        if (tokenNum == 0) // Error handling for empty token
        {
            continue;
        }

        numCommands = separateCommands(tokens, commands); // Form command line

        if (numCommands <= 0) // Error handling for empty command
        {
            continue;
        }

        if (strcmp(commands[0].argv[0], "exit") == 0)
        {
            exit(EXIT_SUCCESS);
        }
        else
        {
            executeCommand(commands, numCommands);
        }
    }
    return 0;
}

/*******************Function Implementation*******************/

void changePrompt(char *new_prompt)
{

    if (new_prompt && strlen(new_prompt) > 0)
    {
        snprintf(prompt, PROMPT_MAX, "%s ", new_prompt);
    }
    else
    {
        snprintf(prompt, PROMPT_MAX, "%% ");
    }
}

void printWorkingDirectory()
{
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("%s\n", cwd);
    }
    else
    {
        perror("getcwd() error");
    }
}

void executeCommand(Command *commands, int numCommands)
{
    int status;
    pid_t pid, pipe_pid;
    int i, p, pipeSepCount, currentCommand;

    for (i = 0; i < numCommands; i++)
    {
        char **original_argv = commands[i].argv;
        glob_t glob_result;
        int glob_index = -1;
        bool has_wildcards = false;

        if (strcmp(commands[i].sep, "&") == 0) // Signal for background process
        {
            signal(SIGCHLD, sigchld_handler);
        }

        pid = fork();
        if (pid < 0) // Fork failed
        {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) // Child
        {
            for (int b = 0; original_argv[b] != NULL; b++) // Wildcard check
            {
                if (strpbrk(original_argv[b], "*?") != NULL)
                {
                    glob_index = b;
                    has_wildcards = true;
                    break;
                }
            }

            if (has_wildcards)
            {
                glob(commands[i].argv[glob_index], GLOB_TILDE, NULL, &glob_result);

                if (glob_result.gl_pathc > 0)
                {
                    size_t new_size = (glob_result.gl_pathc + 1) * sizeof(char*);
                    commands[i].argv = realloc(commands->argv, new_size); // Reallocation of memory
                    
                    // Replace wildcard arguments with expanded filenames
                    for (size_t c = 0; c < glob_result.gl_pathc; c++)
                    {
                        commands[i].argv[glob_index + c] = glob_result.gl_pathv[c];
                    }
                    // Set last element to NULL
                    commands[i].argv[glob_index + glob_result.gl_pathc] = NULL;
                }
            }

            if (strcmp(commands[i].argv[0], "pwd") == 0)
            {
                exit(0);
            }
            else if (strcmp(commands[i].argv[0], "prompt") == 0)
            {
                exit(0);
            }
            else if (strcmp(commands[i].argv[0], "cd") == 0)
            {
                exit(0);
            }
            else if (strcmp(commands[i].sep, "|") == 0)
            {
                pipeSepCount = 0;
                currentCommand = i; // Separate command counter
                while (strcmp(commands[currentCommand].sep, "|") == 0)
                {
                    if (strcmp(commands[currentCommand].sep, ";") == 0)
                    {
                        break;
                    }
                    currentCommand++;
                    pipeSepCount++;
                }
                pipeSepCount++; // For last pipe execution
                int pipefd[pipeSepCount - 1][2];
                for (p = 0; p < pipeSepCount - 1; p++)
                {
                    if (pipe(pipefd[p]) < 0)
                    {
                        perror("Pipe failed");
                        exit(EXIT_FAILURE);
                    }
                }
                for (p = 0; p < pipeSepCount; p++)
                {
                    pipe_pid = fork();
                    if (pipe_pid < 0)
                    {
                        perror("Pipe fork failed");
                        exit(EXIT_FAILURE);
                    }
                    else if (pipe_pid == 0)
                    {
                        if (p == 0) // First command
                        {
                            close(pipefd[p][0]); // Close read end of pipe
                            dup2(pipefd[p][1], STDOUT_FILENO);
                            close(pipefd[p][1]); // Close write end of pipe

                            // Prevent file descriptors leaking
                            for (int j = 1; j < pipeSepCount - 1; j++)
                            {
                                close(pipefd[j][0]);
                                close(pipefd[j][1]);
                            }
                        }
                        // Last command
                        else if (p == pipeSepCount - 1)
                        {
                            close(pipefd[p - 1][1]); // Close write end of previous pipe
                            dup2(pipefd[p - 1][0], STDIN_FILENO);
                            close(pipefd[p - 1][0]); // Close read end of previous pipe

                            // Prevent file descriptors leaking
                            for (int j = 0; j < pipeSepCount - 2; j++)
                            {
                                close(pipefd[j][0]);
                                close(pipefd[j][1]);
                            }
                            close(pipefd[pipeSepCount - 2][0]);
                        }
                        // Intermediate commands
                        else
                        {
                            close(pipefd[p - 1][1]); // Close write end of previous pipe
                            dup2(pipefd[p - 1][0], STDIN_FILENO);
                            close(pipefd[p - 1][0]); // Close read end of previous pipe
                            
                            close(pipefd[p][0]); // Close read end of current pipe
                            dup2(pipefd[p][1], STDOUT_FILENO);
                            close(pipefd[p][1]); // Close write end of current pipe

                            // Prevent file descriptors leaking
                            for (int j = 0; j < pipeSepCount - 2; j++)
                            {
                                if (j != p - 1) 
                                {
                                    close(pipefd[j][0]);
                                    close(pipefd[j][1]);
                                }
                            }
                        }
                        redirectStdIo(&commands[i]);

                        if (execvp(commands[i].argv[0], commands[i].argv) < 0)
                        {
                            perror("execvp failed");
                            exit(EXIT_FAILURE);
                        }
                    }
                    i++; // Next command
                }
                // Ensure all read and write end of pipes are close
                for (p = 0; p < pipeSepCount - 1; p++)
                {
                    close(pipefd[p][0]);
                    close(pipefd[p][1]);
                }

                // Wait for all child process(es) to exit
                for (p = 0; p < pipeSepCount; p++)
                {
                    waitpid(-1, &status, 0);
                }
                exit(0);
            }
            // Normal execution ';'
            redirectStdIo(&commands[i]);

            if (execvp(commands[i].argv[0], commands[i].argv) < 0)
            {
                perror("execvp failed");
                exit(EXIT_FAILURE);
            }
        }
        else // Parent
        {
            if (strcmp(commands[i].argv[0], "pwd") == 0)
            {
                printWorkingDirectory();
            }
            else if (strcmp(commands[i].argv[0], "prompt") == 0)
            {
                changePrompt(commands[i].argv[1]);
            }
            else if (strcmp(commands[i].argv[0], "cd") == 0)
            {
                changeDirectory(commands[i].argv[1]);
            }
            else if (strcmp(commands[i].sep, "&") == 0)
            {
                printf("%d running\n", pid);
            }
            else if (strcmp(commands[i].sep, ";") == 0)
            {
                waitpid(pid, &status, 0);
            }
            else
            {
                // Skips x command if command separator is |
                while (strcmp(commands[i].sep, "|") == 0)
                {
                    i++;
                }
                waitpid(pid, &status, 0);
            }
            commands[i].stdin_file = NULL; // Set standard input to NULL
            commands[i].stdout_file = NULL; // Set standard output to NULL
            
            // Frees memory allocated and restore original array
            if (has_wildcards && glob_result.gl_pathc > 0)
            {
                globfree(&glob_result);
                commands[i].argv = original_argv;
            }
        }
    }
}

void changeDirectory(char *path)
{
    if (path == NULL) // No path provided
    {
        path = getenv("HOME"); // Get home path as a pointer to a string
    }
    else if (strcmp(path, "~") == 0) // If the path is "~"
    {
        path = getenv("HOME"); // Get home path as a pointer to a string
    }
    else if (strcmp(path, "-") == 0) // If the path is "-"
    {
        path = getenv("OLDPWD"); // Get previous working directory path as a pointer to a string
        if (path == NULL)
        {
            printf("simpleShell: cd: OLDPWD not set\n");
            return;
        }
    }

    char oldpwd[1024]; // Store current working directory in OLDPWD
    if (getcwd(oldpwd, sizeof(oldpwd)) != NULL)
    {
        setenv("OLDPWD", oldpwd, 1);
    }
    else
    {
        perror("getcwd() error");
    }

    if (chdir(path) < 0) // Change directory to the specified path
    {
        perror("chdir");
    }
}

void redirectStdIo(Command *command)
{
    if (command->stdin_file != NULL) // Redirect standard input
    {
        int fd_in = open(command->stdin_file, O_RDONLY);
        if (fd_in < 0)
        {
            perror("Error opening file to read from");
            exit(EXIT_FAILURE);
        }
        if (dup2(fd_in, STDIN_FILENO) < 0)
        {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(fd_in);
    }

    else if (command->stdout_file != NULL) // Redirect standard output
    {
        int fd_out = open(command->stdout_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_out < 0)
        {
            perror("error opening file to write to");
            exit(EXIT_FAILURE);
        }
        if (dup2(fd_out, STDOUT_FILENO) < 0)
        {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(fd_out);
    }
}

void signal_handler(int sig)
{
    received_signal = 1;
    printf("\n%s", prompt);
    fflush(stdout);
}

void sigchld_handler(int sig)
{
    int status;
    pid_t chld_pid;
    while ((chld_pid = waitpid(-1, &status, WNOHANG)) > 0 )
    {
        printf("%d exited\n", chld_pid);
    }
}