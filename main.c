#include <dirent.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CLASH_RL_BUFSIZE 1024
#define CLASH_TOKEN_BUFSIZE 64
#define CLASH_TOKEN_DELIM " \t\r\n\a"

int get_clash_builtin_cmd_count();
char *get_dir_name();

int clash_launch(char **args);
int clash_execute(char **args);
void clash_loop(void);

char *clash_read_line(void);
char **clash_split_line(char *line);

int clash_echo(char **args);
int clash_cd(char **args);
int clash_pwd(char **args);
int clash_mkdir(char **args);
int clash_touch(char **args);
int clash_ls(char **args);
int clash_exit(char **args);

char *builtin_cmd_str[] = {"echo", "cd", "pwd", "mkdir", "touch", "ls", "exit"};

int (*builtin_cmd_func[])(char **) = {&clash_echo,  &clash_cd,    &clash_pwd,
                                      &clash_mkdir, &clash_touch, &clash_ls,
                                      &clash_exit};

int main() {
    clash_loop();

    return 0;
}

int get_clash_builtin_cmd_count() {
    return sizeof(builtin_cmd_str) / sizeof(char *);
}

char *get_dir_name() {
    int bufsize = CLASH_RL_BUFSIZE;
    char *dirpath = malloc(sizeof(char) * bufsize);
    getcwd(dirpath, bufsize);
    if (dirpath == NULL) {
        perror("clash");
    }

    free(dirpath);

    return basename(dirpath);
}

int clash_launch(char **args) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            perror("clash");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("clash");
    } else {
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int clash_execute(char **args) {
    int i;
    if (args[0] == NULL) {
        return 1;
    }

    for (i = 0; i < get_clash_builtin_cmd_count(); i++) {
        if (strcmp(args[0], builtin_cmd_str[i]) == 0) {
            return (builtin_cmd_func[i])(args);
        }
    }

    return clash_launch(args);
}

char *clash_read_line(void) {
    int bufsize = CLASH_RL_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if (!buffer) {
        fprintf(stderr, "clash: memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        c = getchar();
        if (c == EOF) {
            exit(EXIT_SUCCESS);
        } else if (c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;

        if (position >= bufsize) {
            bufsize += CLASH_RL_BUFSIZE;
            buffer = realloc(buffer, bufsize);
            if (!buffer) {
                fprintf(stderr, "clash: memory allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

char **clash_split_line(char *line) {
    int bufsize = CLASH_TOKEN_BUFSIZE;
    int position = 0;
    char **tokens = malloc(sizeof(char *) * bufsize);
    char *token, *token_backup;

    if (!tokens) {
        fprintf(stderr, "clash: memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, CLASH_TOKEN_DELIM);

    for (; token != NULL;) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += CLASH_TOKEN_BUFSIZE;
            token_backup = token;
            tokens = realloc(tokens, sizeof(char *) * bufsize);
            if (!tokens) {
                free(token_backup);
                fprintf(stderr, "clash: memory allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, CLASH_TOKEN_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

void clash_loop(void) {
    char *line;
    char **args;
    int status;

    do {
        printf("%s> ", get_dir_name());
        line = clash_read_line();
        args = clash_split_line(line);
        status = clash_execute(args);

        free(line);
        free(args);
    } while (status);
}

int clash_echo(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "clash: argument error");
    } else {
        fprintf(stdout, "%s\n", args[1]);
    }

    return 1;
}

int clash_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "clash: argument error");
    } else {
        if (chdir(args[1]) != 0) {
            perror("clash");
        }
    }

    return 1;
}

int clash_pwd(char **args) {
    int bufsize = CLASH_RL_BUFSIZE;
    char *dirname = malloc(sizeof(char) * bufsize);
    getcwd(dirname, bufsize);
    if (dirname == NULL) {
        perror("clash");
    }

    fprintf(stdout, "%s\n", dirname);
    free(dirname);

    return 1;
}

int clash_mkdir(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "clash: argument error");
    } else {
        if (mkdir(args[1], 0755) != 0) {
            perror("clash");
        }
    }

    return 1;
}

int clash_touch(char **args) {
    FILE *fp;
    fp = fopen(args[1], "w");
    if (fp == NULL) {
        perror("clash");
    }
    fclose(fp);

    return 1;
}

int clash_ls(char **args) {
    struct dirent *ent;
    struct stat stat_buf;
    DIR *pdir;

    if (args[1] == NULL) {
        pdir = opendir("./");
    } else {
        pdir = opendir(args[1]);
    }

    if (pdir == NULL) {
        perror("clash");
    }
    for (; (ent = readdir(pdir)) != NULL;) {
        fprintf(stdout, "%s\t", ent->d_name);
    }
    putchar('\n');
    /*
    if (args[1] != NULL && strcmp(args[1], "-l") == 0) {
        if (args[2] == NULL) {
            pdir = opendir("./");
        } else {
            pdir = opendir(args[2]);
        }

        if (pdir == NULL) {
            perror("clash");
        }

        fprintf(stdout, "%-16s\t%s\n", "name", "update");
        for (; (ent = readdir(pdir)) != NULL;) {
            if (stat(ent->d_name, &stat_buf) == 0) {
                fprintf(stdout, "%-16s\t%s", ent->d_name,
                        ctime(&stat_buf.st_ctimespec));
            }
        }
        putchar('\n');
    } else {
        if (args[1] == NULL) {
            pdir = opendir("./");
        } else {
            pdir = opendir(args[1]);
        }

        if (pdir == NULL) {
            perror("clash");
        }
        for (; (ent = readdir(pdir)) != NULL;) {
            fprintf(stdout, "%s\t", ent->d_name);
        }
        putchar('\n');
    }
    */
    closedir(pdir);
    return 1;
}
int clash_exit(char **args) { return 0; }