#include "displaysrv.h"
#include "signal.h"
#include "sys.h"
#include "util.h"
#include "switch_state.h"
#include "trainsrv.h"
#include "track.h"

#include <assert.h>
#include <kernel.h>
#include <io.h>

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
	unsigned char lx, ly;
	char l;
	unsigned char sx, sy;
	char s;
	unsigned char rx, ry;
	char r;
};

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

static const struct sw3_display switch3_display_info[] = {
	{ 21, 11, '\\', 22, 11, '|', 23, 11, '/' },
	{ 23, 17, '\\', 22, 17, '|', 21, 17, '/' },
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
#define TRACK_X_OFFSET (1 + 1)
#define TRACK_Y_OFFSET (1 + 1)
#define RIGHT_BAR_X_OFFSET (TRACK_X_OFFSET + TRACK_DISPLAY_WIDTH - 3)
#define CLOCK_X_OFFSET (TRACK_X_OFFSET + TRACK_DISPLAY_WIDTH)
#define CLOCK_Y_OFFSET TRACK_Y_OFFSET
#define SENSORS_X_OFFSET CLOCK_X_OFFSET
#define SENSORS_Y_OFFSET (CLOCK_Y_OFFSET + 2)
#define TRAIN_STATUS_X_OFFSET TRACK_X_OFFSET
#define TRAIN_STATUS_Y_OFFSET (1 + TRACK_DISPLAY_HEIGHT + 1 + 2 + 1)
#define FEEDBACK_X_OFFSET TRACK_X_OFFSET
#define FEEDBACK_Y_OFFSET (TRAIN_STATUS_Y_OFFSET + 4 + 1)
#define CONSOLE_X_OFFSET TRACK_X_OFFSET
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
	printf("\e[%d;%dH" VLINE "          " VLINE "          " VLINE EOL, 2, RIGHT_BAR_X_OFFSET + 1);
	printf("\e[%dC", RIGHT_BAR_X_OFFSET);
	hline(SCREEN_WIDTH - TRACK_DISPLAY_WIDTH + 1, LTEE, RTEE);
	puts(EOL);
	vbox(TRACK_DISPLAY_HEIGHT - 1, SCREEN_WIDTH - 2 - TRACK_DISPLAY_WIDTH + 1,
	     RIGHT_BAR_X_OFFSET);

	// print out the bar below the track
	hline(TRACK_DISPLAY_WIDTH, LTEE, UTEE);
	hline(SCREEN_WIDTH - TRACK_DISPLAY_WIDTH, HLINE, RTEE);
	puts(EOL);

	// print out a box for train status info
	vbox(4 + 1, SCREEN_WIDTH - 2, 0);
	hline(SCREEN_WIDTH, LTEE, RTEE);
	puts(EOL);
	// print out title set within the train status box
	printf("\e[s\e[%d;%dHTrain states\e[u", TRAIN_STATUS_Y_OFFSET - 1, TRAIN_STATUS_X_OFFSET);

	// print out a box for console feedback
	vbox(1, SCREEN_WIDTH - 2, 0);
	hline(SCREEN_WIDTH, LTEE, RTEE);
	puts(EOL);

	// print out a box for console commands
	vbox(1, SCREEN_WIDTH - 2, 0);
	hline(SCREEN_WIDTH, BLCORNER, BRCORNER);
	puts(EOL);

	reset_console_cursor();
}

#define MAX_FEEDBACK_LEN 80
#define MAX_LOG_LEN 60
#define LOG_LINE_BUFSIZE (MAX_LOG_LEN + 2) // +2 for ellipsis
#define MAX_LOG_LINES 36
enum displaysrv_req_type {
	UPDATE_SWITCH, UPDATE_SENSOR, UPDATE_TIME, UPDATE_TRACK,
	CONSOLE_INPUT, CONSOLE_BACKSPACE, CONSOLE_CLEAR, CONSOLE_FEEDBACK,
	CONSOLE_LOG, QUIT};

struct display_train_state {
	int train_id;
	struct train_state state;
	int est_stopping_distance;
	int error;
};

struct displaysrv_req {
	enum displaysrv_req_type type;
	// the data associated with each request
	union {
		struct {
			struct switch_state state;
		} sw;
		struct {
			struct sensor_state state;
			unsigned avg_delay;
		} sensor;
		struct {
			int *table;
		} track;
		struct {
			char input;
		} console_input;
		// nothing for console clear, backspace, or quit
		struct {
			char feedback[MAX_FEEDBACK_LEN + 1]; // null terminated string
		} feedback;
		struct {
			unsigned millis;
			int active_trains;
			struct display_train_state active_train_states[MAX_ACTIVE_TRAINS];
		} time;
		struct {
			char msg[LOG_LINE_BUFSIZE];
		} log;
	} data;
};

static int current_log_line = 0;
static void log_print(char *buf, const char *fmt, va_list va) {
	int n = snprintf(buf, MAX_LOG_LEN, "%6d ", time());
	n += vsnprintf(buf + n, MAX_LOG_LEN - n, fmt, va);
	if (n > MAX_LOG_LEN) {
		// ... character http://www.fileformat.info/info/unicode/char/2026/index.htm
		buf[LOG_LINE_BUFSIZE - 4] = 0b11100010;
		buf[LOG_LINE_BUFSIZE - 3] = 0b10000000;
		buf[LOG_LINE_BUFSIZE - 2] = 0b10100110;
		buf[LOG_LINE_BUFSIZE - 1] = '\0';
	} else {
		for (int i = n; i < MAX_LOG_LEN; i++) {
			buf[i] = ' ';
		}
		buf[MAX_LOG_LEN] = '\0';
	}
}
static void handle_log(char *msg) {
	printf("\e[s");
	printf("\e[%d;%dH%s", current_log_line + 2, 82, msg);
	current_log_line = (current_log_line + 1) % MAX_LOG_LINES;
	printf("\e[u");
}
// For logging internal to displaysrv.
void dlogf(const char *fmt, ...) {
	va_list va;
	va_start(va,fmt);
	char buf[LOG_LINE_BUFSIZE];
	log_print(buf, fmt, va);
	handle_log(buf);
	va_end(va);
}
#define SENSOR_BUF_SIZE (TRACK_DISPLAY_HEIGHT + 2 - 3)
struct sensor_reads {
	int start, len;
	unsigned char sensors[SENSOR_BUF_SIZE];
};

static void record_sensor_read(struct sensor_reads *reads, int sensor) {
	int last = (reads->start + reads->len) % SENSOR_BUF_SIZE;
	reads->sensors[last] = sensor;

	if (reads->len < SENSOR_BUF_SIZE) {
		reads->len++;
	} else {
		ASSERT(reads->start == last); // we should have wrapped around to the right spot
		reads->start = (reads->start + 1) % SENSOR_BUF_SIZE;
	}

	printf("\e[s");
	for (int j = reads->len - 1; j >= 0; j--) {
		char buf[4];
		sensor_repr(reads->sensors[(reads->start + j) % SENSOR_BUF_SIZE], buf);
		printf("\e[%d;%dH%s ", SENSORS_Y_OFFSET + reads->len - 1 - j, SENSORS_X_OFFSET, buf);
	}
	printf("\e[u");
}

static void update_sensor_display(int sensor, int blank) {
	struct sensor_display coords = sensor_display_info[sensor / 2];
	char buf[4];
	sensor_repr(sensor, buf);
	if (blank) {
		// clear sensor from display
		buf[0] = buf[1] = buf[2] = ' ';
		buf[3] = '\0';
	} else if (buf[2] == '\0') {
		buf[2] = ' ';
		buf[3] = '\0';
	}
	printf("\e[s\e[%d;%dH%s\e[u", coords.y + TRACK_Y_OFFSET,
	       coords.x + TRACK_X_OFFSET, buf);
}

static void update_sensor(struct sensor_state *sensors, struct sensor_state *old_sensors, struct sensor_reads *reads, unsigned delay_time) {
	// update displayed sensor delay time
	printf("\e[s\e[%d;%dH%03d\e[u", CLOCK_Y_OFFSET, CLOCK_X_OFFSET + 17, delay_time);
	for (int i = 0; i < SENSOR_COUNT; i += 2) {
		int s1 = sensor_get(sensors, i);
		int s2 = sensor_get(sensors, i + 1);
		if (s1 != sensor_get(old_sensors, i) || s2 != sensor_get(old_sensors, i + 1)) {
			int to_draw = s1 ? i : i + 1;
			update_sensor_display(to_draw, !(s1 || s2));
			if (s1 || s2) record_sensor_read(reads, to_draw);
		}
	}
	*old_sensors = *sensors;
}

static void update_single_2switch(int sw, enum sw_direction pos) {
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
		printf("\e[s\e[%d;%dH\e[1;33m%c\e[0m\e[%d;%dH%c\e[u",
		       current_y + TRACK_Y_OFFSET, current_x + TRACK_X_OFFSET, switch_char,
		       last_y + TRACK_Y_OFFSET, last_x + TRACK_X_OFFSET, filler_char);
	} else {
		//ASSERT(0 && "Unknown switch number");
	}
}

static void update_single_3switch(int first_switch, enum sw_direction s1, enum sw_direction s2) {
	const struct sw3_display *coords = &switch3_display_info[(first_switch - 153)/ 2];

	unsigned char tx, ty; // coords for piece of track to draw
	unsigned char b1x, b1y, b2x, b2y; // coords for piece of track to blank out
	char switch_char;

	if (s1 == CURVED && s2 == STRAIGHT) { // right
		tx = coords->rx;
		ty = coords->ry;
		switch_char = coords->r;

		b1x = coords->lx;
		b1y = coords->ly;
		b2x = coords->sx;
		b2y = coords->sy;
	} else if (s1 == STRAIGHT && s2 == CURVED) { // left
		tx = coords->lx;
		ty = coords->ly;
		switch_char = coords->l;

		b1x = coords->rx;
		b1y = coords->ry;
		b2x = coords->sx;
		b2y = coords->sy;
	} else {
		tx = coords->sx;
		ty = coords->sy;
		switch_char = (s1 == CURVED) ? 'X' : coords->s;

		b1x = coords->lx;
		b1y = coords->ly;
		b2x = coords->rx;
		b2y = coords->ry;
	}
	printf("\e[s\e[%d;%dH\e[1;33m%c\e[0m\e[%d;%dH \e[%d;%dH \e[u",
	       ty + TRACK_Y_OFFSET, tx + TRACK_X_OFFSET, switch_char,
	       b1y + TRACK_Y_OFFSET, b1x + TRACK_X_OFFSET,
	       b2y + TRACK_Y_OFFSET, b2x + TRACK_Y_OFFSET);

}

static void update_switch(struct switch_state *state, struct switch_state *old_state) {
	for (int i = 1; i <= 18; i++) {
		enum sw_direction s = switch_get(state, i);
		if (s != switch_get(old_state, i)) {
			update_single_2switch(i, s);
		}
	}
	for (int i = 153; i <= 155; i += 2) {
		enum sw_direction s1 = switch_get(state, i);
		enum sw_direction s2 = switch_get(state, i + 1);
		if (s1 != switch_get(old_state, i) || s2 != switch_get(old_state, i + 1)) {
			update_single_3switch(i, s1, s2);
		}
	}
	*old_state = *state;
}

static void update_time(unsigned millis) {
	int tenths = (millis % 1000) / 100;
	int seconds = millis / 1000;
	int minutes = seconds / 60;

	seconds %= 60;
	minutes %= 60;

	int idle = idle_permille();
	int idle_whole = idle / 10;
	int idle_decimal = idle % 10;

	printf("\e[s\e[%d;%dH%02d:%02d:%d " VLINE " %02d.%d%% " VLINE "\e[u",
			CLOCK_Y_OFFSET, CLOCK_X_OFFSET - 1, minutes, seconds, tenths,
			idle_whole, idle_decimal);
}

static void update_train_states(int active_trains, struct display_train_state *active_train_states,
                                const struct switch_state *switches) {

	puts("\e[s");
	ASSERT(active_trains <= 4);
	for (int i = 0; i < active_trains; i++) {
		const int row = i;

		const int term_col = TRAIN_STATUS_X_OFFSET;
		const int term_row = TRAIN_STATUS_Y_OFFSET + row;

		const int train_id = active_train_states[i].train_id;

		if (position_is_uninitialized(&active_train_states[i].state.position)) {
			printf("\e[%d;%dH%d / %d => Train %d, position unknown",
			       term_row, term_col, i, active_trains, train_id);
		} else {
			const int displacement = active_train_states[i].state.position.displacement;
			const char *pos_name = active_train_states[i].state.position.edge->src->name;
			const int velocity = active_train_states[i].state.velocity;
			const int stopping_distance = active_train_states[i].est_stopping_distance;
			const int error = active_train_states[i].error;

			char buf[78];
			int len = snprintf(buf, sizeof(buf), "\e[%d;%dH%d / %d => Train %d, %d mm past %s, vel %d, %d to stop, %d error",
			       term_row, term_col, i, active_trains, train_id, displacement, pos_name, velocity,
			       stopping_distance, error);
			while (len < 78) {
				buf[len] = ' ';
				len++;
			}
			fput_buf(buf, sizeof(buf), COM2);
		}
	}
	puts("\e[u");
}
struct node_display {
	const char *name;
	const char *name_r;
	unsigned char x, y;
};

static const struct node_display node_display_info[] = {
	{"C3", "C4", 9, 0},
	{"MR5", "BR5", 14, 0},
	{"MR18", "BR18", 29, 0},
	{"C7", "C8", 34, 0},
	{"MR3", "BR3", 38, 0},
	{"A5", "A6", 43, 0},
	{"B9", "B10", 52, 0},
	{"E11", "E12", 8, 3},
	{"MR7", "BR7", 11, 4},
	{"D11", "D12", 18, 3},
	{"C15", "C16", 26, 3},
	{"MR6", "BR6", 32, 4},
	{"C5", "C6", 35, 3},
	// TODO: Fill in the rest of this table
};
bool find_track_node_pos(const char *name, int *x, int *y) {
	for (int i = 0; i < ARRAY_LENGTH(node_display_info); i++) {
		if (strcmp(name, node_display_info[i].name) == 0 ||
			strcmp(name, node_display_info[i].name_r) == 0) {
				*x = node_display_info[i].x;
				*y = node_display_info[i].y;
				dlogf("Found track node %s", name);
				return true;
		}
	}
	//dlogf("Could not find track node %s", name);
	return false;
}
static void update_track(int *track_table) {
	dlogf("Update_track called");
	for (int i = 0; i < TRACK_MAX; i++) {
		int x = -1, y = -1;
		bool exists = find_track_node_pos(track[i].name, &x, &y);
		if (exists) {
			puts("\e[s");
			printf("\e[%d;%dH\e[1;31m#\e[0m", y + 2, x + 2);
			puts("\e[u");
		}
// 			puts("\e[s");
// 			printf("\e[%d;%dH\e[1;31m%c\e[0m", y + 2, x + 2, track_repr[y][x]);
// 			puts("\e[u");
	}
}

static void console_input(char c) {
	putc(c);
}

static void console_backspace(void) {
	puts("\b \b");
}

static void console_feedback(char *fb) {
	puts("\e[s");
	clear_line(FEEDBACK_Y_OFFSET);
	puts(fb);
	puts("\e[u");
}

static void console_clear(void) {
	clear_line(CONSOLE_Y_OFFSET);
}

static void displaysrv_update_time(int displaysrv, unsigned millis, int active_trains, const struct display_train_state *active_train_states);

static void clock_update_task(void) {
	int ticks = 0;
	int displaysrv = parent_tid();
	int trainsrv = -1;
	for (;;) {
		ticks = delay_until(ticks + 10);
		if (trainsrv < 0) {
			// Sometimes the clock_update task starts before the trainsrv has
			// had a chance to start up. We could fix this by properly ordering
			// our task startups, but this is easier and faster and seems to
			// work just fine thank you very much.
			trainsrv = try_whois("trains");
			continue;
		}

		int active_trains_ids[MAX_ACTIVE_TRAINS];
		const int active_trains = trains_query_active(active_trains_ids);

		struct display_train_state active_train_states[MAX_ACTIVE_TRAINS];
		for (int i = 0; i < active_trains; i++) {
			const int train_id = active_trains_ids[i];
			active_train_states[i].train_id = train_id;
			trains_query_spatials(train_id, &active_train_states[i].state);
			active_train_states[i].est_stopping_distance = trains_get_stopping_distance(train_id);
			active_train_states[i].error = trains_query_error(train_id);
		}

		displaysrv_update_time(displaysrv, ticks * 10, active_trains, active_train_states);
	}
}
void ptid(void) {
	printf("Displayserver tid: %d"EOL, tid());
}
void displaysrv(void) {
	register_as(DISPLAYSRV_NAME);
	ptid();
	create(PRIORITY_DISPLAYSRV_CLOCK_UPDATE, clock_update_task);
	signal_recv();

	initial_draw();

	struct sensor_state old_sensors;
	struct sensor_reads sensor_reads;
	struct switch_state old_switches;
	sensor_reads.len = sensor_reads.start = 0;
	memset(&old_sensors, 0, sizeof(old_sensors));
	memset(&old_switches, 0, sizeof(old_switches));
	struct displaysrv_req req;
	int tid;


	printf("\e[s\e[1;82H------LOG:-----\e[u");
	int mock_table[TRACK_MAX] = {}; // For testing.
	update_track(mock_table);

	for (;;) {
		receive(&tid, &req, sizeof(req));
		// requests are fire and forget, and provide no feedback
		reply(tid, NULL, 0);

		switch (req.type) {
		case UPDATE_SWITCH:
			update_switch(&req.data.sw.state, &old_switches);
			break;
		case UPDATE_SENSOR:
			update_sensor(&req.data.sensor.state, &old_sensors, &sensor_reads, req.data.sensor.avg_delay);
			break;
		case UPDATE_TIME:
			update_time(req.data.time.millis);
			update_train_states(req.data.time.active_trains, req.data.time.active_train_states, &old_switches);
			break;
		case UPDATE_TRACK:
			update_track(req.data.track.table);
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
		case CONSOLE_LOG:
			handle_log(req.data.log.msg);
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

void displaysrv_start(void) {
	int tid = create(PRIORITY_DISPLAYSRV, displaysrv);
	// block until displaysrv has registered itself with the nameserver
	signal_send(tid);
}

// external interface to displaysrv
// TODO: These functions should probably be async/fire and forget for the most
// part, but no time to fix it right now.
static void displaysrv_send(int displaysrv, enum displaysrv_req_type type, struct displaysrv_req *req) {
	req->type = type;
	send(displaysrv, req, sizeof(*req), NULL, 0);
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

void displaysrv_update_sensor(int displaysrv, struct sensor_state *state, unsigned avg_delay) {
	struct displaysrv_req req;
	req.data.sensor.state = *state;
	req.data.sensor.avg_delay = avg_delay;
	displaysrv_send(displaysrv, UPDATE_SENSOR, &req);
}

void displaysrv_update_switch(int displaysrv, struct switch_state *state) {
	struct displaysrv_req req;
	req.data.sw.state = *state;
	displaysrv_send(displaysrv, UPDATE_SWITCH, &req);
}

void displaysrv_update_track_table(int displaysrv, int *track_table) {
	struct displaysrv_req req;
	req.data.track.table = track_table;
	displaysrv_send(displaysrv, UPDATE_TRACK, &req);
}

void displaysrv_log(const char *fmt, ...) {
	va_list va;
	va_start(va,fmt);
	struct displaysrv_req req;
	log_print(req.data.log.msg, fmt, va);
	int tid = whois(DISPLAYSRV_NAME);
	displaysrv_send(tid, CONSOLE_LOG, &req);
	va_end(va);
}

void displaysrv_quit(int displaysrv) {
	struct displaysrv_req req;
	displaysrv_send(displaysrv, QUIT, &req);
}

static void displaysrv_update_time(int displaysrv, unsigned millis, int active_trains, const struct display_train_state *active_train_states) {
	struct displaysrv_req req;
	req.data.time.millis = millis;
	req.data.time.active_trains = active_trains;
	memcpy(&req.data.time.active_train_states, active_train_states, sizeof(struct display_train_state) * active_trains);
	displaysrv_send(displaysrv, UPDATE_TIME, &req);
}
