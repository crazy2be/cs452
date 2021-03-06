#pragma once
enum request_type {
	INVALID_REQUEST, // Nobody

	WHOIS, REGISTER_AS, DUMP_NAMES, // Name server

	TICK_HAPPENED, DELAY, DELAY_UNTIL, DELAY_ASYNC, TIME, SHUTDOWN, // Clock server

	QUERY_ACTIVE, QUERY_SPATIALS, QUERY_ARRIVAL, SEND_SENSORS,
	SET_SPEED, REVERSE, REVERSE_UNSAFE, SWITCH_SWITCH, SWITCH_GET,
    GET_STOPPING_DISTANCE, SET_STOPPING_DISTANCE, GET_LAST_KNOWN_SENSOR,
    QUERY_ERROR, // Trains server

	CND_DEST, CND_SENSOR, CND_SWITCH_TIMEOUT, CND_STOP_TIMEOUT, // Conductor
	TRK_RESERVE_PATH, TRK_RESERVE, TRK_SET_ID, TRK_TABLE, // tracksrv
};
