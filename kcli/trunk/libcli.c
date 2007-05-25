//=============================================================================
// This file is part of Kcli, a command line interface in a Linux kernel
// module for embedded Linux applications.
//
// Copyright (C) 2007 Christopher Stone.
//
// This file was derived directly from libcli, which is a user space library
// developed by David Parrish and Brendan O'Dea. The original source code
// file contains no specific copyright notice, but, it was released under
// the GNU Lesser General Public License, and this file retains that
// license.
//
// This file is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License, version 2.1, as published
// by the Free Software Foundation.
//
// Kcli is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You will find documentation for Kcli at http://www.javadevices.com
//
// You will find the maintainers and current source code of Kcli at BerliOS:
//    http://developer.berlios.de/projects/jem/
//
//=============================================================================
// libcli.c
//
// Embedded CLI implementation.
//
//=============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/in.h>
#include <linux/syscalls.h>
#include <linux/net.h>
#include "libcli.h"
#include "regex.h"


extern struct socket *clientSocket;

static void   cli_error(struct cli_def *cli, char *format, ...)
                 __attribute__((format (printf, 2, 3)));


enum cli_states {
    STATE_LOGIN,
    STATE_PASSWORD,
    STATE_NORMAL,
    STATE_ENABLE_PASSWORD,
    STATE_ENABLE
};

struct unp {
    char *username;
    char *password;
    struct unp *next;
};

#define OUTBUFMAX   256
static char outbuf[OUTBUFMAX];
static unsigned int outbufsz;

/* free and zero (to avoid double-free) */
#define free_z(p) kfree(p), (p) = 0

static int kcli_socket_read(void *buf, size_t nbyte)
{
    struct kvec             iov = {buf, nbyte};
    struct msghdr           msg = {NULL, };

    return kernel_recvmsg(clientSocket, &msg, &iov, 1, nbyte, 0);
}

static int kcli_socket_write(const void *buf, size_t nbyte)
{
    struct kvec             iov;
    struct msghdr           msg = {NULL, };

    iov.iov_base        = (void *) buf;
    iov.iov_len         = nbyte;

    return kernel_sendmsg(clientSocket, &msg, &iov, 1, nbyte);
}

static char *strdup(char *s)
{
    char *result = kzalloc(strlen(s) + 1, GFP_KERNEL);
    if (result == NULL) return NULL;
    return strcpy(result, s);
}

void cli_set_auth_callback(struct cli_def *cli,
                           int (*auth_callback)(char *, char *))
{
    down(&cli->cli_mtx);
    cli->auth_callback = auth_callback;
    up(&cli->cli_mtx);
}

void cli_set_enable_callback(struct cli_def *cli,
                             int (*enable_callback)(char *))
{
    down(&cli->cli_mtx);
    cli->enable_callback = enable_callback;
    up(&cli->cli_mtx);
}

void cli_allow_user(struct cli_def *cli, char *username, char *password)
{
    struct unp *n = kzalloc(sizeof(struct unp), GFP_KERNEL);
    n->username = strdup(username);
    n->password = strdup(password);
    n->next = 0;

    down(&cli->cli_mtx);
    if (cli->users) {
        struct unp *u = cli->users;
        while (u->next)
            u = u->next;

        u->next = n;
    } else
        cli->users = n;
    up(&cli->cli_mtx);
}

void cli_allow_enable(struct cli_def *cli, char *password)
{
    down(&cli->cli_mtx);
    kfree(cli->enable_password);
    cli->enable_password = strdup(password);
    up(&cli->cli_mtx);
}

void cli_deny_user(struct cli_def *cli, char *username)
{
    struct unp *u;
    struct unp *p;

    down(&cli->cli_mtx);
    for (p = 0, u = cli->users; u; p = u, u = u->next) {
        if (strcmp(username, u->username))
            continue;

        if (p)
            p->next = u->next;
        else
            cli->users = u->next;

        kfree(u->username);
        kfree(u->password);
        kfree(u);
        break;
    }
    up(&cli->cli_mtx);
}

void cli_set_banner(struct cli_def *cli, char *banner)
{
    down(&cli->cli_mtx);
    if (cli->banner) free_z(cli->banner);
    if (banner && *banner)
        cli->banner = strdup(banner);
    if (cli->banner[strlen(cli->banner)-1] == '\n') {
        cli->banner[strlen(cli->banner)-1] = '\0';
    }
    up(&cli->cli_mtx);
}

void cli_set_hostname(struct cli_def *cli, char *hostname)
{
    down(&cli->cli_mtx);
    if (cli->hostname) free_z(cli->hostname);
    if (hostname && *hostname)
        cli->hostname = strdup(hostname);
    if (cli->hostname[strlen(cli->hostname)-1] == '\n') {
        cli->hostname[strlen(cli->hostname)-1] = '\0';
    }
    up(&cli->cli_mtx);
}

void cli_set_promptchar(struct cli_def *cli, char *promptchar)
{
    down(&cli->cli_mtx);
    if (cli->promptchar) kfree(cli->promptchar);
    cli->promptchar = strdup(promptchar);
    up(&cli->cli_mtx);
}

int cli_set_privilege(struct cli_def *cli, int priv)
{
    int old = cli->privilege;
    cli->privilege = priv;

    if (priv != old)
        cli_set_promptchar(cli, priv == PRIVILEGE_PRIVILEGED ? "# " : "> ");

    return old;
}

static void set_modestring(struct cli_def *cli, char *modestring)
{
    if (cli->modestring) free_z(cli->modestring);
    if (modestring)
        cli->modestring = strdup(modestring);
}

int cli_set_configmode(struct cli_def *cli, int mode, char *config_desc)
{
    int old;

    down(&cli->cli_mtx);
    old         = cli->mode;
    cli->mode   = mode;

    if (mode != old) {
        if (!cli->mode) {
            /* not config mode */
            set_modestring(cli, 0);
        } else if (config_desc && *config_desc) {
            char string[64];
            snprintf(string, sizeof(string), "(config-%s)", config_desc);
            set_modestring(cli, string);
        } else {
            set_modestring(cli, "(config)");
        }
    }

    up(&cli->cli_mtx);
    return old;
}

static int build_shortest(struct cli_def *cli, struct cli_command *commands)
{
    struct cli_command *c;

    for (c = commands; c; c = c->next) {
        int len = strlen(c->command);
        for (c->unique_len = 1; c->unique_len <= len; c->unique_len++) {
            int foundmatch = 0;
            struct cli_command *p;

            for (p = commands; p; p = p->next) {
                if (c == p)
                    continue;

                if (!strncmp(p->command, c->command, c->unique_len))
                    foundmatch++;
            }

            if (!foundmatch)
                break;
        }

        if (c->children)
            build_shortest(cli, c->children);
    }

    return CLI_OK;
}

struct cli_command *cli_register_command(struct cli_def *cli,
                                         struct cli_command *parent, char *command,
                                         int (*callback)(struct cli_def *cli, char *, char **, int),
                                         int privilege, int mode, char *help) {
    struct cli_command *c;
    struct cli_command *p;

    if (!command)
        return 0;

    if (!(c = kzalloc(sizeof(struct cli_command), GFP_KERNEL)))
        return 0;

    c->callback = callback;
    c->next = 0;
    c->command = strdup(command);
    c->parent = parent;
    c->privilege = privilege;
    c->mode = mode;

    if (help)
        c->help = strdup(help);

    if (parent) {
        if (!parent->children) {
            parent->children = c;
        } else {
            for (p = parent->children; p && p->next; p = p->next)
                ;

            if (p)
                p->next = c;
        }
    } else {
        if (!cli->commands) {
            cli->commands = c;
        } else {
            for (p = cli->commands; p && p->next; p = p->next)
                ;

            if (p)
                p->next = c;
        }
    }

    down(&cli->cli_mtx);
    build_shortest(cli, parent ? parent : cli->commands);
    up(&cli->cli_mtx);
    return c;
}

int cli_unregister_command(struct cli_def *cli, char *command)
{
    struct cli_command *c;
    struct cli_command *p = 0;

    if (!(command && cli->commands))
        return CLI_ERROR;

    down(&cli->cli_mtx);
    for (c = cli->commands; c; p = c, c = c->next) {
        if (strcmp(c->command, command))
            continue;

        if (p)
            p->next = c->next;
        else
            cli->commands = c->next;

        kfree(c->command);
        kfree(c->help);
        kfree(c);
        up(&cli->cli_mtx);
        return CLI_OK;
    }
    up(&cli->cli_mtx);

    return CLI_ERROR;
}

static char *command_name(struct cli_def *cli, struct cli_command *command)
{
    free_z(cli->_name_buf);

    while (command) {
        char *o = cli->_name_buf;
        int sz = strlen(command->command) + 1;
        if (o)
            sz += strlen(o) + 1;

        cli->_name_buf = kzalloc(sz, GFP_KERNEL);
        if (o)
            sprintf(cli->_name_buf, "%s %s", command->command, o);
        else
            strcpy(cli->_name_buf, command->command);

        command = command->parent;
        if (o) kfree(o);
    }

    return cli->_name_buf;
}

static int show_help(struct cli_def *cli, struct cli_command *c)
{
    struct cli_command *p;

    for (p = c; p; p = p->next) {
        if (p->command && p->callback && cli->privilege >= p->privilege &&
            (p->mode == cli->mode || p->mode == MODE_ANY)) {
            cli_error(cli, "  %-20s %s", command_name(cli, p), p->help ? : "");
        }

        if (p->children)
            show_help(cli, p->children);
    }

    return CLI_OK;
}

static int cmd_enable(struct cli_def *cli, char *command,
                      char *argv[], int argc)
{
    if (cli->privilege == PRIVILEGE_PRIVILEGED)
        return CLI_OK;

    if (!cli->enable_password && !cli->enable_callback) {
        /* no password required, set privilege immediately */
        cli_set_privilege(cli, PRIVILEGE_PRIVILEGED);
        cli_set_configmode(cli, MODE_EXEC, 0);
    } else {
        /* require password entry */
        cli->state = STATE_ENABLE_PASSWORD;
    }

    return CLI_OK;
}

static int cmd_disable(struct cli_def *cli, char *command,
                       char *argv[], int argc)
{
    cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
    cli_set_configmode(cli, MODE_EXEC, 0);
    return CLI_OK;
}

static int cmd_help(struct cli_def *cli, char *command,
                    char *argv[], int argc)
{
    cli_error(cli, "\nCommands available:");
    show_help(cli, cli->commands);
    return CLI_OK;
}

static int cmd_history(struct cli_def *cli, char *command,
                       char *argv[], int argc)
{
    int i;

    cli_error(cli, "\nCommand history:");
    for (i = 0; i < MAX_HISTORY; i++)
        if (cli->history[i])
            cli_error(cli, "%3d. %s", i, cli->history[i]);

    return CLI_OK;
}

static int cmd_quit(struct cli_def *cli, char *command,
                    char *argv[], int argc)
{
    cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
    cli_set_configmode(cli, MODE_EXEC, 0);
    return CLI_QUIT;
}

static int cmd_exit(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (cli->mode == MODE_EXEC)
        return cmd_quit(cli, command, argv, argc);

    if (cli->mode > MODE_CONFIG)
        cli_set_configmode(cli, MODE_CONFIG, 0);
    else
        cli_set_configmode(cli, MODE_EXEC, 0);

    return CLI_OK;
}

static int cmd_configure_terminal(struct cli_def *cli, char *command,
                                  char *argv[], int argc)
{
    cli_set_configmode(cli, MODE_CONFIG, 0);
    return CLI_OK;
}

struct cli_def *cli_init() {
    struct cli_def *cli;
    struct cli_command *c;

    if (!(cli = kzalloc(sizeof(struct cli_def), GFP_KERNEL)))
        return NULL;

    init_MUTEX(&cli->cli_mtx);

    cli_register_command(cli, 0, "help", cmd_help, PRIVILEGE_UNPRIVILEGED,
                         MODE_ANY, "Show available commands");

    cli_register_command(cli, 0, "quit", cmd_quit, PRIVILEGE_UNPRIVILEGED,
                         MODE_ANY, "Disconnect");

    cli_register_command(cli, 0, "logout", cmd_quit, PRIVILEGE_UNPRIVILEGED,
                         MODE_ANY, "Disconnect");

    cli_register_command(cli, 0, "exit", cmd_exit, PRIVILEGE_UNPRIVILEGED,
                         MODE_ANY, "Exit from current mode");

    cli_register_command(cli, 0, "history", cmd_history, PRIVILEGE_UNPRIVILEGED,
                         MODE_ANY, "Show a list of previously run commands");

    cli_register_command(cli, 0, "enable", cmd_enable, PRIVILEGE_UNPRIVILEGED,
                         MODE_EXEC, "Turn on privileged commands");

    cli_register_command(cli, 0, "disable", cmd_disable, PRIVILEGE_PRIVILEGED,
                         MODE_EXEC, "Turn off privileged commands");

    c = cli_register_command(cli, 0, "configure", 0, PRIVILEGE_PRIVILEGED,
                             MODE_EXEC, "Enter configuration mode");

    cli_register_command(cli, c, "terminal", cmd_configure_terminal,
                         PRIVILEGE_PRIVILEGED, MODE_EXEC,
                         "Configure from the terminal");

    cli->_print_buf     = kzalloc(4095, GFP_KERNEL);
    cli->_print_bufsz   = 4096;
    if (cli->_print_buf == NULL) {
        kfree(cli);
        return NULL;
    }
    cli->_name_buf      = kzalloc(1023, GFP_KERNEL);
    if (cli->_name_buf == NULL) {
        kfree(cli);
        kfree(cli->_print_buf);
        return NULL;
    }
    cli->privilege      = cli->mode = -1;
    cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
    cli_set_configmode(cli, MODE_EXEC, 0);
    return cli;
}

static void unregister_all(struct cli_def *cli, struct cli_command *command)
{
    struct cli_command *c;

    if (!command)
        command = cli->commands;

    if (!command)
        return;

    for (c = command; c; ) {
        struct cli_command *p = c->next;

        /* unregister all child commands */
        if (c->children)
            unregister_all(cli, c->children);

        kfree(c->command);
        kfree(c->help);
        kfree(c);
        c = p;
    }
}

int cli_done(struct cli_def *cli)
{
    struct unp *u = cli->users;

    /* free all users */
    while (u) {
        struct unp *n = u->next;
        kfree(u->username);
        kfree(u->password);
        kfree(u);
        u = n;
    }

    /* free all commands */
    unregister_all(cli, 0);

    kfree(cli->banner);
    kfree(cli->promptchar);
    kfree(cli->hostname);
    free_z(cli);

    return CLI_OK;
}

static int add_history(struct cli_def *cli, char *cmd)
{
    int i = 0;

    /* table full */
    if (cli->history[MAX_HISTORY-1]) {
        kfree(cli->history[0]);
        for (; i < MAX_HISTORY-1; i++)
            cli->history[i] = cli->history[i+1];

        cli->history[i] = 0;
    }

    while (cli->history[i])
        i++;

    cli->history[i] = strdup(cmd);
    return i + 1;
}

static int parse_line(char *line, char *words[], int max_words)
{
    int nwords = 0;
    char *p = line;
    char *word_start = line;
    int inquote = 0;

    while (nwords < max_words - 1) {
        if (!*p || *p == inquote ||
            (word_start && !inquote && (*p == ' ' || *p == '|'))) {
            if (word_start) {
                int len = p - word_start;

                memcpy(words[nwords] = kzalloc(len + 1, GFP_KERNEL), word_start, len);
                words[nwords++][len] = 0;
            }

            if (!*p)
                break;

            if (inquote)
                p++; /* skip over trailing quote */

            inquote = 0;
            word_start = 0;
        } else if (*p == '"' || *p == '\'') {
            inquote = *p++;
            word_start = p;
        } else {
            if (!word_start) {
                if (*p == '|')
                    words[nwords++] = strdup("|");
                else if (*p != ' ')
                    word_start = p;
            }

            p++;
        }
    }

    return nwords;
}

static char *join_words(int argc, char **argv)
{
    char *p;
    int len = 0;
    int i;

    for (i = 0; i < argc; i++) {
        if (i)
            len += 1;

        len += strlen(argv[i]);
    }

    p = kzalloc(len + 1, GFP_KERNEL);
    p[0] = 0;

    for (i = 0; i < argc; i++) {
        if (i)
            strcat(p, " ");

        strcat(p, argv[i]);
    }

    return p;
}

struct match_filter_state {
    int flags;
#define MATCH_REGEX     1
#define MATCH_INVERT        2
    union {
        char *string;
        regex_t re;
    } match;
};

static int match_filter(struct cli_def *cli, char *string, void *data)
{
    struct match_filter_state *state = data;
    int r = CLI_ERROR;

    if (!string) { /* clean up */
        if (state->flags & MATCH_REGEX)
            regfree(&state->match.re);
        else
            kfree(state->match.string);

        kfree(state);
        return CLI_OK;
    }

    if (state->flags & MATCH_REGEX) {
        if (!regexec(&state->match.re, string, 0, 0, 0))
            r = CLI_OK;
    } else {
        if (strstr(string, state->match.string))
            r = CLI_OK;
    }

    if (state->flags & MATCH_INVERT) {
        if (r == CLI_OK)
            r = CLI_ERROR;
        else
            r = CLI_OK;
    }

    return r;
}

static int match_filter_init(struct cli_def *cli, int argc, char **argv,
                             struct cli_filter *filt)
{
    struct match_filter_state *state;
    int rflags;
    int i;
    char *p;

    if (argc < 2) {
        outbufsz = snprintf(outbuf, OUTBUFMAX, "Match filter requires an argument\r\n");
        kcli_socket_write(outbuf, outbufsz);

        return CLI_ERROR;
    }

    filt->filter = match_filter;
    filt->data = state = kzalloc(sizeof(struct match_filter_state), GFP_KERNEL);

    if (argv[0][0] == 'i' || /* include/exclude */
        (argv[0][0] == 'e' && argv[0][1] == 'x')) {
        if (argv[0][0] == 'e')
            state->flags = MATCH_INVERT;

        state->match.string = join_words(argc-1, argv+1);
        return CLI_OK;
    }

    state->flags = MATCH_REGEX;

    /* grep/egrep */
    rflags = REG_NOSUB;
    if (argv[0][0] == 'e') /* egrep */
        rflags |= REG_EXTENDED;

    i = 1;
    while (i < argc - 1 && argv[i][0] == '-' && argv[i][1]) {
        int last = 0;
        p = &argv[i][1];

        if (strspn(p, "vie") != strlen(p))
            break;

        while (*p) {
            switch (*p++) {
            case 'v':
                state->flags |= MATCH_INVERT;
                break;

            case 'i':
                rflags |= REG_ICASE;
                break;

            case 'e':
                last++;
                break;
            }
        }

        i++;
        if (last)
            break;
    }

    p = join_words(argc-i, argv+i);
    if ((i = regcomp(&state->match.re, p, rflags))) {
        outbufsz = snprintf(outbuf, OUTBUFMAX, "Invalid pattern \"%s\"\r\n", p);
        kcli_socket_write(outbuf, outbufsz);

        kfree(p);
        return CLI_ERROR;
    }

    kfree(p);
    return CLI_OK;
}

struct range_filter_state {
    int matched;
    char *from;
    char *to;
};

static int range_filter(struct cli_def *cli, char *string, void *data)
{
    struct range_filter_state *state = data;
    int r = CLI_ERROR;

    if (!string) { /* clean up */
        kfree(state->from);
        kfree(state->to);
        kfree(state);
        return CLI_OK;
    }

    if (!state->matched)
        state->matched = !!strstr(string, state->from);

    if (state->matched) {
        r = CLI_OK;
        if (state->to && strstr(string, state->to))
            state->matched = 0;
    }

    return r;
}

static int range_filter_init(struct cli_def *cli, int argc, char **argv,
                             struct cli_filter *filt)
{
    struct range_filter_state *state;
    char *from = 0;
    char *to = 0;

    if (!strncmp(argv[0], "bet", 3)) { /* between */
        if (argc < 3) {
            outbufsz = snprintf(outbuf, OUTBUFMAX, "Between filter requires 2 arguments\r\n");
            kcli_socket_write(outbuf, outbufsz);

            return CLI_ERROR;
        }

        from = strdup(argv[1]);
        to = join_words(argc-2, argv+2);
    } else { /* begin */
        if (argc < 2) {
            outbufsz = snprintf(outbuf, OUTBUFMAX, "Begin filter requires an argument\r\n");
            kcli_socket_write(outbuf, outbufsz);

            return CLI_ERROR;
        }

        from = join_words(argc-1, argv+1);
    }

    filt->filter = range_filter;
    filt->data = state = kzalloc(sizeof(struct range_filter_state), 1);

    state->from = from;
    state->to = to;

    return CLI_OK;
}

static int count_filter(struct cli_def *cli, char *string, void *data)
{
    int *count = data;

    if (!string) { /* clean up */
        /* print count */
        outbufsz = snprintf(outbuf, OUTBUFMAX, "%d\r\n", *count);
        kcli_socket_write(outbuf, outbufsz);

        kfree(count);
        return CLI_OK;
    }

    while (*string == ' ')
        string++;

    if (*string)
        (*count)++;  /* only count non-blank lines */

    return CLI_ERROR; /* no output */
}

static int count_filter_init(struct cli_def *cli, int argc,
                             char **argv, struct cli_filter *filt)
{
    if (argc > 1) {
        outbufsz = snprintf(outbuf, OUTBUFMAX, "Count filter does not take arguments\r\n");
        kcli_socket_write(outbuf, outbufsz);

        return CLI_ERROR;
    }

    filt->filter = count_filter;
    filt->data = kzalloc(sizeof(int), GFP_KERNEL);

    return CLI_OK;
}

static int find_command(struct cli_def *cli, struct cli_command *commands,
                        int num_words, char *words[], int start_word, int filters[])
{
    struct cli_command *c;
    int c_words = num_words;

    if (filters[0])
        c_words = filters[0];

    /* deal with ? for help */
    if (!words[start_word])
        return CLI_ERROR;

    if (words[start_word][strlen(words[start_word])-1] == '?') {
        int l = strlen(words[start_word])-1;

        for (c = commands; c; c = c->next) {
            if (!strncmp(c->command, words[start_word], l) &&
                (c->callback || c->children) &&
                cli->privilege >= c->privilege &&
                (c->mode == cli->mode || c->mode == MODE_ANY)) {
                cli_error(cli, "  %-20s %s", c->command, c->help ? : "");
            }
        }

        return CLI_OK;
    }

    for (c = commands; c; c = c->next) {
        if (cli->privilege < c->privilege)
            continue;

        if (strncmp(c->command, words[start_word], c->unique_len))
            continue;

        if (strncmp(c->command, words[start_word],
                        strlen(words[start_word])))
            continue;

        /* drop out of config submode */
        if (cli->mode > MODE_CONFIG && c->mode == MODE_CONFIG)
            cli_set_configmode(cli, MODE_CONFIG, 0);

        if (c->mode == cli->mode || c->mode == MODE_ANY) {
            int rc = CLI_OK;
            int f;
            struct cli_filter **filt = &cli->filters;

            /* found a word! */
            if (!c->children) {
                /* last word */
                if (!c->callback) {
                    cli_error(cli, "No callback for \"%s\"",
                              command_name(cli, c));

                    return CLI_ERROR;
                }
            } else {
                if (start_word == c_words - 1) {
                    cli_error(cli, "Incomplete command");
                    return CLI_ERROR;
                }

                return find_command(cli, c->children, num_words, words,
                                    start_word + 1, filters);
            }

            if (!c->callback) {
                cli_error(cli, "Internal server error processing \"%s\"",
                          command_name(cli, c));

                return CLI_ERROR;
            }

            for (f = 0; rc == CLI_OK && filters[f]; f++) {
                int n = num_words;
                char **argv;
                int argc;
                int len;

                if (filters[f+1])
                    n = filters[f+1];

                if (filters[f] == n - 1) {
                    cli_error(cli, "Missing filter");
                    return CLI_ERROR;
                }

                argv = words + filters[f] + 1;
                argc = n - (filters[f] + 1);
                len = strlen(argv[0]);
                if (argv[argc-1][strlen(argv[argc-1])-1] == '?') {
                    if (argc == 1) {
                        cli_error(cli, "  %-20s %s", "begin",
                                  "Begin with lines that match");

                        cli_error(cli, "  %-20s %s", "between",
                                  "Between lines that match");

                        cli_error(cli, "  %-20s %s", "count",
                                  "Count of lines");

                        cli_error(cli, "  %-20s %s", "exclude",
                                  "Exclude lines that match");

                        cli_error(cli, "  %-20s %s", "include",
                                  "Include lines that match");

                        cli_error(cli, "  %-20s %s", "grep",
                                  "Include lines that match regex "
                                  "(options: -v, -i, -e)");

                        cli_error(cli, "  %-20s %s", "egrep",
                                  "Include lines that match extended regex");
                    } else {
                        if (argv[0][0] != 'c') /* count */
                            cli_error(cli, "  WORD");

                        if (argc > 2 || argv[0][0] == 'c') /* count */
                            cli_error(cli, "  <cr>");
                    }

                    return CLI_OK;
                }

                if (argv[0][0] == 'b' && len < 3) { /* [beg]in, [bet]ween */
                    cli_error(cli, "Ambiguous filter \"%s\" (begin, between)",
                              argv[0]);

                    return CLI_ERROR;
                }

                if (argv[0][0] == 'e' && len < 2) { /* [eg]rep, [ex]clude */
                    cli_error(cli, "Ambiguous filter \"%s\" (egrep, exclude)",
                              argv[0]);

                    return CLI_ERROR;
                }

                *filt = kzalloc(sizeof(struct cli_filter), GFP_KERNEL);

                if (!strncmp("include", argv[0], len) ||
                    !strncmp("exclude", argv[0], len) ||
                    !strncmp("grep", argv[0], len) ||
                    !strncmp("egrep", argv[0], len))
                    rc = match_filter_init(cli, argc, argv, *filt);
                else if (!strncmp("begin", argv[0], len) ||
                         !strncmp("between", argv[0], len))
                    rc = range_filter_init(cli, argc, argv, *filt);
                else if (!strncmp("count", argv[0], len))
                    rc = count_filter_init(cli, argc, argv, *filt);
                else {
                    cli_error(cli, "Invalid filter \"%s\"", argv[0]);
                    rc = CLI_ERROR;
                }

                if (rc == CLI_OK)
                    filt = &(*filt)->next;
                else
                    free_z(*filt);
            }

            if (rc == CLI_OK) {
                rc = c->callback(cli, command_name(cli, c),
                                 words + start_word + 1,
                                 c_words - start_word - 1);
            }

            while (cli->filters) {
                struct cli_filter *filt = cli->filters;

                /* call one last time to clean up */
                filt->filter(cli, 0, filt->data);
                cli->filters = filt->next;
                kfree(filt);
            }

            return rc;
        }
    }

    cli_error(cli, "Invalid %s \"%s\"",
              commands->parent ? "argument" : "command", words[start_word]);

    return CLI_ERROR;
}

static int run_command(struct cli_def *cli, char *command)
{
#define MAX_WORD 128
#define MAX_FILT 128
    char *words[MAX_WORD] = { 0};
    int filters[MAX_FILT] = { 0};
    int num_words;
    int r;
    int i;
    int f;

    if (!command)
        return CLI_ERROR;

    while (*command == ' ')
        command++;

    if (!*command)
        return CLI_OK;

    num_words = parse_line(command, words, MAX_WORD);
    for (i = f = 0; i < num_words && f < MAX_FILT-1; i++) {
        if (words[i][0] == '|')
            filters[f++] = i;
    }

    filters[f] = 0;

    if (num_words)
        r = find_command(cli, cli->commands, num_words, words, 0, filters);
    else
        r = CLI_ERROR;

    for (i = 0; i < num_words; i++)
        kfree(words[i]);

    if (r == CLI_QUIT)
        return r;

    return CLI_OK;
}

static int get_completions(char *command, char **completions,
                           int max_completions)
{
    return 0;
}

static void clear_line(int sockfd, char *cmd, int l, int cursor)
{
    int i;
    if (cursor < l)
        for (i = 0; i < (l - cursor); i++)
            kcli_socket_write(" ", 1);

    for (i = 0; i < l; i++)
        cmd[i] = '\b';

    for (; i < l * 2; i++)
        cmd[i] = ' ';

    for (; i < l * 3; i++)
        cmd[i] = '\b';

    kcli_socket_write(cmd, i);
    memset(cmd, 0, i);
    l = cursor = 0;
}

void cli_reprompt(struct cli_def *cli)
{
    cli->showprompt = 1;
}

void cli_regular(struct cli_def *cli, int (*callback)(struct cli_def *cli))
{
    cli->regular_callback = callback;
}

#define DES_PREFIX "{crypt}"    /* to distinguish clear text from DES crypted */
#define MD5_PREFIX "$1$"

static int pass_matches(char *pass, char *try)
{
    //int des;
    //if ((des = !strncasecmp(pass, DES_PREFIX, sizeof(DES_PREFIX)-1)))
    //    pass += sizeof(DES_PREFIX)-1;

    //if (des || !strncmp(pass, MD5_PREFIX, sizeof(MD5_PREFIX)-1))
    //    try= crypt(try, pass);

    return !strcmp(pass, try);
}

#define CTRL(c) (c - '@')

static int show_prompt(struct cli_def *cli, int sockfd)
{
    int len = 0;

    if (cli->hostname)
        len += kcli_socket_write(cli->hostname, strlen(cli->hostname));

    if (cli->modestring)
        len += kcli_socket_write(cli->modestring, strlen(cli->modestring));

    return len + kcli_socket_write(cli->promptchar, strlen(cli->promptchar));
}

int cli_loop(struct cli_def *cli, int sockfd)
{
    int n;
    unsigned char c;
    char *cmd = 0;
    char *oldcmd = 0;
    int l;
    int oldl = 0;
    int is_telnet_option = 0;
    int skip = 0;
    int esc = 0;
    int cursor = 0;
    int hist_sz = 0;
    char *username = 0;
    char *password = 0;
    char *negotiate =
    "\xFF\xFB\x03"
    "\xFF\xFB\x01"
    "\xFF\xFD\x03"
    "\xFF\xFD\x01";

    cli->state = STATE_LOGIN;

    memset(cli->history, 0, MAX_HISTORY);
    kcli_socket_write(negotiate, strlen(negotiate));

    if ((cmd = kzalloc(4096, GFP_KERNEL)) == 0)
        return CLI_ERROR;

    cli->client = sockfd;

    if (cli->banner)
        cli_error(cli, "%s", cli->banner);

    /* start off in unprivileged mode */
    cli_set_privilege(cli, PRIVILEGE_UNPRIVILEGED);
    cli_set_configmode(cli, MODE_EXEC, 0);

    /* no auth required? */
    if (!cli->users && !cli->auth_callback)
        cli->state = STATE_NORMAL;

    while (1) {
        int hist = -1;
        int lastchar = 0;
        struct timeval tm;

        cli->showprompt = 1;

        if (oldcmd) {
            l = cursor = oldl;
            oldcmd[l] = 0;
            cli->showprompt = 1;
            oldcmd = 0;
            oldl = 0;
        } else {
            memset(cmd, 0, 4096);
            l = 0;
            cursor = 0;
        }

        tm.tv_sec = 1;
        tm.tv_usec = 0;

        while (1) {
            if (cli->showprompt) {
                if (cli->state != STATE_PASSWORD &&
                    cli->state != STATE_ENABLE_PASSWORD)
                    kcli_socket_write("\r\n", 2);

                switch (cli->state) {
                case STATE_LOGIN:
                    kcli_socket_write("Username: ", 10);
                    break;

                case STATE_PASSWORD:
                    kcli_socket_write("Password: ", 10);
                    break;

                case STATE_NORMAL:
                case STATE_ENABLE:
                    show_prompt(cli, sockfd);
                    kcli_socket_write(cmd, l);
                    if (cursor < l) {
                        int n = l - cursor;
                        while (n--)
                            kcli_socket_write("\b", 1);
                    }

                    break;

                case STATE_ENABLE_PASSWORD:
                    kcli_socket_write("Password: ", 10);
                    break;

                }

                cli->showprompt = 0;
            }

            if ((n = kcli_socket_read(&c, 1)) < 0) {

                l = -1;
                break;
            }

            if (n == 0) {
                l = -1;
                break;
            }

            if (skip) {
                skip--;
                continue;
            }

            if (c == 255 && !is_telnet_option) {
                is_telnet_option++;
                continue;
            }

            if (is_telnet_option) {
                if (c >= 251 && c <= 254) {
                    is_telnet_option = c;
                    continue;
                }

                if (c != 255) {
                    is_telnet_option = 0;
                    continue;
                }

                is_telnet_option = 0;
            }

            /* handle ANSI arrows */
            if (esc) {
                if (esc == '[') {
                    /* remap to readline control codes */
                    switch (c) {
                    case 'A': /* Up */
                        c = CTRL('P');
                        break;

                    case 'B': /* Down */
                        c = CTRL('N');
                        break;

                    case 'C': /* Right */
                        c = CTRL('F');
                        break;

                    case 'D': /* Left */
                        c = CTRL('B');
                        break;

                    default:
                        c = 0;
                    }

                    esc = 0;
                } else {
                    esc = (c == '[') ? c : 0;
                    continue;
                }
            }

            if (c == 0)
                continue;

            if (c == '\n')
                continue;

            if (c == '\r') {
                if (cli->state != STATE_PASSWORD &&
                    cli->state != STATE_ENABLE_PASSWORD)
                    kcli_socket_write("\r\n", 2);

                break;
            }

            if (c == 27) {
                esc = 1;
                continue;
            }

            if (c == CTRL('C')) {
                kcli_socket_write("\a", 1);
                continue;
            }

            /* back word, backspace/delete */
            if (c == CTRL('W') || c == CTRL('H') || c == 0x7f) {
                int back = 0;

                if (c == CTRL('W')) { /* word */
                    int nc = cursor;

                    if (l == 0 || cursor == 0)
                        continue;

                    while (nc && cmd[nc - 1] == ' ') {
                        nc--;
                        back++;
                    }

                    while (nc && cmd[nc - 1] != ' ') {
                        nc--;
                        back++;
                    }
                } else { /* char */
                    if (l == 0 || cursor == 0) {
                        kcli_socket_write("\a", 1);
                        continue;
                    }

                    back = 1;
                }

                if (back) {
                    while (back--) {
                        if (l == cursor) {
                            cmd[--cursor] = 0;
                            if (cli->state != STATE_PASSWORD &&
                                cli->state != STATE_ENABLE_PASSWORD)
                                kcli_socket_write("\b \b", 3);
                        } else {
                            int i;
                            cursor--;
                            if (cli->state != STATE_PASSWORD &&
                                cli->state != STATE_ENABLE_PASSWORD) {
                                int len;
                                for (i = cursor; i <= l; i++)
                                    cmd[i] = cmd[i+1];

                                len = strlen(cmd + cursor);
                                kcli_socket_write("\b", 1);
                                kcli_socket_write(cmd + cursor, len);
                                kcli_socket_write(" ", 1);
                                for (i = 0; i <= len; i++)
                                    kcli_socket_write("\b", 1);
                            }
                        }

                        l--;
                    }

                    continue;
                }
            }

            /* redraw */
            if (c == CTRL('L')) {
                int i;
                int cursorback = l - cursor;

                if (cli->state == STATE_PASSWORD ||
                    cli->state == STATE_ENABLE_PASSWORD)
                    continue;

                kcli_socket_write("\r\n", 2);
                show_prompt(cli, sockfd);
                kcli_socket_write(cmd, l);

                for (i = 0; i < cursorback; i++)
                    kcli_socket_write("\b", 1);

                continue;
            }

            /* clear line */
            if (c == CTRL('U')) {
                if (cli->state == STATE_PASSWORD ||
                    cli->state == STATE_ENABLE_PASSWORD)
                    memset(cmd, 0, l);
                else
                    clear_line(sockfd, cmd, l, cursor);

                l = cursor = 0;
                continue;
            }

            /* kill to EOL */
            if (c == CTRL('K')) {
                if (cursor == l)
                    continue;

                if (cli->state != STATE_PASSWORD &&
                    cli->state != STATE_ENABLE_PASSWORD) {
                    int c;
                    for (c = cursor; c < l; c++)
                        kcli_socket_write(" ", 1);

                    for (c = cursor; c < l; c++)
                        kcli_socket_write("\b", 1);
                }

                memset(cmd + cursor, 0, l - cursor);
                l = cursor;
                continue;
            }

            /* EOT */
            if (c == CTRL('D')) {
                if (cli->state == STATE_PASSWORD ||
                    cli->state == STATE_ENABLE_PASSWORD)
                    break;

                if (l)
                    continue;

                strcpy(cmd, "quit");
                l = cursor = strlen(cmd);
                kcli_socket_write("quit\r\n", l + 2);
                break;
            }

            /* disable */
            if (c == CTRL('Z')) {
                if (cli->mode != MODE_EXEC) {
                    clear_line(sockfd, cmd, l, cursor);
                    cli_set_configmode(cli, MODE_EXEC, 0);
                    cli->showprompt = 1;
                }

                continue;
            }

            /* TAB completion */
            if (c == CTRL('I')) {
                char *completions[128];
                int num_completions = 0;

                if (cli->state == STATE_PASSWORD ||
                    cli->state == STATE_ENABLE_PASSWORD)
                    continue;

                if (cursor != l)
                    continue;

                if (l > 0)
                    num_completions = get_completions(cmd, completions, 128);

                if (num_completions == 0) {
                    kcli_socket_write("\a", 1);
                } else if (num_completions == 1) {
                    /* single completion */
                    int i;
                    for (i = l; i > 0; i--, cursor--) {
                        kcli_socket_write("\b", 1);
                        if (i == ' ') break;
                    }

                    strcpy((cmd + i), completions[0]);
                    l += strlen(completions[0]);
                    cursor = l;
                    kcli_socket_write(completions[0], strlen(completions[0]));
                } else if (lastchar == CTRL('I')) {
                    /* double tab */
                    int i;
                    kcli_socket_write("\r\n", 2);
                    for (i = 0; i < num_completions; i++) {
                        kcli_socket_write(completions[i], strlen(completions[i]));
                        if (i % 4 == 3)
                            kcli_socket_write("\r\n", 2);
                        else
                            kcli_socket_write("     ", 1);
                    }

                    if (i % 4 != 3)
                        kcli_socket_write("\r\n", 2);

                    cli->showprompt = 1;
                } else {
                    /* more than one completion */
                    lastchar = c;
                    kcli_socket_write("\a", 1);
                }
                continue;
            }

            /* history */
            if (c == CTRL('P') || c == CTRL('N')) {
                if (cli->state == STATE_PASSWORD ||
                    cli->state == STATE_ENABLE_PASSWORD)
                    continue;

                if (!hist_sz) /* empty */
                    continue;

                if (c == CTRL('P')) { /* Up */
                    if (hist == -1 /* inital */ || --hist < 0 /* wrap */)
                        hist = hist_sz - 1;
                } else { /* Down */
                    if (hist == -1 || ++hist > hist_sz - 1)
                        hist = 0;
                }

                /* show history item */
                clear_line(sockfd, cmd, l, cursor);
                memset(cmd, 0, 4096);
                strncpy(cmd, cli->history[hist], 4095);
                l = cursor = strlen(cmd);
                kcli_socket_write(cmd, l);
                continue;
            }

            /* left/right cursor motion */
            if (c == CTRL('B') || c == CTRL('F')) {
                if (c == CTRL('B')) { /* Left */
                    if (cursor) {
                        if (cli->state != STATE_PASSWORD &&
                            cli->state != STATE_ENABLE_PASSWORD)
                            kcli_socket_write("\b", 1);

                        cursor--;
                    }
                } else { /* Right */
                    if (cursor < l) {
                        if (cli->state != STATE_PASSWORD &&
                            cli->state != STATE_ENABLE_PASSWORD)
                            kcli_socket_write(&cmd[cursor], 1);

                        cursor++;
                    }
                }

                continue;
            }

            /* start of line */
            if (c == CTRL('A')) {
                if (cursor) {
                    if (cli->state != STATE_PASSWORD &&
                        cli->state != STATE_ENABLE_PASSWORD) {
                        kcli_socket_write("\r", 1);
                        show_prompt(cli, sockfd);
                    }

                    cursor = 0;
                }

                continue;
            }

            /* end of line */
            if (c == CTRL('E')) {
                if (cursor < l) {
                    if (cli->state != STATE_PASSWORD &&
                        cli->state != STATE_ENABLE_PASSWORD)
                        kcli_socket_write(&cmd[cursor], l - cursor);

                    cursor = l;
                }

                continue;
            }

            /* normal character typed */
            if (cursor == l) { /* append to end of line */
                cmd[cursor] = c;
                if (l < 4095) {
                    l++;
                    cursor++;
                } else {
                    kcli_socket_write("\a", 1);
                    continue;
                }
            } else { /* insert */
                int i;
                /* move everything one character to the right */
                if (l >= 4094) l--;
                for (i = l; i >= cursor; i--)
                    cmd[i + 1] = cmd[i];

                /* kcli_socket_write what we've just added */
                cmd[cursor] = c;

                kcli_socket_write(&cmd[cursor], l - cursor + 1);
                for (i = 0; i < (l - cursor + 1); i++)
                    kcli_socket_write("\b", 1);

                l++;
                cursor++;
            }

            if (cli->state != STATE_PASSWORD &&
                cli->state != STATE_ENABLE_PASSWORD) {
                if (c == '?' && cursor == l) {
                    kcli_socket_write("\r\n", 2);
                    oldcmd = cmd;
                    oldl = cursor = l - 1;
                    break;
                }
                kcli_socket_write(&c, 1);
            }

            oldcmd = 0;
            oldl = 0;
            lastchar = c;
        }

        if (l < 0)
            break;

        if (!strcmp(cmd, "quit"))
            break;

        if (cli->state == STATE_LOGIN) {
            if (l == 0)
                continue;

            /* require login */
            kfree(username);
            username = strdup(cmd);
            cli->state = STATE_PASSWORD;
            cli->showprompt = 1;
        } else if (cli->state == STATE_PASSWORD) {
            /* require password */
            int allowed = 0;

            kfree(password);
            password = strdup(cmd);
            if (cli->auth_callback && cli->auth_callback(username, password) == CLI_OK)
                allowed++;

            if (!allowed) {
                struct unp *u;
                for (u = cli->users; u; u = u->next) {
                    if (!strcmp(u->username, username) &&
                        pass_matches(u->password, password)) {
                        allowed++;
                        break;
                    }
                }
            }

            if (allowed) {
                //cli_error(cli, "");
                cli->state = STATE_NORMAL;
            } else {
                cli_error(cli, "\n\nAccess denied");
                free_z(username);
                free_z(password);
                cli->state = STATE_LOGIN;
            }

            cli->showprompt = 1;
        } else if (cli->state == STATE_ENABLE_PASSWORD) {
            int allowed = 0;
            if (cli->enable_password) {
                /* check stored static enable password */
                if (pass_matches(cli->enable_password, cmd))
                    allowed++;
            }

            if (!allowed && cli->enable_callback) {
                /* check callback */
                if (cli->enable_callback(cmd))
                    allowed++;
            }

            if (allowed) {
                cli->state = STATE_ENABLE;
                cli_set_privilege(cli, PRIVILEGE_PRIVILEGED);
            } else {
                cli_error(cli, "\n\nAccess denied");
                cli->state = STATE_NORMAL;
            }
        } else {
            if (!l)
                continue;

            if (cmd[l - 1] != '?')
                hist_sz = add_history(cli, cmd);

            if (run_command(cli, cmd) == CLI_QUIT)
                break;
        }
    }

    {
        /* cleanup */
        int i;
        for (i = 0; i < MAX_HISTORY; i++)
            free_z(cli->history[i]);

        kfree(username);
        kfree(password);
        kfree(cmd);
    }

    return CLI_OK;
}

static void do_print(struct cli_def *cli, int filter, char *format, va_list ap)
{
    char **buf = &cli->_print_buf;
    int *sz = &cli->_print_bufsz;
    char *p;
    int n;

    if (!*buf) {
        *buf = kzalloc(4095, GFP_KERNEL);
        if (*buf == NULL) {
            return;
        }
        *sz = 4095;
    }

    while ((n = vsnprintf(*buf, *sz, format, ap)) >= *sz) {
        kfree(*buf);
        if (!(*buf = kzalloc(*sz += 4096, GFP_KERNEL))) {
            *sz = 0;
            return;
        }
    }

    if (n < 0) /* vsnprintf failed */
        return;

    p = *buf;
    do {
        char *next = strchr(p, '\n');
        struct cli_filter *f = filter ? cli->filters : 0;
        int print = 1;

        if (next)
            *next++ = 0;

        while (print && f) {
            print = (f->filter(cli, p, f->data) == CLI_OK);
            f = f->next;
        }

        if (print) {
            if (cli->print_callback)
                cli->print_callback(cli, p);
            else {
                outbufsz = snprintf(outbuf, OUTBUFMAX, "%s\r\n", p);
                kcli_socket_write(outbuf, outbufsz);
            }
        }

        p = next;
    } while (p);
}

void cli_print(struct cli_def *cli, char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    do_print(cli, 1, format, ap);
    va_end(ap);
}

static void cli_error(struct cli_def *cli, char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    do_print(cli, 0, format, ap);
    va_end(ap);
}

void cli_print_callback(struct cli_def *cli,
                        void (*callback)(struct cli_def *, char *))
{
    cli->print_callback = callback;
}

EXPORT_SYMBOL(cli_register_command);
EXPORT_SYMBOL(cli_set_auth_callback);
EXPORT_SYMBOL(cli_set_enable_callback);
EXPORT_SYMBOL(cli_allow_user);
EXPORT_SYMBOL(cli_allow_enable);
EXPORT_SYMBOL(cli_deny_user);
EXPORT_SYMBOL(cli_set_banner);
EXPORT_SYMBOL(cli_set_hostname);
EXPORT_SYMBOL(cli_set_promptchar);
EXPORT_SYMBOL(cli_set_privilege);
EXPORT_SYMBOL(cli_unregister_command);
EXPORT_SYMBOL(cli_reprompt);
EXPORT_SYMBOL(cli_regular);
EXPORT_SYMBOL(cli_print_callback);
EXPORT_SYMBOL(cli_print);

