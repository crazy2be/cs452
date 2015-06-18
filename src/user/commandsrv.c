#include "commandsrv.h"
#include <io_server.h>
#include <assert.h>
#include <util.h>
#include "servers.h"
#include "nameserver.h"
#include "displaysrv.h"
#include "trainsrv.h"

void get_command(char *buf, int buflen, int displaysrv) {
	int i = 0;
	for (;;) {
		char c = getc();
		if (c == '\r') {
			displaysrv_console_clear(displaysrv);
			break;
		} else if (c == '\b') {
			if (i > 0) {
				i--;
				displaysrv_console_backspace(displaysrv);
			}
		} else {
			displaysrv_console_input(displaysrv, c);
			buf[i++] = c;
			ASSERT(i < buflen && "We are about to overflow the command buffer");
		}
	}
	buf[i++] = '\0';
}

//
// Command parsing shit
//


// character helpers

inline int is_alpha(char c) {
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

inline int is_whitespace(char c) {
	// null-term counts as whitespace so the end of the line is treated as whitespace
	return (c == ' ') || (c == '\t') || (c == '\0');
}

inline int is_digit(char c) {
	return '0' <= c && c <= '9';
}

void consume_whitespace(char *cmd, int *ip) {
	int i = *ip;
	while (cmd[i] && is_whitespace(cmd[i])) i++;
	*ip = i;
};

enum command_type { TR, SW, RV, QUIT, INVALID };
char *command_listing[] = { "tr", "sw", "rv", "q" };

enum command_type get_command_type(char *cmd, int *ip) {
	int i = *ip;
	while (is_alpha(cmd[i])) {
		i++;
	}
	for (int c = 0; c < sizeof(command_listing) / sizeof(command_listing[0]); c++) {
		if (strncmp(cmd, command_listing[c], i) == 0 && command_listing[c][i] == '\0') {
			*ip = i;
			return (enum command_type) c;
		}
	}
	return INVALID;
}

int get_integer(char *cmd, int *ip, int *out) {
	int i = *ip;

    int sign = 1;
    int number = 0;

    if (cmd[i] == '-') {
        sign = -1;
		i++;
    }

    for (;;) {
        if (cmd[i] < '0' || cmd[i] > '9') {
            // illegal number
            return 1;
        }
        number = number * 10 + cmd[i++] - '0';

        if (is_whitespace(cmd[i])) {
            break;
        }
    }

    *out = sign * number;
	*ip = i;

    return 0;
}

//
// High level interface
//

void handle_tr(int displaysrv, int train, int speed) {
	trains_set_speed(train, speed);
	displaysrv_console_feedback(displaysrv, "");
}

void handle_sw(int displaysrv, int sw, enum sw_direction pos) {
	trains_switch(sw, pos);
	displaysrv_console_feedback(displaysrv, "");
}

void handle_rv(int displaysrv, int train) {
	trains_reverse(train);
	displaysrv_console_feedback(displaysrv, "");
}

void process_command(char *cmd, int displaysrv) {
	int i = 0;
	enum command_type type = get_command_type(cmd, &i);
	/* printf("Consuming whitespace at %d" EOL, i); */
	consume_whitespace(cmd, &i);
	/* printf("SWITCHING ON TYPE %d" EOL, type); */
	switch (type) {
	case TR:
		{
			int train, speed;
			if (get_integer(cmd, &i, &train)) {
				break;
			}
			consume_whitespace(cmd, &i);
			if (get_integer(cmd, &i, &speed)) {
				break;
			}
			handle_tr(displaysrv, train, speed);
			return;
		}
	case SW:
		{
			int sw;
			enum sw_direction pos;
			if (get_integer(cmd, &i, &sw)) {
				break;
			}
			consume_whitespace(cmd, &i);
			switch (cmd[i++]) {
			case 'C':
			case 'c':
				pos = CURVED;
				break;
			case 'S':
			case 's':
				pos = STRAIGHT;
				break;
			default:
				goto unknown;
			}
			if (cmd[i] != '\0') break;
			/* printf("Parsed command SW %d %d" EOL, sw, pos); */
			handle_sw(displaysrv, sw, pos);
			return;
		}
	case RV:
		{
			int train;
			if (get_integer(cmd, &i, &train)) {
				break;
			}
			handle_rv(displaysrv, train);
		}
		break;
	case QUIT:
		displaysrv_quit(displaysrv);
		stop_servers();
		break;
	default:
		break;
	}
unknown:
	// TODO: we need to have sprintf so that we can pass proper feedback in
	displaysrv_console_feedback(displaysrv, "Unknown command");
}

void commandsrv_start(void) {
	char buf[80];

	int displaysrv = whois(DISPLAYSRV_NAME);
	ASSERT(displaysrv >= 0);

	for (;;) {
		get_command(buf, sizeof(buf), displaysrv);
		printf("GOT COMMAND \"%s\"" EOL, buf);
		process_command(buf, displaysrv);
	}
}

void commandsrv(void) {
	create(HIGHER(PRIORITY_MIN, 5), commandsrv_start);
}
