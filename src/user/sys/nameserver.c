#include "nameserver.h"

#include <hashtable.h>
#include <util.h>
#include "../request_type.h"

#include <kernel.h>

#define NAMESERVER_TID 2

struct nameserver_request {
	enum request_type type;
	char name[MAX_KEYLEN + 1];
};

void nameserver(void) {
	struct hashtable name_map;
	hashtable_init(&name_map);

	for (;;) {
		int tid = -1, resp = 0, err = -10;
		struct nameserver_request req;
		receive(&tid, &req, sizeof(req));

		switch (req.type) {
		case WHOIS:
			err = hashtable_get(&name_map, req.name, &resp);
			break;
		case REGISTER_AS:
			err = hashtable_set(&name_map, req.name, tid);
			break;
		default:
			WTF("Nameserver got unknown request %d"EOL, req.type);
			break;
		}
		if (err < 0) resp = err;
		reply(tid, &resp, sizeof(resp));
	}
}

int try_whois(const char *name) {
	struct nameserver_request req;
	req.type = WHOIS;
	ASSERT(strlen(name) <= MAX_KEYLEN);
	strcpy(req.name, name);

	int tid = -5;
	// could optimize to only send the used part of the req
	send(NAMESERVER_TID, &req, sizeof(req), &tid, sizeof(tid));
	return tid;
}

void register_as(const char *name) {
	int rpy = -1, name_len = -1;
	struct nameserver_request req;

	name_len = strlen(name);
	ASSERT(name_len <= MAX_KEYLEN);

	req.type = REGISTER_AS;
	strcpy(req.name, name);
	send(NAMESERVER_TID, &req, sizeof(req), &rpy, sizeof(rpy));
	ASSERTOK(rpy);
}
