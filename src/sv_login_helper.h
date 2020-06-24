#ifndef SV_LOGIN_HELPER_H
#define SV_LOGIN_HELPER_H

#include <sys/types.h>
#include <stdbool.h>
#include "qbuf.h"

struct login_helper {
	/* Arbitary user data (usually the client_t struct) */
	void *userdata;

	/* Used for communication with the helper program */
	int stdin, stdout;

	/* PID of the helper program. (unused) SIGCHILD is ignored by mvdsv */
	pid_t pid;

	/* Wait pid. (unused) SIGCHILD is ignored by mvdsv */
	bool wait_pid;

	/* Receive buffer */
	struct qbuf recvbuf;

	/* Send buffer */
	struct qbuf sendbuf;

	/* All those callbacks *must* be set by the MVDSV server */

	/* Will reply with the full userinfo string */
	int (*userinfo_handler)(struct login_helper *helper);

	/* Must add/modify the client's userinfo string */
	int (*setinfo_handler)(struct login_helper *helper, const char *uinfo);

	/* Will print on the client's screen PRINT_HIGH */
	int (*print_handler)(struct login_helper *helper, const char *msg);

	/* Message will be broadcast to all the server clients */
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

	/* This returns whether the authentication was successful
	 * If the auth fails MVDSV will disconnect the user and close pipe
	 * with the helper program */
	int (*login_handler)(struct login_helper *helper, bool auth_success);
};

struct login_helper *login_helper_new(char *program);
int login_helper_check_fds(struct login_helper *helper);
int login_helper_write(struct login_helper *helper,
	const char *opcode, const char *data);
void login_helper_free(struct login_helper *helper);

#endif /* SV_LOGIN_HELPER_H */
