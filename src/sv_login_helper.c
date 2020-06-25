#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <paths.h>
#include <errno.h>
#include <inttypes.h>
#include "sv_login_helper.h"

extern char **environ;

/* All opcodes have 5 bytes */
#define LOGIN_HELPER_OPCODE_SIZE 5

/* Those are the supported opcodes */
#define LOGIN_HELPER_OPCODE_USERINFO "UINFO"
#define LOGIN_HELPER_OPCODE_SETINFO "SINFO"
#define LOGIN_HELPER_OPCODE_PRINT "PRINT"
#define LOGIN_HELPER_OPCODE_BROADCAST "BCAST"
#define LOGIN_HELPER_OPCODE_SERVER_COMMAND "SVCMD"
#define LOGIN_HELPER_OPCODE_CLIENT_COMMAND "CLCMD"
#define LOGIN_HELPER_OPCODE_INPUT "INPUT"
#define LOGIN_HELPER_OPCODE_LOGIN "LOGIN"

static void close_pipe(int *fds)
{
	close(fds[0]);
	close(fds[1]);
}

static pid_t login_helper_launch(char *program, int *stdin, int *stdout)
{
	int stdin_pipe_fd[2];
	int stdout_pipe_fd[2];
	int status;
	pid_t pid;
	char *argp[] = { "sh", "-c", program, NULL };

	/* Open STDIN pipe */
	status = pipe(stdin_pipe_fd);
	if (status < 0)
		return -1;

	/* Open STDOUT pipe */
	status = pipe(stdout_pipe_fd);
	if (status < 0) {
		close_pipe(stdin_pipe_fd);
		return -1;
	}

	/* Fork program */
	pid = fork();
	if (pid == -1) {
		close_pipe(stdin_pipe_fd);
		close_pipe(stdout_pipe_fd);
		return -1;
	}

	/* Child */
	if (pid == 0) {
		close(stdin_pipe_fd[1]);
		close(stdout_pipe_fd[0]);
		dup2(stdin_pipe_fd[0], STDIN_FILENO);
		dup2(stdout_pipe_fd[1], STDOUT_FILENO);
		close(stdin_pipe_fd[0]);
		close(stdin_pipe_fd[1]);

		execve(_PATH_BSHELL, argp, environ);
		_exit(127);
		/* NEVER REACHED */
	}

	/* Parent */
	close(stdin_pipe_fd[0]);
	close(stdout_pipe_fd[1]);
	*stdin = stdin_pipe_fd[1];
	*stdout = stdout_pipe_fd[0];

	/* Child process pid */
	return pid;
}

struct login_helper *login_helper_new(char *program)
{
	int stdin, stdout;
	pid_t pid;

	/* Launch helper */
	pid = login_helper_launch(program, &stdin, &stdout);
	if (pid == -1)
		return NULL;

	/* Create helper instance */
	struct login_helper *helper = calloc(1, sizeof(struct login_helper));
	if (helper == NULL) {
		close(stdin);
		close(stdout);
		return NULL;
	}

	helper->stdin = stdin;
	helper->stdout = stdout;
	helper->pid = pid;
	qbuf_clear(&helper->recvbuf);
	qbuf_clear(&helper->sendbuf);
	return helper;
}

void login_helper_free(struct login_helper *helper)
{
	close(helper->stdout);
	close(helper->stdin);
	free(helper);
}

static const char *__to_c_string(const char *data, int size)
{
	static char str[BUFSIZ];

	if (size > sizeof(str)-1)
		return "<STRING OVERFLOW>";

	memcpy(str, data, size);
	str[size] = '\0';
	return str;
}

static int login_helper_parse(struct login_helper *helper, const char *data,
	uint32_t size)
{
	/* Userinfo request */
	if (!memcmp(data, LOGIN_HELPER_OPCODE_USERINFO,
			LOGIN_HELPER_OPCODE_SIZE)) {
		return helper->userinfo_handler(helper);
	}

	/* Setinfo request */
	else if (!memcmp(data, LOGIN_HELPER_OPCODE_SETINFO,
			LOGIN_HELPER_OPCODE_SIZE)) {
		return helper->setinfo_handler(helper,
			__to_c_string(data + 5, size - 5));
	}

	/* Print message request */
	else if (!memcmp(data, LOGIN_HELPER_OPCODE_PRINT,
			LOGIN_HELPER_OPCODE_SIZE)) {
		return helper->print_handler(helper,
			__to_c_string(data + 5, size - 5));
	}

	/* Broadcast message request */
	else if (!memcmp(data, LOGIN_HELPER_OPCODE_BROADCAST,
			LOGIN_HELPER_OPCODE_SIZE)) {
		return helper->broadcast_handler(helper,
			__to_c_string(data + 5, size - 5));
	}

	/* Input request */
	else if (!memcmp(data, LOGIN_HELPER_OPCODE_INPUT,
			LOGIN_HELPER_OPCODE_SIZE)) {
		return helper->input_handler(helper);
	}

	/* Server command request */
	else if (!memcmp(data, LOGIN_HELPER_OPCODE_SERVER_COMMAND,
			LOGIN_HELPER_OPCODE_SIZE)) {
		return helper->server_command_handler(helper,
			__to_c_string(data + 5, size - 5));
	}

	/* Client command request */
	else if (!memcmp(data, LOGIN_HELPER_OPCODE_CLIENT_COMMAND,
			LOGIN_HELPER_OPCODE_SIZE)) {
		return helper->client_command_handler(helper,
			__to_c_string(data + 5, size - 5));
	}

	/* Login result */
	else if (!memcmp(data, LOGIN_HELPER_OPCODE_LOGIN,
			LOGIN_HELPER_OPCODE_SIZE)) {
		const char *buf = data + 5;

		/* Authentication successful */
		if (!memcmp(buf, "success", sizeof("success")-1)
			|| !memcmp(buf, "SUCCESS", sizeof("SUCCESS")-1))
			return helper->login_handler(helper, true);

		/* Authentication failed */
		else
			return helper->login_handler(helper, false);

	}

	/* Unknown opcode */
	return -1;
}

static int login_helper_receive(struct login_helper *helper)
{
	struct qbuf *qbuf = &helper->recvbuf;
	unsigned int avail = qbuf_avail(qbuf);

	/* Overflowed */
	if (avail == 0)
		return -1;

	/* Read data from helper */
	int status = read(helper->stdout, qbuf->end, avail);

	if (status == -1) {
		/* Nothing on read window or interrupt */
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return 0;

		/* Error */
		return -1;
	}

	/* EOF from helper */
	if (status == 0)
		return 1;

	/* Increment used bytes */
	qbuf_use(qbuf, status);

	/* Parse data from helper */
	unsigned int remaining_len = qbuf_len(qbuf);
	while (remaining_len > 4) {

		/* Parse packet header */
		uint32_t size = *(uint32_t *)qbuf->start;
		char *data = (char *)qbuf->start + 4;

		/* Not enough data received yet */
		if (size > (remaining_len - 4))
			break;

		/* Parse data */
		status = login_helper_parse(helper, data, size);
		if (status)
			return -1;

		/* Mark bytes unused */
		qbuf_unuse(qbuf, size + 4);

		/* Update remaining len */
		remaining_len = qbuf_len(qbuf);

	}

	/* Reorganize receive buffer */
	qbuf_reorganize(qbuf);
	return 0;
}

static int login_helper_send(struct login_helper *helper)
{
	struct qbuf *sendbuf = &helper->sendbuf;
	int status = write(helper->stdin, sendbuf->start, qbuf_len(sendbuf));

	if (status == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return 0;
		return -1;
	}

	/* Mark unused */
	qbuf_unuse(sendbuf, status);
	return 0;
}

static inline int __max_fd(int a, int b)
{
	return (a > b) ? a : b;
}

int login_helper_check_fds(struct login_helper *helper)
{
	/* Setup write fds if we have buffered data */
	struct qbuf *sendbuf = &helper->sendbuf;
	unsigned int sendbuf_len = qbuf_len(sendbuf);
	fd_set wfds;
	FD_ZERO(&wfds);
	if (sendbuf_len > 0)
		FD_SET(helper->stdin, &wfds);

	/* Setup read fds */
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(helper->stdout, &rfds);

	static struct timeval tv = { 0 };
	int status = select(__max_fd(helper->stdin, helper->stdout) + 1,
		&rfds, &wfds, NULL, &tv);

	/* select() error */
	if (status == -1) {

		/* Was just a system interrupt */
		if (errno == EINTR)
			return 0;

		return -1;
	}

	/* Nothing to be done */
	if (status == 0)
		return 0;

	/* Send buffered data */
	if (FD_ISSET(helper->stdin, &wfds)) {
		status = login_helper_send(helper);
		if (status == -1)
			return -1;
	}

	/* Read buffered */
	if (FD_ISSET(helper->stdout, &rfds)) {
		status = login_helper_receive(helper);
		if (status)
			return status;
	}

	return 0;
}

int login_helper_write(struct login_helper *helper,
	const char *opcode, const char *data)
{
	int opcode_size = strlen(opcode);
	/* Invalid opcode */
	if (opcode_size != LOGIN_HELPER_OPCODE_SIZE)
		return -1;

	int data_size = strlen(data);

	struct qbuf *sendbuf = &helper->sendbuf;
	unsigned int needed_space = 4 + opcode_size + data_size;

	/* Try reorganizing the buffer to accomodate data */
	if (qbuf_make_space(sendbuf, needed_space))

		/* Not enough space on buffer (overflow) */
		return -1;

	/* Write packet */
	uint32_t *size = (uint32_t *)sendbuf->end;
	*size = opcode_size + data_size;
	memcpy(sendbuf->end + 4, opcode, opcode_size);
	memcpy(sendbuf->end + 4 + opcode_size, data, data_size);

	/* Mark bytes used */
	qbuf_use(sendbuf, needed_space);

	/* Success */
	return login_helper_send(helper);
}
