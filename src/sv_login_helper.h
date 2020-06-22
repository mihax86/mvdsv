#ifndef SV_LOGIN_HELPER_H
#define SV_LOGIN_HELPER_H

#include <sys/types.h>
#include <stdbool.h>
#include "qbuf.h"

struct login_helper {
	/* Arbitary user data */
	void *userdata;

	/* Used for communication with the helper program */
	int stdin, stdout;

	/* PID of the helper program */
	pid_t pid;

	/* Wait pid */
	bool wait_pid;

	/* Receive buffer */
	struct qbuf recvbuf;

	/* Send buffer */
	struct qbuf sendbuf;

	/* Those callbacks *must* be set by the MVDSV server: */
	/* Must reply with the full userinfo string */
	void (*userinfo_handler)(struct login_helper *helper);

	/* Must print on the player screen PRINT_HIGH */
	void (*print_handler)(struct login_helper *helper, const char *msg);

	/* Must execute any command received by this callback on the server */
	void (*command_handler)(struct login_helper *helper, const char *cmd);

	/* Must prompt the player for input */
	void (*input_handler)(struct login_helper *helper);

	/* This returns whether the authentication was successful */
	void (*login_handler)(struct login_helper *helper, bool auth_success);
};

struct login_helper *login_helper_new(char *program);
int login_helper_check_fds(struct login_helper *helper);
int login_helper_write(struct login_helper *helper,
	const char *opcode, const char *data);
void login_helper_free(struct login_helper *helper);

#endif /* SV_LOGIN_HELPER_H */
