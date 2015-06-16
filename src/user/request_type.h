#pragma once
enum request_type {
	WHOIS, REGISTER_AS, // Name server
	TICK_HAPPENED, DELAY, TIME, DELAY_UNTIL, SHUTDOWN, // Clock server
	SET_SPEED, REVERSE, SWITCH_SWITCH, // Trains server
};
