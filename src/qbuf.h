#ifndef QBUF_H
#define QBUF_H

struct qbuf {
	unsigned char buffer[8192];
	unsigned char *start;
	unsigned char *end;
};

struct qbuf *qbuf_alloc();
void qbuf_free(struct qbuf *qbuf);
void qbuf_clear(struct qbuf *qbuf);
unsigned int qbuf_len(struct qbuf *qbuf);
unsigned int qbuf_avail(struct qbuf *qbuf);
void qbuf_unuse(struct qbuf *qbuf, unsigned int bytes);
void qbuf_use(struct qbuf *qbuf, unsigned int bytes);
void qbuf_reorganize(struct qbuf *qbuf);
int qbuf_make_space(struct qbuf *qbuf, unsigned int bytes);

#endif /* QBUF_H */
