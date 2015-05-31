#include "timer.h"

int time_hours(long long time) {
	return time / (TIME_SECOND * 60 * 60) % 24;
}
int time_minutes(long long time) {
	return time / (TIME_SECOND * 60) % 60;
}
int time_seconds(long long time) {
	return time / TIME_SECOND % 60;
}
int time_useconds(long long time) {
	return (time*1000000) / TIME_SECOND % 1000000;
}
int time_fraction(long long time) {
	return time % TIME_SECOND;
}
