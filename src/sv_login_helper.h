/*
  Copyright (C) 2020 Luiz A. Bühnemann

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef SV_LOGIN_HELPER_H
#define SV_LOGIN_HELPER_H

#include <sys/types.h>
#include <stdbool.h>
#include "qbuf.h"

/* Those are the supported opcodes */
#define LOGIN_HELPER_OPCODE_SERVERINFO "SINFO" /* Request the serverinfo string */
#define LOGIN_HELPER_OPCODE_USERINFO "UINFO" /* Request userinfo string */
#define LOGIN_HELPER_OPCODE_SETAUTH "SAUTH" /* Set *auth star key on client */
#define LOGIN_HELPER_OPCODE_PRINT "PRINT" /* High priority print PRINT_HIGH */
#define LOGIN_HELPER_OPCODE_CENTERPRINT "CPRNT" /* Print on the center client's screen */
#define LOGIN_HELPER_OPCODE_BROADCAST "BCAST" /* Broadcast message to all users */
#define LOGIN_HELPER_OPCODE_SERVER_COMMAND "SVCMD" /* Issues a command on the server */
#define LOGIN_HELPER_OPCODE_CLIENT_COMMAND "CLCMD" /* Issues a command on the client */
#define LOGIN_HELPER_OPCODE_INPUT "INPUT" /* Requests input from the user */
#define LOGIN_HELPER_OPCODE_LOGIN "LOGIN" /* Allows user to join the server */
#define LOGIN_HELPER_OPCODE_END_OF_CMD "EOCMD" /* Marks the end of output from
						* a command execution on
						* the server (SVCMD) */

struct login_helper {
	/* Arbitary user data (usually the client_t struct) */
	void *userdata;

	/* Used for communication with the helper program */
	int stdin, stdout;

	/* PID of the helper program. (unused) SIGCHLD is ignored by mvdsv */
	pid_t pid;

	/* Receive buffer */
	struct qbuf recvbuf;

	/* Send buffer */
	struct qbuf sendbuf;

	/* All those callbacks *must* be set by the MVDSV server */

	/* Will reply with the serverinfo string */
	int (*serverinfo_handler)(struct login_helper *helper);

	/* Will reply with the full userinfo string */
	int (*userinfo_handler)(struct login_helper *helper);

	/* Must add/modify the client's userinfo string */
	int (*setauth_handler)(struct login_helper *helper, const char *auth);

	/* Will print on the client's screen PRINT_HIGH */
	int (*print_handler)(struct login_helper *helper, const char *msg);

	/* Will print on the center client's screen */
	int (*centerprint_handler)(struct login_helper *helper, const char *msg);

	/* Message will be broadcast to all the server's clients */
	int (*broadcast_handler)(struct login_helper *helper, const char *msg);

	/* Will prompt the player for input */
	int (*input_handler)(struct login_helper *helper);

	/* Must execute any command received by this callback on the
	 * server console */
	int (*server_command_handler)(struct login_helper *helper,
		const char *cmd);

	/* Must execute any command received by this callback on the
	 * client's console */
	int (*client_command_handler)(struct login_helper *helper,
		const char *cmd);

	/* That will make the user join the server */
	int (*login_handler)(struct login_helper *helper);
};

struct login_helper *login_helper_new(char *program);
int login_helper_check_fds(struct login_helper *helper);
int login_helper_write(struct login_helper *helper,
	const char *opcode, const char *data);
void login_helper_free(struct login_helper *helper);

#endif /* SV_LOGIN_HELPER_H */
