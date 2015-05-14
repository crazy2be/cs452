#define RBUF_SIZE 4096

struct RBuf { // Ring Buffer
	int i; // index offset
	int l; // length of data
	int c; // capacity of buffer
	char buf[RBUF_SIZE];
};

void rbuf_init(struct RBuf *rbuf);
void rbuf_put(struct RBuf *rbuf, char val);
char rbuf_take(struct RBuf *rbuf);
