#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <fcntl.h>

#define MAXCOM 1024
#define MAXLIST 100
#define MAXHISTORYLEN 100
#define TOK_DELIM " \t\r\n\a\""
#define LINE_END ";"

/*
CUSTOM SHELL
Created by- Harsh Lahoti
Compile: gcc -Werror -Wextra -pedantic hls.c -lreadline -o hls.o
Execute: ./hls.o
*/

// Custom colours for the text in the kernel
void p_reset()
{
    printf("\033[0m");
}

void p_blue(int bold)
{
    if (bold != 0)
        printf("\033[1;34m");
    else
        printf("\033[0;34m");
}

void p_yellow(int bold)
{
    if (bold != 0)
        printf("\033[1;33m");
    else
        printf("\033[0;33m");
}

void p_green(int bold)
{
    if (bold != 0)
        printf("\033[1;32m");
    else
        printf("\033[0;32m");
}

char *history[MAXHISTORYLEN];
int history_ind = 0;
char cwd[1024];
struct timeval t1, t2;
double time_taken;

// List based implementation of history, used alongside the one inbuilt with readline
int own_hist(char **args)
{
    for (int i = history_ind - 2; i >= 0; i--)
    {
        fprintf(stdout, "%s\n", history[i]);
    }
    return -1;
}

int own_help(char **args)
{
    char *builtin_func_list[] = {
        "help",
        "exit",
        "hist",
        "cd"};
    long unsigned int i = 0;
    (void)(**args);

    fprintf(stdout, "\n---help simple_shell---\n");
    fprintf(stdout, "Type a command and its arguments, then hit enter\n");
    fprintf(stdout, "Built-in commands:\n");
    for (; i < sizeof(builtin_func_list) / sizeof(char *); i++)
    {
        fprintf(stdout, "  -> %s\n", builtin_func_list[i]);
    }
    fprintf(stdout, "Use the man command for information on other programs.\n\n");
    return (-1);
}

int own_exit(char **args)
{
    /* exit with status */
    if (args[1])
    {
        return (atoi(args[1]));
    }
    /* exit success */
    else
    {
        return (0);
    }
}

// Implemented inside shell because of its nature as an environment setting
int own_cd(char **args)
{
    if (args[1] == NULL)
    {
        fprintf(stderr, "expected argument to \"cd\"\n");
    }
    else
    {
        if (chdir(args[1]) != 0)
        {
            perror("error in own_cd.c: changing dir\n");
        }
    }
    return (-1);
}

// Signal handling for SIGINT (2, terminate)
void ctr_C_handle(int sig)
{
    // getcwd(cwd, sizeof(cwd));
    // printf("(%s) @ [~%s] -\\\n$ ", getlogin(), cwd);
    printf("\n");
    fflush(stdout);
    signal(SIGINT, ctr_C_handle);
}

// Takes input and stores it in history
int takeInput(char *str)
{
    char *buf;

    buf = readline("");

    if (strlen(buf) != 0)
    {
        int i = 0;
        char *temp;
        while (1)
        {
            temp = buf + i;
            if (temp[0] != ' ')
                break;
            i++;
        }
        buf = temp;
        add_history(buf);

        strcpy(str, buf);
        history[history_ind] = buf;
        history_ind++;
        FILE *his = fopen("HISTORY.txt", "a");
        fputs(buf, his);
        fputs("\n", his);
        fclose(his);

        return -1;
    }
    else
    {
        return 1;
    }
}

// Executes single commands (in piped commands, executes a single part) using execvp
int execute(char **parsed_line)
{
    // printf("DEBUG 5: %s\n", parsed_line[0]);
    pid_t pid = fork();
    if (pid < 0)
        return 1;
    if (pid)
    {
        // parent
        wait(NULL);
        return -1;
    }
    else
    {
        execvp(parsed_line[0], parsed_line);
        exit(0);
    }
}

// Deciding whther the command is inbuilt or has to be executed using execvp (via call to execute)
int cmd_handle(char **parsed_line)
{
    char *builtin_func_list[] = {
        "cd",
        "help",
        "exit",
        "hist"};
    int (*builtin_func[])(char **) = {
        &own_cd,
        &own_help,
        &own_exit,
        &own_hist};
    long unsigned int i = 0;
    // printf("DEBUG 4: %s\n", parsed_line[0]);
    /*

    short fi = 0, fo = 0;

    if (in_no != STDIN_FILENO)
    {
        dup2(in_no, STDIN_FILENO);
        close(in_no);
        fi = 1;
    }
    if (out_no != STDOUT_FILENO)
    {
        dup2(out_no, STDOUT_FILENO);
        close(out_no);
        fo = 1;
    }

    if (parsed_line[0] == NULL)
    {
        return -1;
    }
    */

    // printf("DEBUG 3: %s\n", parsed_line[0]);

    for (; i < sizeof(builtin_func_list) / sizeof(char *); i++)
    {
        if (strcmp(parsed_line[0], builtin_func_list[i]) == 0)
        {
            int status = (*builtin_func[i])(parsed_line);
            fflush(stdout);
            return (status);
        }
    }
    int status = execute(parsed_line);
    fflush(stdout);
    return (status);
}

// Breaking a command into its piped components
char **pipe_stuff(char *line)
{
    int bufsize = MAXLIST;
    char **piped_line = malloc(bufsize * sizeof(char *));
    int i = 0;
    piped_line[i] = strsep(&line, "|");
    while (piped_line[i] != NULL)
    {
        piped_line[++i] = strsep(&line, "|");
        if (i >= bufsize)
        {
            bufsize *= 2;
            piped_line = realloc(piped_line, bufsize * sizeof(char *));
        }
    }

    return piped_line;
}

// Splitting a string based on characters in TOK_DELIM
char **split_line(char *line)
{
    int bufsize = 64;
    int i = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token;

    if (!tokens)
    {
        fprintf(stderr, "allocation error in split_line: tokens\n");
        exit(EXIT_FAILURE);
    }
    token = strtok(line, TOK_DELIM);
    while (token != NULL)
    {
        /* handle comments */
        if (token[0] == '#')
        {
            break;
        }
        tokens[i] = token;
        i++;
        if (i >= bufsize)
        {
            bufsize += bufsize;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens)
            {
                fprintf(stderr, "reallocation error in split_line: tokens");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, TOK_DELIM);
    }
    tokens[i] = NULL;
    return (tokens);
}

// Creates a cascade of children to execute commands with any number of pipes, each child executes its part and gives output to parent
int cmd_handle_pipe(char **piped_line, int n)
{

    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid)
    {
        // parent
        wait(NULL);
        return -1;
    }
    else
    {
        int fd[n][2];
        for (int i = 0; i < n; i++)
        {
            if (pipe(fd[i]) < 0)
                exit(0);
        }
        pid_t pid2[n];
        int j;
        for (j = 0; j < n; j++)
        {
            pid2[j] = fork();
            if (pid2[j] < 0)
            {
                printf("ERROR 1\n");
                break;
            }
            if (pid2[j])
                break;
        }
        /*
        // printf("DEBUG 9: %s %d %d %d\n", piped_line[n - j - 1], j, getpid(), n);

        for (int i = 0; i < n; i++)
        {
            if (i == 0)
            {
                printf("s ");
            }
            else
            {
                printf("%d ", i);
            }
        }
        printf("\n");
        for (int i = 0; i < n; i++)
        {
            if (i == n - 1)
            {
                printf("s ");
            }
            else
            {
                printf("%d ", i + 1);
            }
        }
        printf("\n\n");

        // if (j != n - 1)
        // {
        //     printf("DEBUG 13: in of %d to read of %d\n", j, j);
        //     dup2(fd[j][0], STDIN_FILENO);
        // }
        // if (j != 0)
        // {
        //     printf("DEBUG 12: out of %d to write of %d\n", j, j - 1);
        //     dup2(fd[j - 1][1], STDOUT_FILENO);
        // }
        */

        for (int i = 0; i < n; i++)
        {
            // printf("DEBUG 14: i=%d for %d\n", i, j);
            if ((i == j) && (j != n - 1))
            {
                // printf("DEBUG 13: in of %d to read of %d\n", j, j);
                dup2(fd[j][0], STDIN_FILENO);
            }

            close(fd[i][0]);
        }
        for (int i = 0; i < n; i++)
        {
            if ((i + 1 == j))
            {
                // printf("DEBUG 12: out of %d to write of %d\n", j, j - 1);
                dup2(fd[j - 1][1], STDOUT_FILENO);
            }
            close(fd[i][1]);
        }

        if (j < n - 1)
            wait(&pid + j);
        cmd_handle(split_line(piped_line[n - j - 1]));
        fflush(stdout);
        exit(0);
    }
    return -1;
}

// Executes a single command (irrespective of the presence of pipes)
int exec_handle(char *line)
{
    char **parsed_line;
    char **piped_line;
    int status = -1;
    piped_line = pipe_stuff(line);
    if (piped_line[1])
    {
        int count_pcmd = 1;
        while (piped_line[count_pcmd])
            count_pcmd++;
        status = cmd_handle_pipe(piped_line, count_pcmd);
        fflush(stdout);
    }
    else
    {
        parsed_line = split_line(piped_line[0]);
        status = cmd_handle(parsed_line);
        fflush(stdout);
    }
    free(piped_line);
    return status;
}

// Breaks a line into multiple commands (if present)
char **mult_finder(char *line)
{
    int bufsize = MAXLIST;
    char **mult_line = malloc(bufsize * sizeof(char *));
    int i = 0;
    mult_line[i] = strsep(&line, LINE_END);
    while (mult_line[i] != NULL)
    {
        mult_line[++i] = strsep(&line, LINE_END);
        if (i >= bufsize)
        {
            bufsize *= 2;
            mult_line = realloc(mult_line, bufsize * sizeof(char *));
        }
    }

    return mult_line;
}

// Executes all the (possible several) commands in a line
int mult_handle(char *line)
{
    int status = -1;

    char **mult_cmds = mult_finder(line);

    int i = 0;
    while (mult_cmds[i])
        status = exec_handle(mult_cmds[i++]);

    return status;
}

// Prompts for input
void prompt()
{
    getcwd(cwd, sizeof(cwd));
    p_blue(1);
    printf("(%s)", getlogin());
    p_reset();
    printf(" @ ");
    p_yellow(1);
    printf("[~%s]", cwd);
    p_reset();
    printf(" -\\\n$ ");
    fflush(stdout);
}

// Time and execute lines
int cmd_main(char *line)
{
    int status = -1;
    prompt();

    p_green(1);
    if (takeInput(line) != -1)
        return -1;
    printf("\n");
    p_reset();
    fflush(stdout);

    gettimeofday(&t1, NULL);
    status = mult_handle(line);
    gettimeofday(&t2, NULL);

    time_taken = (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;
    p_blue(1);
    if (time_taken > 1000.0)
    {
        time_taken /= 1000;
        printf("\n(%f s)\n\n", time_taken);
    }
    else
    {
        printf("\n(%f ms)\n\n", time_taken);
    }
    p_reset();

    return status;
}

int main()
{
    signal(SIGINT, ctr_C_handle);
    printf("\033[H\033[J");

    char line[MAXCOM];
    int status = -1;

    while (status == -1)
        status = cmd_main(line);

    return 0;
}
