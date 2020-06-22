#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "qbuf.h"

struct qbuf *qbuf_alloc()
{
	struct qbuf *qbuf = malloc(sizeof(struct qbuf));
	if (qbuf == NULL)
		return NULL;

	qbuf_clear(qbuf);
	return qbuf;
}

void qbuf_free(struct qbuf *qbuf)
{
	free(qbuf);
}

void qbuf_clear(struct qbuf *qbuf)
{
	qbuf->start = qbuf->buffer;
	qbuf->end = qbuf->buffer;
}

unsigned int qbuf_len(struct qbuf *qbuf)
{
	return qbuf->end - qbuf->start;
}

static unsigned int qbuf_avail_front(struct qbuf *qbuf)
{
	return qbuf->start - qbuf->buffer;
}

unsigned int qbuf_avail(struct qbuf *qbuf)
{
	return qbuf->buffer + sizeof(qbuf->buffer) - qbuf->end;
}

static void __qbuf_memmove(struct qbuf *qbuf)
{
	/* Can't do nothing */
	if (qbuf->start == qbuf->buffer)
		return;

	/* Make space for new data */
	unsigned int size = qbuf_len(qbuf);
	memmove(qbuf->buffer, qbuf->start, size);
	qbuf->start = qbuf->buffer;
	qbuf->end = qbuf->buffer + size;
}

void qbuf_reorganize(struct qbuf *qbuf)
{
	/* All consumed (clear) */
	if (qbuf->start == qbuf->end) {
		qbuf_clear(qbuf);
		return;
	}

	/* No need for reorganization this time */
	if (qbuf_avail(qbuf) >= qbuf_avail_front(qbuf))
		return;

	/* Make space for new data */
	__qbuf_memmove(qbuf);
}

void qbuf_use(struct qbuf *qbuf, unsigned int bytes)
{
	qbuf->end += bytes;
}

void qbuf_unuse(struct qbuf *qbuf, unsigned int bytes)
{
	qbuf->start += bytes;
}

int qbuf_make_space(struct qbuf *qbuf, unsigned int bytes)
{
	if (qbuf_avail(qbuf) < bytes) {

		/* Make all the space possible */
		__qbuf_memmove(qbuf);

		/* Check again */
		if (qbuf_avail(qbuf) < bytes)
			return -1;
	}

	return 0;
}
