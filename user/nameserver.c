#include "nameserver.h"

// TODO: these deps should not live in the kernel dir
#include "../kernel/hashtable.h"
#include "../kernel/util.h"

#include <kernel.h>

#define NAMESERVER_TID 1

enum request_type { WHOIS, REGISTER_AS };

struct request {
    enum request_type type;
    char name[MAX_KEYLEN + 1];
};

void nameserver(void) {
    struct hashtable name_map;

    hashtable_init(&name_map);

    for (;;) {
        int tid, resp, err;
        struct request req;
        receive(&tid, &req, sizeof(req));

        switch (req.type) {
            case WHOIS:
                err = hashtable_get(&name_map, req.name, &resp);
                if (err != HASHTABLE_SUCCESS) {
                    resp = err;
                }
                break;
            case REGISTER_AS:
                err = hashtable_set(&name_map, req.name, tid);
                if (err != HASHTABLE_SUCCESS) {
                    resp = err;
                }
                break;
            default:
                resp = -1;
                break;
        }

        reply(tid, &resp, sizeof(resp));
    }
}

int whois(const char *name) {
    int success, resp, name_len;
    struct request req;

    name_len = strlen(name);
    if (name_len > MAX_KEYLEN) {
        return -2;
    }

    req.type = WHOIS;
    strcpy(req.name, name);

    // could optimize to only send the used part of the req
    resp = send(NAMESERVER_TID, &req, sizeof(req), &success, sizeof(success));

    if (resp != sizeof(tid)) {
        return resp;
    }

    return success;
}

int register_as(const char *name) {
    int tid, resp, name_len;
    struct request req;

    name_len = strlen(name);
    if (name_len > MAX_KEYLEN) {
        return -2;
    }

    req.type = REGISTER_AS;
    strcpy(req.name, name);
    resp = send(NAMESERVER_TID, &req, sizeof(req), &tid, sizeof(tid));

    if (resp != sizeof(tid)) {
        return resp;
    }

    return tid;
}
