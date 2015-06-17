#include "displaysrv.h"
#include "signal.h"
#include "nameserver.h"
#include "util.h"
#include "clockserver.h"

#include <assert.h>
#include <kernel.h>

#define TRACK_DISPLAY_WIDTH 58
#define TRACK_DISPLAY_HEIGHT 25

// for now, we display most of the track from precalculated text
// this will need to change if we want to make parts of the train blink
// to representa a train's position

static const char track_repr[TRACK_DISPLAY_HEIGHT][TRACK_DISPLAY_WIDTH] = {
"    ___________________________________________________  ",
"             /                \\        \\                 ",
"            /                  \\        \\                ",
"        ___/____________________\\___     \\_____________  ",
"       /                            \\     \\              ",
"      /                              \\     \\             ",
"     /    _________________________   \\     \\__________  ",
"    /    /       \\         /       \\   \\     \\           ",
"    |   /         \\   |   /         \\   |     \\          ",
"    |  /           \\  |  /           \\  |      \\         ",
"    | /             \\ | /             \\ |       |        ",
"    |/               \\|/               \\|       |        ",
"    |                 |                 |       |        ",
"    |                 |                 |       |        ",
"    |                 |                 |       |        ",
"    |                 |                 |       |        ",
"    |                 |                 |       |        ",
"    |\\               /|\\               /|       |        ",
"    | \\             / | \\             / |       |        ",
"    |  \\           /  |  \\           /  |      /         ",
"    |   \\         /   |   \\         /   |     /          ",
"    |    \\_______/_________\\_______/   /     /_________  ",
"     \\                                /     /            ",
"      \\                              /     /             ",
"       \\____________________________/_____/____________  "
};

// this doesn't handle the 3-way switches in the middle
struct sw2_display {
	unsigned char sx, sy;
	char sr;
	unsigned char cx, cy;
	char cr;
};
struct sw3_display {
	unsigned char lx, ly, sx, sy, rx, ry;
};

// TODO: handle 3-way switches

static const struct sw2_display switch_display_info[] = {
	{ 45, 7,  '\\', 45, 6,  '_'  }, // 1
	{ 42, 4,  '\\', 42, 3,  '_'  }, // 2
	{ 39, 0,  '_' , 39, 1,  '\\' }, // 3
	{ 45, 21, '/',  46, 21, '_'  }, // 4
	{ 13, 0,  '_',  13, 1,  '/'  }, // 5
	{ 32, 3,  '_',  32, 3,  '\\' }, // 6
	{ 11, 3,  '_',  11, 3,  '/'  }, // 7
	{ 4,  11, '|',  5,  11, '/'  }, // 8
	{ 4,  17, '|',  5,  17, '\\' }, // 9
	{ 17, 21, '_',  17, 21, '/'  }, // 10
	{ 36, 24, '_',  36, 24, '/'  }, // 11
	{ 42, 24, '_',  42, 24, '/'  }, // 12
	{ 27, 21, '_',  27, 21, '\\' }, // 13
	{ 40, 17, '|',  39, 17, '/'  }, // 14
	{ 40, 11, '|',  39, 11, '\\' }, // 15
	{ 27, 6,  '_',  27, 7,  '/'  }, // 16
	{ 17, 6,  '_',  17, 7,  '\\' }, // 17
	{ 30, 0,  '_',  30, 1,  '\\' }, // 18
};

struct sensor_display {
	// coordinates of first letter of sensor id on the map
	unsigned char x, y;
};

static const struct sensor_display sensor_display_info[] = {
	{ 44, 25 }, // A1 & A2
	{ 35, 18 }, // A3 & A4
	{ 42, 1  }, // A5 & A6
	{ 44, 4  }, // A7 & A8
	{ 47, 7  }, // A9 & A10
	{ 50, 10 }, // A11 & A12
	{ 47, 22 }, // A13 & A14
	{ 50, 18 }, // A15 & A15
	{ 24, 7  }, // B1 & B2
	{ 28, 8  }, // B3 & B4
	{ 24, 22 }, // B5 & B6
	{ 51, 7  }, // B7 & B8
	{ 51, 1  }, // B9 & B10
	{ 51, 4  }, // B11 & B12
	{ 16, 10 }, // B13 & B14
	{ 34, 10 }, // B15 & B15
	{ 26, 10 },
	{ 10, 1  },
	{ 33, 4  },
	{ 32, 1  },
	{ 33, 8  },
	{ 32, 20 },
	{ 32, 25 },
	{ 27, 4  },
	{ 17, 18 },
	{ 19, 22 },
	{ 8,  18 },
	{ 3,  23 },
	{ 1,  8  },
	{ 14, 4  },
	{ 19, 7  },
	{ 14, 8  },
	{ 26, 18 },
	{ 15, 20 },
	{ 10, 20 },
	{ 10, 25 },
	{ 8,  10 },
	{ 4,  3  },
	{ 10, 8  },
	{ 28, 20 },
};

#define HLINE "\xe2\x94\x80"
#define VLINE "\xe2\x94\x82"
#define ULCORNER "\xe2\x94\x8c"
#define URCORNER "\xe2\x94\x90"
#define BLCORNER "\xe2\x94\x94"
#define BRCORNER "\xe2\x94\x98"
#define LTEE "\xe2\x94\x9c"
#define RTEE "\xe2\x94\xa4"
#define DTEE "\xe2\x94\xac"
#define UTEE "\xe2\x94\xb4"
#define CROSS "\xe2\x94\xbc"

#define SCREEN_WIDTH 80
#define TRAIN_X_OFFSET (1 + 1)
#define TRAIN_Y_OFFSET (1 + 1)
#define RIGHT_BAR_X_OFFSET (TRAIN_X_OFFSET + TRACK_DISPLAY_WIDTH - 3)
#define CLOCK_X_OFFSET (TRAIN_X_OFFSET + TRACK_DISPLAY_WIDTH)
#define CLOCK_Y_OFFSET TRAIN_Y_OFFSET
#define FEEDBACK_X_OFFSET TRAIN_X_OFFSET
#define FEEDBACK_Y_OFFSET (1 + TRACK_DISPLAY_HEIGHT + 1 + 2)
#define CONSOLE_X_OFFSET TRAIN_X_OFFSET
#define CONSOLE_Y_OFFSET (FEEDBACK_Y_OFFSET + 2)

static inline void reset_console_cursor(void) {
	printf("\e[%d;%dH", CONSOLE_Y_OFFSET, CONSOLE_X_OFFSET);
}

static inline void hline(int l, const char *left, const char *right) {
	puts(left);
	for (int i = 0; i < l - 2; i++) puts(HLINE);
	puts(right);
}

static inline void vbox(int h, int w, int xoffset) {
	for (int i = 0; i < h; i++) {
		if (xoffset != 0) {
			printf("\e[%dC" VLINE "\e[%dC" VLINE EOL, xoffset, w);
		} else {
			printf(VLINE "\e[%dC" VLINE EOL, w);
		}
	}
}

static void clear_line(int line) {
	char buf[SCREEN_WIDTH - 2 + 1];
	memset(buf, ' ', sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	printf("\e[%d;%dH%s\e[%d;%dH", line, CONSOLE_X_OFFSET, buf, line, CONSOLE_X_OFFSET);
}

static void initial_draw(void) {
	puts("\e[2J\e[;H");
	hline(TRACK_DISPLAY_WIDTH, ULCORNER, DTEE);
	hline(SCREEN_WIDTH - TRACK_DISPLAY_WIDTH, HLINE, URCORNER);
	puts(EOL);

	// print out the starting track
	for (int i = 0; i < TRACK_DISPLAY_HEIGHT; i++) {
		printf(VLINE "%s                     " VLINE EOL, track_repr[i]);
	}
	vbox(1, SCREEN_WIDTH - 2, 0);

	// reset the cursor to where the track is, then print out the right menu
	printf("\e[%d;%dH", 2, 1);
	vbox(1, SCREEN_WIDTH - 2 - TRACK_DISPLAY_WIDTH + 1, RIGHT_BAR_X_OFFSET);
	printf("\e[%dC", RIGHT_BAR_X_OFFSET);
	hline(SCREEN_WIDTH - TRACK_DISPLAY_WIDTH + 1, LTEE, RTEE);
	vbox(TRACK_DISPLAY_HEIGHT, SCREEN_WIDTH - 2 - TRACK_DISPLAY_WIDTH + 1,
		RIGHT_BAR_X_OFFSET);

	hline(TRACK_DISPLAY_WIDTH, LTEE, UTEE);
	hline(SCREEN_WIDTH - TRACK_DISPLAY_WIDTH, HLINE, RTEE);
	puts(EOL);

	vbox(1, SCREEN_WIDTH - 2, 0);
	hline(SCREEN_WIDTH, LTEE, RTEE);
	puts(EOL);
	vbox(1, SCREEN_WIDTH - 2, 0);
	hline(SCREEN_WIDTH, BLCORNER, BRCORNER);
	puts(EOL);

	reset_console_cursor();
}

#define MAX_FEEDBACK_LEN 80
enum displaysrv_req_type { UPDATE_SWITCH, UPDATE_SENSOR, UPDATE_TIME, CONSOLE_INPUT,
	CONSOLE_BACKSPACE, CONSOLE_CLEAR, CONSOLE_FEEDBACK, QUIT };
struct displaysrv_req {
	enum displaysrv_req_type type;
	// the data associated with each request
	union {
		struct {
			unsigned char num;
			enum sw_direction pos;
		} sw;
		struct {
			struct sensor_state state;
		} sensor;
		struct {
			char input;
		} console_input;
		// nothing for console clear, backspace, or quit
		struct {
			char feedback[MAX_FEEDBACK_LEN + 1]; // null terminated string
		} feedback;
		struct {
			unsigned millis;
		} time;
	} data;
};

static void update_sensor(struct sensor_state *sensors, struct sensor_state *old_sensors) {
	char buf[4];
	for (int i = 0; i < SENSOR_COUNT; i += 2) {
		int s1 = sensor_get(sensors, i);
		int s2 = sensor_get(sensors, i + 1);
		if (s1 != sensor_get(old_sensors, i) || s2 != sensor_get(old_sensors, i + 1)) {
			struct sensor_display coords = sensor_display_info[i / 2];
			if (s1 || s2) {
				int to_draw = s1 ? i : i + 1;
				sensor_repr(to_draw, buf);
				if (!buf[2]) {
					buf[2] = ' ';
					buf[3] = '\0';
				}
			} else {
				// clear sensor from display
				buf[0] = buf[1] = buf[2] = ' ';
				buf[3] = '\0';
			}

			printf("\e[s\e[%d;%dH%s\e[u", coords.y + TRAIN_Y_OFFSET,
					coords.x + TRAIN_X_OFFSET, buf);
			/* printf("Saw sensor %s change to %d" EOL, buf, status); */
		}
	}
	*old_sensors = *sensors;
}

static void update_switch(int sw, enum sw_direction pos) {
	if (sw >= 1 && sw <= 18) {
		ASSERT(pos == STRAIGHT || pos == CURVED);
		struct sw2_display disp = switch_display_info[sw - 1];

		// 2-way case
		unsigned char current_x, current_y, last_x, last_y;
		char switch_char;
		char filler_char = ' ';
		if (pos == STRAIGHT) {
			current_x = disp.sx;
			current_y = disp.sy;
			last_x = disp.cx;
			last_y = disp.cy;
			switch_char = disp.sr;
		} else {
			current_x = disp.cx;
			current_y = disp.cy;
			last_x = disp.sx;
			last_y = disp.sy;
			switch_char = disp.cr;
		}
		if (current_x == last_x && current_y == last_y) {
			ASSERT(disp.cr == '/' || disp.cr == '\\');
			// special case for switches which go down from _/ __ -> _____
			// we need to write slightly different characters
			filler_char = (pos == STRAIGHT) ? '_' : ' ';
			last_x += (disp.cr == '/') ? 1 : -1;
		}
		printf("\e[s\e[%d;%dH\e[1;31m%c\e[0m\e[%d;%dH%c\e[u",
			current_y + TRAIN_Y_OFFSET, current_x + TRAIN_X_OFFSET, switch_char,
			last_y + TRAIN_Y_OFFSET, last_x + TRAIN_X_OFFSET, filler_char);
	} else {
		//ASSERT(0 && "Unknown switch number");
	}
}

static void update_time(unsigned millis) {
	int tenths = (millis % 1000) / 100;
	int seconds = millis / 1000;
	int minutes = seconds / 60;

	seconds %= 60;
	minutes %= 60;

	printf("\e[s\e[%d;%dH%02d:%02d:%d\e[u", CLOCK_Y_OFFSET, CLOCK_X_OFFSET, minutes, seconds, tenths);
}

static void console_input(char c) {
	putc(c);
}

static void console_backspace(void) {
	puts("\b \b");
}

static void console_feedback(char *fb) {
	clear_line(FEEDBACK_Y_OFFSET);
	puts(fb);
	reset_console_cursor();
}

static void console_clear(void) {
	clear_line(CONSOLE_Y_OFFSET);
}

static void displaysrv_update_time(int displaysrv, unsigned millis);

static void clock_update_task(void) {
	int ticks = 0;
	int displaysrv = parent_tid();
	for (;;) {
		ticks = delay_until(ticks + 10);
		displaysrv_update_time(displaysrv, ticks * 10);
	}
}

void displaysrv_start(void) {
	register_as(DISPLAYSRV_NAME);
	create(HIGHER(PRIORITY_MIN, 2), clock_update_task);
	signal_recv();

	initial_draw();

	struct sensor_state old_sensors;
	memset(&old_sensors, 0, sizeof(old_sensors));
	struct displaysrv_req req;
	int tid;

	for (;;) {
		ASSERT(receive(&tid, &req, sizeof(req)) > 0);
		// requests are fire and forget, and provide no feedback
		ASSERT(reply(tid, NULL, 0) == REPLY_SUCCESSFUL);

		switch (req.type) {
		case UPDATE_SWITCH:
			update_switch(req.data.sw.num, req.data.sw.pos);
			break;
		case UPDATE_SENSOR:
			update_sensor(&req.data.sensor.state, &old_sensors);
			break;
		case UPDATE_TIME:
			update_time(req.data.time.millis);
			break;
		case CONSOLE_INPUT:
			console_input(req.data.console_input.input);
			break;
		case CONSOLE_BACKSPACE:
			console_backspace();
			break;
		case CONSOLE_CLEAR:
			console_clear();
			break;
		case CONSOLE_FEEDBACK:
			console_feedback(req.data.feedback.feedback);
			break;
		case QUIT:
			return;
		default:
			printf("Got request type %d" EOL, req.type);
			ASSERT(0 && "Unknown request type in displaysrv");
			break;
		}
	}
}

void displaysrv(void) {
	int tid = create(HIGHER(PRIORITY_MIN, 1), displaysrv_start);
	// block until displaysrv has registered itself with the nameserver
	ASSERT(signal_send(tid) == 0);
}

// external interface to displaysrv

static void displaysrv_send(int displaysrv, enum displaysrv_req_type type, struct displaysrv_req *req) {
	req->type = type;
	ASSERT(send(displaysrv, req, sizeof(*req), NULL, 0) == 0);
}

void displaysrv_console_input(int displaysrv, char c) {
	struct displaysrv_req req;
	req.data.console_input.input = c;
	displaysrv_send(displaysrv, CONSOLE_INPUT, &req);
}

void displaysrv_console_backspace(int displaysrv) {
	struct displaysrv_req req;
	displaysrv_send(displaysrv, CONSOLE_BACKSPACE, &req);
}

void displaysrv_console_clear(int displaysrv) {
	struct displaysrv_req req;
	displaysrv_send(displaysrv, CONSOLE_CLEAR, &req);
}

void displaysrv_console_feedback(int displaysrv, char *fb) {
	ASSERT(strlen(fb) <= MAX_FEEDBACK_LEN);
	struct displaysrv_req req;
	strcpy(req.data.feedback.feedback, fb);
	displaysrv_send(displaysrv, CONSOLE_FEEDBACK, &req);
}

void displaysrv_update_sensor(int displaysrv, struct sensor_state *state) {
	struct displaysrv_req req;
	req.data.sensor.state = *state;
	displaysrv_send(displaysrv, UPDATE_SENSOR, &req);
}

void displaysrv_update_switch(int displaysrv, int sw, enum sw_direction pos) {
	struct displaysrv_req req;
	req.data.sw.num = sw;
	req.data.sw.pos = pos;
	displaysrv_send(displaysrv, UPDATE_SWITCH, &req);
}

void displaysrv_quit(int displaysrv) {
	struct displaysrv_req req;
	displaysrv_send(displaysrv, QUIT, &req);
}

static void displaysrv_update_time(int displaysrv, unsigned millis) {
	struct displaysrv_req req;
	req.data.time.millis = millis;
	displaysrv_send(displaysrv, UPDATE_TIME, &req);
}
