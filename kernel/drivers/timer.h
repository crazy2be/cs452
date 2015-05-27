#if QEMU
#define TIME_SECOND (1000000/256) // 1Mhz / 256
#else
//#define TIME_SECOND 2000 // 2kHz
#define TIME_SECOND 508000 // 508kHz
#endif

void timer_init();
void timer_service();
long long timer_time();
int time_hours(long long time);
int time_minutes(long long time);
int time_seconds(long long time);
int time_useconds(long long time);
int time_fraction(long long time);
