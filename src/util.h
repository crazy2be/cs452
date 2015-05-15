void assert(int musttrue, const char *msg);
void sleep(int n);
void *memset(void *ptr, int value, int num);
int modi(int a, int b);

#define COM1 0
#define COM2 1
int fprintf(int channel, const char *format, ...);
#define bwprintf(channel, ...) do { \
	fprintf(channel, __VA_ARGS__); io_flush(channel); } while (0)
#define printf(...) fprintf(COM2, __VA_ARGS__)
