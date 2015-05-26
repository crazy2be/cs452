#include <kernel.h>
#include <io.h>
#include <assert.h>
#include "nameserver.h"

enum rps_req { CONNECT, MOVE_ROCK, MOVE_PAPER, MOVE_SCISSORS, QUIT };

// there should be an even number of clients
#define MIN_CLIENT_TID 3
#define MAX_CLIENT_TID 4

// pick arbitrary magic numbers to sanity check responses
#define RESP_CONNECTED 17
#define RESP_WON 32
#define RESP_DRAW 76
#define RESP_LOST 92
#define RESP_QUIT 23

void rps_client(void) {
    int server = whois("rps_server");
    enum rps_req req;
    unsigned resp;
    int resp_len;

    req = CONNECT;
    resp_len = send(server, &req, sizeof(req), &resp, sizeof(resp));
    ASSERT(resp_len == sizeof(resp));
    ASSERT(resp == RESP_CONNECTED);
    printf("Client %d connected" EOL, tid());

    unsigned rounds_to_play = rand() % 10;

    for (unsigned i = 0; i < rounds_to_play; i++) {
        req = MOVE_ROCK + rand() % 3;
        resp_len = send(server, &req, sizeof(req), &resp, sizeof(resp));
        ASSERT(resp_len == sizeof(resp));

        switch (resp) {
            case RESP_WON:
                printf("Client %d won round %d" EOL, tid(), i);
                break;
            case RESP_DRAW:
                printf("Client %d drew round %d" EOL, tid(), i);
                break;
            case RESP_LOST:
                printf("Client %d lost round %d" EOL, tid(), i);
                break;
            case RESP_QUIT:
                printf("Client %d won round %d by default, now quitting..." EOL, tid(), i);
                return;
        }
    }

    req = QUIT;
    resp_len = send(server, &req, sizeof(req), &resp, sizeof(resp));
    ASSERT(resp_len == sizeof(resp));
    ASSERT(resp == RESP_QUIT);
    printf("Client %d quit" EOL, tid());
}

#define STATUS_WAITING 0
#define STATUS_MOVED_ROCK 1
#define STATUS_MOVED_PAPER 2
#define STATUS_MOVED_SCISSORS 3
#define STATUS_QUIT 4

struct rps_client_status {
    int partner_tid;
    int status;
};

void rps_server(void) {
    register_as("rps_server");

    int unpaired_tid = -1;
    struct rps_client_status status[MAX_CLIENT_TID - MIN_CLIENT_TID + 1];

    for (;;) {
        int tid;
        enum rps_req req;
        unsigned resp;
        int req_len = receive(&tid, &req, sizeof(req));
        ASSERT(req_len == sizeof(req));

        switch (req) {
        case CONNECT:
            if (unpaired_tid < 0) {
                unpaired_tid = tid;
            } else {
                // there is a pair ready
                status[tid - MIN_CLIENT_TID].partner_tid = unpaired_tid;
                status[tid - MIN_CLIENT_TID].status = STATUS_WAITING;
                status[unpaired_tid - MIN_CLIENT_TID].partner_tid = tid;
                status[unpaired_tid - MIN_CLIENT_TID].status = STATUS_WAITING;

                // tell clients the server is ready to respond
                resp = RESP_CONNECTED;
                reply(tid, &resp, sizeof(resp));
                reply(unpaired_tid, &resp, sizeof(resp));

                unpaired_tid = -1;
            }
            break;
        case QUIT:
            {
                int partner = status[tid - MIN_CLIENT_TID].partner_tid;
                resp = RESP_QUIT;
                if (status[partner - MIN_CLIENT_TID].status != STATUS_WAITING) {
                    ASSERT(status[partner - MIN_CLIENT_TID].status != STATUS_QUIT);
                    reply(partner, &resp, sizeof(resp));
                    status[partner - MIN_CLIENT_TID].status = STATUS_QUIT;
                }
                status[tid - MIN_CLIENT_TID].status = STATUS_QUIT;
                reply(tid, &resp, sizeof(resp));
            }
            break;
        case MOVE_ROCK:
        case MOVE_PAPER:
        case MOVE_SCISSORS:
            {
                int partner = status[tid - MIN_CLIENT_TID].partner_tid;
                int partner_status = status[partner - MIN_CLIENT_TID].status;
                if (partner_status == STATUS_WAITING) {
                    // store move in array, and wait for partner to respond
                    status[tid - MIN_CLIENT_TID].status = STATUS_MOVED_ROCK + (req - MOVE_ROCK);
                } else if (partner_status == STATUS_QUIT) {
                    resp = RESP_QUIT;
                    reply(tid, &resp, sizeof(resp));
                    status[tid - MIN_CLIENT_TID].status = STATUS_QUIT;
                } else {
                    int move = req;
                    int partner_move = partner_status - STATUS_MOVED_ROCK + MOVE_ROCK;
                    unsigned int partner_resp;
                    if (move == partner_move) {
                        resp = partner_resp = RESP_DRAW;
                    } else if (move > partner_move && move != MOVE_ROCK) {
                        resp = RESP_WON;
                        partner_resp = RESP_LOST;
                    } else {
                        resp = RESP_LOST;
                        partner_resp = RESP_WON;
                    }
                    status[partner - MIN_CLIENT_TID].status = STATUS_WAITING;
                    reply(tid, &resp, sizeof(resp));
                    reply(partner, &partner_resp, sizeof(partner_resp));
                }
            }
            break;
        default:
            // whatever, dude...
            break;
        }
    }
    ASSERT(0 && "SERVER SHOULD NEVER QUIT");
}


void init_task(void) {
    create(PRIORITY_MAX, nameserver);
    create(PRIORITY_MIN - 1, rps_server);
    for (int i = MIN_CLIENT_TID; i <= MAX_CLIENT_TID; i++) {
        int tid = create(PRIORITY_MIN - 1, rps_client);
        ASSERT(i == tid);
    }
}

int main(int argc, char *argv[]) {
    boot(init_task, 15);
}
