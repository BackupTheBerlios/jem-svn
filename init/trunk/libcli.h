#ifndef __LIBCLI_H__
#define __LIBCLI_H__

#define CLI_OK			0
#define CLI_ERROR		-1
#define CLI_QUIT		-2
#define MAX_HISTORY		256

#define PRIVILEGE_UNPRIVILEGED	0
#define PRIVILEGE_PRIVILEGED	15
#define MODE_ANY		-1
#define MODE_EXEC		0
#define MODE_CONFIG		1

#define LIBCLI_HAS_ENABLE	1

#include <stdio.h>

struct cli_def {
    int completion_callback;
    struct cli_command *commands;
    int (*auth_callback)(char *, char *);
    int (*regular_callback)(struct cli_def *cli);
    int (*enable_callback)(char *);
    char *banner;
    struct unp *users;
    char *enable_password;
    char *history[MAX_HISTORY];
    char showprompt;
    char *promptchar;
    char *hostname;
    char *modestring;
    int privilege;
    int mode;
    int state;
    struct cli_filter *filters;
    void (*print_callback)(struct cli_def *cli, char *string);
    FILE *client;
    /* internal buffers */
    char *_name_buf;
    char *_print_buf;
    int _print_bufsz;
};

struct cli_filter {
    int (*filter)(struct cli_def *cli, char *string, void *data);
    void *data;
    struct cli_filter *next;
};

struct cli_command {
    char *command;
    int (*callback)(struct cli_def *, char *, char **, int);
    int unique_len;
    char *help;
    int privilege;
    int mode;
    struct cli_command *next;
    struct cli_command *children;
    struct cli_command *parent;
};

struct cli_def *cli_init(void);
int cli_done(struct cli_def *cli);
struct cli_command *cli_register_command(struct cli_def *cli,
    struct cli_command *parent, char *command,
    int (*callback)(struct cli_def *, char *, char **, int),
    int privilege, int mode, char *help);

int cli_unregister_command(struct cli_def *cli, char *command);
int cli_loop(struct cli_def *cli, int sockfd);
int cli_file(struct cli_def *cli, FILE *fh, int privilege, int mode);
void cli_set_auth_callback(struct cli_def *cli,
    int (*auth_callback)(char *, char *));

void cli_set_enable_callback(struct cli_def *cli,
    int (*enable_callback)(char *));

void cli_allow_user(struct cli_def *cli, char *username, char *password);
void cli_allow_enable(struct cli_def *cli, char *password);
void cli_deny_user(struct cli_def *cli, char *username);
void cli_set_banner(struct cli_def *cli, char *banner);
void cli_set_hostname(struct cli_def *cli, char *hostname);
void cli_set_promptchar(struct cli_def *cli, char *promptchar);
int cli_set_privilege(struct cli_def *cli, int privilege);
int cli_set_configmode(struct cli_def *cli, int mode, char *config_desc);
void cli_reprompt(struct cli_def *cli);
void cli_regular(struct cli_def *cli, int (*callback)(struct cli_def *cli));
void cli_print(struct cli_def *cli, char *format, ...)
    __attribute__((format (printf, 2, 3)));

void cli_error(struct cli_def *cli, char *format, ...)
    __attribute__((format (printf, 2, 3)));

void cli_print_callback(struct cli_def *cli,
    void (*callback)(struct cli_def *, char *));


//=================================================================================================
// This file is part of Jem, a real time Java operating system designed for embedded systems.
//
// Jem is free software; you can redistribute it and/or modify it under the terms of the GNU
// General Public License, version 2, as published by the Free Software Foundation.
//
// Jem is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with Jem; if not,
// write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA
//
// As a special exception, if other files instantiate templates or use macros or inline functions
// from this file, or you compile this file and link it with other works to produce a work based
// on this file, this file does not by itself cause the resulting work to be covered by the GNU
// General Public License. However the source code for this file must still be made available in
// accordance with section (3) of the GNU General Public License.
//
// This exception does not invalidate any other reasons why a work based on this file might be
// covered by the GNU General Public License.
//
// Alternative licenses for Jem may be arranged by contacting Sombrio Systems Inc. at 
// http://www.sombrio.com
//=================================================================================================

#endif

