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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <paths.h>
#include <errno.h>
#include <inttypes.h>
#include "sv_login_helper.h"

extern char **environ;

/* All opcodes have 5 bytes */
#define LOGIN_HELPER_OPCODE_SIZE 5

static void close_pipe(int *fds)
{
	close(fds[0]);
	close(fds[1]);
}

static int __set_nonblocking(int fd)
{
	int flags;

	/* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
	/* FIXME: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	/* Otherwise, use the old way of doing it */
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
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

	/* Set fds all to non-blocking mode */
	__set_nonblocking(*stdin);
	__set_nonblocking(*stdout);

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

/* NOTE: Only helper 2 server messages are parsed here */
static int login_helper_parse(struct login_helper *helper, const char *data,
	uint32_t size)
{
	/* Userinfo request */
	if (!memcmp(data, LOGIN_HELPER_OPCODE_USERINFO,
			LOGIN_HELPER_OPCODE_SIZE)) {
		return helper->userinfo_handler(helper);
	}

	/* Serverinfo request */
	else if (!memcmp(data, LOGIN_HELPER_OPCODE_SERVERINFO,
			LOGIN_HELPER_OPCODE_SIZE)) {
		return helper->serverinfo_handler(helper);
	}

	/* Set auth request */
	else if (!memcmp(data, LOGIN_HELPER_OPCODE_SETAUTH,
			LOGIN_HELPER_OPCODE_SIZE)) {
		return helper->setauth_handler(helper,
			__to_c_string(data + 5, size - 5));
	}

	/* Print message request */
	else if (!memcmp(data, LOGIN_HELPER_OPCODE_PRINT,
			LOGIN_HELPER_OPCODE_SIZE)) {
		return helper->print_handler(helper,
			__to_c_string(data + 5, size - 5));
	}

	/* CenterPrint message request */
	else if (!memcmp(data, LOGIN_HELPER_OPCODE_CENTERPRINT,
			LOGIN_HELPER_OPCODE_SIZE)) {
		return helper->centerprint_handler(helper,
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
		return helper->login_handler(helper);

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
	qbuf_mark_used(qbuf, status);

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
		qbuf_mark_unused(qbuf, size + 4);

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
	qbuf_mark_unused(sendbuf, status);
	return 0;
}

int login_helper_check_fds(struct login_helper *helper)
{
	/* Send any buffered data */
	if (qbuf_len(&helper->sendbuf) > 0) {
		int status = login_helper_send(helper);
		if (status)
			return status;
	}

	/* Read buffered */
	return login_helper_receive(helper);
}

int login_helper_write(struct login_helper *helper,
	const char *opcode, const char *data)
{
	int opcode_size = strlen(opcode);
	/* Invalid opcode */
	if (opcode_size != LOGIN_HELPER_OPCODE_SIZE)
		return -1;

	int data_size = strlen(data != NULL ? data : "");

	if (data_size > 0) {

		/* Strip line break at end of string, we don't want the
		 * helper needing to parse those line breaks
		 * at the end of the line */
		if (data[data_size-1] == '\n')
			data_size--;
	}

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
	qbuf_mark_used(sendbuf, needed_space);

	/* Success */
	return login_helper_send(helper);
}
