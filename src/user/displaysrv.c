#include "displaysrv.h"
#include "signal.h"
#include "nameserver.h"
#include "util.h"
#include "track_control.h"

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

#define HLINE "\xe2\x94\x80"
#define VLINE "\xe2\x94\x82"
#define ULCORNER "\xe2\x94\x8c"
#define URCORNER "\xe2\x94\x90"
#define BLCORNER "\xe2\x94\x94"
#define BRCORNER "\xe2\x94\x98"
#define LTEE "\xe2\x94\x9c"
#define RTEE "\xe2\x94\xa4"
#define UTEE "\xe2\x94\xac"
#define DTEE "\xe2\x94\xb4"
#define CROSS "\xe2\x94\xbc"

#define SCREEN_WIDTH 80
#define TRAIN_X_OFFSET 1 + 1
#define TRAIN_Y_OFFSET 1 + 1
#define CONSOLE_X_OFFSET 1 + 1
#define CONSOLE_Y_OFFSET (1 + 1 + TRACK_DISPLAY_HEIGHT + 1)

void reset_console_cursor(void) {
	printf("\e[%d;%dH", CONSOLE_Y_OFFSET, CONSOLE_X_OFFSET);
}

void initial_draw(void) {
	puts("\e[2J\e[;H" ULCORNER);
	for (int i = 0; i < SCREEN_WIDTH - 2; i++) puts(HLINE);
	puts(URCORNER EOL);

	for (int i = 0; i < TRACK_DISPLAY_HEIGHT; i++) {
		printf(VLINE "%s                     " VLINE EOL, track_repr[i]);
	}

	puts(LTEE);
	for (int i = 0; i < SCREEN_WIDTH - 2; i++) puts(HLINE);
	puts(RTEE EOL);

	puts(VLINE "\e[78C" VLINE EOL);

	puts(BLCORNER);
	for (int i = 0; i < SCREEN_WIDTH - 2; i++) puts(HLINE);
	puts(BRCORNER EOL);

	reset_console_cursor();
}

#define MAX_FEEDBACK_LEN 80
enum displaysrv_req_type { UPDATE_SWITCH, UPDATE_SENSOR, CONSOLE_INPUT,
	CONSOLE_BACKSPACE, CONSOLE_CLEAR, CONSOLE_FEEDBACK };
struct displaysrv_req {
	enum displaysrv_req_type type;
	// the data associated with each request
	union {
		struct {
			unsigned char num;
			enum sw_direction pos;
		} sw;
		struct {
			unsigned char num;
			unsigned char tripped;
		} sensor;
		struct {
			char input;
		} console_input;
		// nothing for console clear or backspace
		struct {
			char feedback[MAX_FEEDBACK_LEN + 1]; // null terminated string
		} feedback;
	} data;
};

void update_switch(int sw, enum sw_direction pos) {
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
		ASSERT(0 && "Unknown switch number");
	}
}

void console_input(char c) {
	putc(c);
}

void console_backspace(void) {
	puts("\b \b");
}

void console_clear(void) {
	reset_console_cursor();
	puts("                                                                              ");
	reset_console_cursor();
}

void displaysrv_start(void) {
	register_as(DISPLAYSRV_NAME);
	signal_recv();

	initial_draw();

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
			break;
		default:
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

void displaysrv_console_input(int displaysrv, char c) {
	struct displaysrv_req req;
	req.type = CONSOLE_INPUT;
	req.data.console_input.input = c;
	ASSERT(send(displaysrv, &req, sizeof(req), NULL, 0) == 0);
}

void displaysrv_console_backspace(int displaysrv) {
	struct displaysrv_req req;
	req.type = CONSOLE_BACKSPACE;
	ASSERT(send(displaysrv, &req, sizeof(req), NULL, 0) == 0);
}

void displaysrv_console_clear(int displaysrv) {
	struct displaysrv_req req;
	req.type = CONSOLE_CLEAR;
	ASSERT(send(displaysrv, &req, sizeof(req), NULL, 0) == 0);
}

void displaysrv_feedback(int displaysrv, char *fb) {
	ASSERT(strlen(fb) <= MAX_FEEDBACK_LEN);

	struct displaysrv_req req;
	req.type = CONSOLE_FEEDBACK;
	strcpy(req.data.feedback.feedback, fb);
	ASSERT(send(displaysrv, &req, sizeof(req), NULL, 0) == 0);
}

void displaysrv_update_switch(int displaysrv, int sw, enum sw_direction pos) {
	struct displaysrv_req req;
	req.type = UPDATE_SWITCH;
	req.data.sw.num = sw;
	req.data.sw.pos = pos;
	ASSERT(send(displaysrv, &req, sizeof(req), NULL, 0) == 0);
}
