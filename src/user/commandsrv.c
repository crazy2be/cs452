#include "commandsrv.h"
#include <io.h>
#include <assert.h>
#include <util.h>
#include "sys.h"
#include "signal.h"
#include "displaysrv.h"
#include "trainsrv.h"
#include "trainsrv/train_alert_srv.h"
#include "trainsrv/position.h"
#include "trainsrv/track_control.h" // TODO: remove this
#include "track.h"
#include "conductor.h"

static void get_command(char *buf, int buflen, int displaysrv) {
	int i = 0;
	static int parsing_esc = 0;
	const char DELETE = 127;
	for (;;) {
		char c = getc();
		if (c == '\r') {
			displaysrv_console_clear(displaysrv);
			break;
		} else if (c == '\b' || c == DELETE) {
			if (i > 0) {
				i--;
				displaysrv_console_backspace(displaysrv);
			}
			// Avoid echoing arrow keys. TODO: We could add line editing.
		} else if (c == 0x1B) {
			parsing_esc = 1;
		} else if (parsing_esc) {
			if (c == '[') {
				parsing_esc = 2;
				continue;
			}
			if ((c >= '0' && c <= '9') || c == ';' || c == ' ') continue;
			parsing_esc = 0;
			continue;
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

static inline int is_alpha(char c) {
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

static inline int is_whitespace(char c) {
	// null-term counts as whitespace so the end of the line is treated as whitespace
	return (c == ' ') || (c == '\t') || (c == '\0');
}

static inline int is_digit(char c) {
	return '0' <= c && c <= '9';
}

static inline int is_alphanum(char c) {
	return is_alpha(c) || is_digit(c);
}

static inline char to_upper(char c) {
	return ('a' <= c && c <= 'z') ? c + 'A' - 'a' : c;
}

static void consume_whitespace(char *cmd, int *ip) {
	int i = *ip;
	while (cmd[i] && is_whitespace(cmd[i])) i++;
	*ip = i;
};

enum command_type { TR, SW, RV, QUIT, STOP, BSW, BISECT, ROUTE, FREEZE, INVALID };
char *command_listing[] = { "tr", "sw", "rv", "q", "stop", "bsw", "bisect", "route", "f", "" };

static enum command_type get_command_type(char *cmd, int *ip) {
	int i = *ip;
	while (is_alpha(cmd[i])) {
		i++;
	}
	for (int c = 0; c < ARRAY_LENGTH(command_listing); c++) {
		if (strncmp(cmd, command_listing[c], i) == 0 && command_listing[c][i] == '\0') {
			*ip = i;
			return (enum command_type) c;
		}
	}
	return INVALID;
}

static int get_integer(char *cmd, int *ip, int *out) {
	int i = *ip;

	int sign = 1;
	int number = 0;

	if (cmd[i] == '-') {
		sign = -1;
		i++;
	}

	for (;;) {
		if (!is_digit(cmd[i])) {
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

static const struct track_node* get_node(char *cmd, int *ip) {
	int i = *ip;
	const int max_node_len = 5;
	char buf[max_node_len + 1]; // buffer for name of node: max name length + null term

	int j;
	for (j = 0; j < max_node_len; j++) {
		if (is_whitespace(cmd[i])) {
			break;
		} else if (is_alphanum(cmd[i])) {
			buf[j] = to_upper(cmd[i++]);
		} else {
			// unknown character
			return NULL;
		}
	}
	if (j == 0 || !is_whitespace(cmd[i])) {
		// name was empty, or got cut off
		return NULL;
	}
	buf[j] = '\0';

	for (j = 0; j < ARRAY_LENGTH(track); j++) {
		if (strcmp(track[j].name, buf) == 0) {
			*ip = i;
			return &track[j];
		}
	}
	return NULL;
}

//
// High level interface
//

static void handle_tr(int displaysrv, int train, int speed) {
	trains_set_speed(train, speed);
	displaysrv_console_feedback(displaysrv, "");
}

static void handle_sw(int displaysrv, int sw, enum sw_direction pos) {
	trains_switch(sw, pos);
	displaysrv_console_feedback(displaysrv, "");
}

static void handle_rv(int displaysrv, int train) {
	trains_reverse(train);
	displaysrv_console_feedback(displaysrv, "");
}


struct stop_task_params {
	int train;
	struct position pos;
};
static void stop_task(void) {
	int tid;
	struct stop_task_params params;
	receive(&tid, &params, sizeof(params));
	reply(tid, NULL, 0);
	train_alert_at(params.train, params.pos);
	trains_set_speed(params.train, 0);
}

static void handle_stop(int displaysrv, int train, struct position pos) {
	struct train_state state;
	trains_query_spatials(train, &state);
	struct switch_state switches = trains_get_switches();

	int stopping_distance = trains_get_stopping_distance(train);
	struct position stopping_point = position_calculate_stopping_position(&state.position, &pos, stopping_distance, &switches);
	if (position_is_uninitialized(&stopping_point)) {
		displaysrv_console_feedback(displaysrv, "No path to target location");
		return;
	}

	int tid = create(PRIORITY_COMMANDSRV, stop_task);
	struct stop_task_params params = { train, stopping_point };
	send(tid, &params, sizeof(params), NULL, 0);
}

struct bisect_params { int displaysrv; int train; };

static void bisect_task(void) {
	struct bisect_params params;
	int tid;
	receive(&tid, &params, sizeof(params));
	reply(tid, NULL, 0);

#ifdef TRACKA
	/* struct position target = { &track[61].edge[DIR_STRAIGHT], 0 }; // d14 */
	struct position target = { &track[31].edge[DIR_STRAIGHT], 0 }; // b16
#else
	struct position target = { &track[21].edge[DIR_STRAIGHT], 0 }; // b6
#endif

	// overall approach: start with a range of guesses for the stopping distance
	// try to stop exactly on a sensor. if the sensor trips, we guessed too long,
	// otherwise too short. we bisect to refine our guess.
	// this takes O(log_2(n)), with a big fucking constant factor.
	//
	// Note: this will just break if somebody switches the turnouts or changes
	// the train speed while this is in progress.
	// TODO: this seems to come up a fair bit - we may want to have a lock on
	// controlling a particular train for TC2

	struct train_state ts;
	trains_query_spatials(params.train, &ts);
	struct switch_state switches = trains_get_switches();
	const int speed_setting = ts.speed_setting;

	// initial guesses for the train stopping distance
	int lo = 0;
	int hi = 1500;
	int guess;

	const int target_sensor = target.edge->src->num;;
	char target_repr[4];
	sensor_repr(target_sensor, target_repr);

	const int iterations = 8;

	for (int i = 0; i < iterations; i++) {
		guess = (lo + hi) / 2;
		struct position stopping_point = position_calculate_stopping_position(&ts.position, &target, guess, &switches);

		train_alert_at(params.train, stopping_point);
		trains_set_speed(params.train, 0);

		// conservative guess to ensure the train is fully stopped
		delay(500);
		int current_sensor = trains_get_last_known_sensor(params.train);
		struct position current_position = { &track_node_from_sensor(current_sensor)->edge[DIR_STRAIGHT], 0};
		char current_repr[4];
		sensor_repr(current_sensor, current_repr);

		if (position_distance_apart(&current_position, &target, &switches) < position_distance_apart(&target, &current_position, &switches)) {
			// undershot - stopping distance is less than we think it is
			hi = guess;
			logf("Round %d/%d, undershot, target %s, current %s, lo %d, hi %d",
				 i+1, iterations, target_repr, current_repr, lo, hi);
		} else {
			// overshot - stopping distance is more than we think it is
			lo = guess;
			logf("Round %d/%d, overshot, target %s, current %s, lo %d, hi %d",
				 i+1, iterations, target_repr, current_repr, lo, hi);
		}

		trains_set_speed(params.train, speed_setting);

		trains_query_spatials(params.train, &ts);

		// delay
		delay(500);
	}


	guess = (lo + hi) / 2;
	trains_set_stopping_distance(params.train, guess);

	logf("Bisected stopping distance to find stopping distance of %d mm", guess);
}

static void handle_bisect(int displaysrv, int train) {
	int tid = create(PRIORITY_COMMANDSRV, bisect_task);
	struct bisect_params params = { displaysrv, train };
	send(tid, &params, sizeof(params), NULL, 0);
}
static bool get_train_number(char *s, int *i, int *train) {
	if (get_integer(s, i, train)) {
		displaysrv_console_feedback(whois("displaysrv"), "Expected train number");
		return false;
	}
	if (1 <= *train && *train <= 80) return true;
	displaysrv_console_feedback(whois("displaysrv"), "Invalid train number");
	return false;
}
static bool get_speed(char *s, int *i, int *speed) {
	if (get_integer(s, i, speed)) {
		displaysrv_console_feedback(whois("displaysrv"), "Expected speed");
		return false;
	}
	if (0 <= *speed && *speed <= 14) return true;
	displaysrv_console_feedback(whois("displaysrv"), "Invalid speed");
	return false;
}
static bool get_switch_direction(char *s, int *i, enum sw_direction *dir) {
	switch (s[(*i)++]) {
	case 'C': case 'c': *dir = CURVED; return true;
	case 'S': case 's': *dir = STRAIGHT; return true;
	default:
		displaysrv_console_feedback(whois("displaysrv"), "Invalid switch direction");
		return false;
	}
}
static void process_command(char *cmd, int displaysrv) {
	int i = 0;
	enum command_type type = get_command_type(cmd, &i);
	consume_whitespace(cmd, &i);
	switch (type) {
	case TR: {
		int train, speed;
		if (!get_train_number(cmd, &i, &train)) return;
		consume_whitespace(cmd, &i);
		if (!get_speed(cmd, &i, &speed)) return;
		handle_tr(displaysrv, train, speed);
		return;
	}
	case SW: {
		int sw;
		enum sw_direction dir;
		if (get_integer(cmd, &i, &sw)) break;
		consume_whitespace(cmd, &i);
		if (!get_switch_direction(cmd, &i, &dir)) return;
		if (cmd[i] != '\0') break;
		/* printf("Parsed command SW %d %d" EOL, sw, pos); */
		if (!((1 <= sw && sw <= 18) || (145 <= sw && sw <= 156))) {
			displaysrv_console_feedback(displaysrv, "Invalid switch");
		} else {
			handle_sw(displaysrv, sw, dir);
		}
		return;
	}
	case RV: {
		int train;
		if (!get_train_number(cmd, &i, &train)) return;
		handle_rv(displaysrv, train);
		return;
	}
	case QUIT:
		displaysrv_quit(displaysrv);
		stop_servers();
		return;
	case STOP: {
		// command can be in the form:
		//     stp <train> <node> <edge choice> <displacement>
		// OR
		//     stp <train> <node> <displacement>
		int train;
		int edge_choice = 0;
		int displacement;
		const struct track_node *node;
		if (!get_train_number(cmd, &i, &train)) return;
		consume_whitespace(cmd, &i);
		if ((node = get_node(cmd, &i)) == NULL) {
			displaysrv_console_feedback(displaysrv, "failed to parse node" EOL);
			return;
		}
		consume_whitespace(cmd, &i);
		if (get_integer(cmd, &i, &displacement)) {
			displaysrv_console_feedback(displaysrv, "failed to parse displacement" EOL);
			return;
		}
		consume_whitespace(cmd, &i);

		// 4th argument is optional
		if (cmd[i] != '\0') {
			edge_choice = displacement;
			if (get_integer(cmd, &i, &displacement)) {
				displaysrv_console_feedback(displaysrv, "failed to parse displacement (2)" EOL);
				return;
			}
		}

		if (!(0 == edge_choice || (1 == edge_choice && node->type == NODE_BRANCH))) {
			displaysrv_console_feedback(displaysrv, "Invalid edge selection");
		} else if (!(0 <= displacement && displacement <= node->edge[edge_choice].dist)) {
			displaysrv_console_feedback(displaysrv, "Overlong displacement");
		} else {
			/* char buf[80]; */
			/* snprintf(buf, sizeof(buf), "Got STP %d %s:%d + %d", train, node->name, edge_choice, displacement); */
			/* displaysrv_console_feedback(displaysrv, buf); */
			handle_stop(displaysrv, train, (struct position) {
				&node->edge[edge_choice], displacement
			});
		}
		return;
	}
	case BSW: {
		enum sw_direction dir;
		consume_whitespace(cmd, &i);
		if (!get_switch_direction(cmd, &i, &dir)) return;
		if (cmd[i] != '\0') break;
		struct switch_state switches;
		memset(&switches, dir == CURVED ? 0xff : 0x00, sizeof(switches));
		tc_switch_switches_bulk(switches);
		return;
	}
	case BISECT: {
		int train;
		if (!get_train_number(cmd, &i, &train)) return;
		handle_bisect(displaysrv, train);
		return;
	}
	case ROUTE: {
		int train = -1;
		if (!get_train_number(cmd, &i, &train)) return;
		consume_whitespace(cmd, &i);
		const struct track_node *node = NULL;
		if ((node = get_node(cmd, &i)) == NULL) {
			displaysrv_console_feedback(displaysrv, "Failed to parse node");
			return;
		}
		char conductor_name[CONDUCTOR_NAME_LEN];
		conductor_get_name(train, &conductor_name);

		int tid = try_whois(conductor_name);
		if (tid < 0) {
			char buf[80];
			snprintf(buf, sizeof(buf), "No conductor for train %d (could not whois %s: %d)", train, conductor_name, tid);
			displaysrv_console_feedback(displaysrv, buf);
			return;
		}
		struct conductor_req req;
		req.type = CND_DEST;
		req.u.dest.dest = node;

		send(tid, &req, sizeof(req), NULL, 0);
		return;
	}
	case FREEZE: {
		displaysrv_console_freeze();
	}
	default:
		break;
	}
	// TODO: we need to have sprintf so that we can pass proper feedback in
	displaysrv_console_feedback(displaysrv, "Unknown command");
}

void commandsrv(void) {
	register_as("commandsrv");
	signal_recv();

	int displaysrv = whois(DISPLAYSRV_NAME);
	ASSERT(displaysrv >= 0);

	for (;;) {
		char buf[80];
		get_command(buf, sizeof(buf), displaysrv);
		process_command(buf, displaysrv);
	}
}

void commandsrv_start(void) {
	int tid = create(PRIORITY_COMMANDSRV, commandsrv);
	signal_send(tid);
}
