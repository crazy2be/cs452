#include <kernel.h>
#include <io.h>
#include <assert.h>
#include "nameserver.h"

enum rps_req { CONNECT, MOVE_ROCK, MOVE_PAPER, MOVE_SCISSORS, QUIT };

// there should be an even number of clients
#define MIN_CLIENT_TID 3
#define MAX_CLIENT_TID 6

// pick arbitrary magic numbers to sanity check responses
#define RESP_CONNECTED 17
#define RESP_WON 32
#define RESP_DRAW 76
#define RESP_LOST 92
#define RESP_QUIT 23

char *move_names[] = {
    "rock",
    "paper",
    "scissors"
};

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

    unsigned rounds_to_play = rand() % 20;

    for (unsigned i = 0; i < rounds_to_play; i++) {
        int move = rand() % 3;
        req = MOVE_ROCK + move;
        resp_len = send(server, &req, sizeof(req), &resp, sizeof(resp));
        ASSERT(resp_len == sizeof(resp));

        switch (resp) {
            case RESP_WON:
                printf("Client %d won round %d with %s" EOL, tid(), i, move_names[move]);
                break;
            case RESP_DRAW:
                printf("Client %d drew round %d with %s" EOL, tid(), i, move_names[move]);
                break;
            case RESP_LOST:
                printf("Client %d lost round %d with %s" EOL, tid(), i, move_names[move]);
                break;
            case RESP_QUIT:
                printf("Client %d won round %d by default, now quitting..." EOL, tid(), i);
                return;
            default:
                ASSERT(0 && "Unknown response from the RPS server");
                break;
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

void rps_reply(int tid, unsigned resp) {
    ASSERT(REPLY_SUCCESSFUL == reply(tid, &resp, sizeof(resp)));
}

void rps_quit(int tid, struct rps_client_status *status, unsigned *clients_left) {
    rps_reply(tid, RESP_QUIT);
    status[tid - MIN_CLIENT_TID].status = STATUS_QUIT;
    (*clients_left)--;
}

void rps_server(void) {
    register_as("rps_server");

    int unpaired_tid = -1;
    unsigned clients_left = MAX_CLIENT_TID - MIN_CLIENT_TID + 1;
    struct rps_client_status status[clients_left];

    while (clients_left > 0) {
        int tid;
        enum rps_req req;
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
                rps_reply(tid, RESP_CONNECTED);
                rps_reply(unpaired_tid, RESP_CONNECTED);

                unpaired_tid = -1;
            }
            break;
        case QUIT:
            {
                int partner = status[tid - MIN_CLIENT_TID].partner_tid;
                if (status[partner - MIN_CLIENT_TID].status != STATUS_WAITING) {
                    ASSERT(status[partner - MIN_CLIENT_TID].status != STATUS_QUIT);
                    rps_quit(partner, status, &clients_left);
                }
                rps_quit(tid, status, &clients_left);
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
                    rps_quit(tid, status, &clients_left);
                } else {
                    int move = req;
                    int partner_move = partner_status - STATUS_MOVED_ROCK + MOVE_ROCK;
                    int diff = move - partner_move;
                    if (diff == 0) {
                        rps_reply(tid, RESP_DRAW);
                        rps_reply(partner, RESP_DRAW);
                    } else if (diff == 1 || diff == -2) {
                        rps_reply(tid, RESP_WON);
                        rps_reply(partner, RESP_LOST);
                    } else {
                        rps_reply(tid, RESP_LOST);
                        rps_reply(partner, RESP_WON);
                    }
                    status[partner - MIN_CLIENT_TID].status = STATUS_WAITING;
                }
            }
            break;
        default:
            ASSERT(0 && "Unknown request from the RPS client");
            break;
            break;
        }
    }
    printf("RPS server done" EOL);
}


#include "../kernel/drivers/timer.h"

void init_task(void) {
    for (;;) {
    }
    /* *(volatile unsigned*) 0x10140018 = 0xdeadbeef; */
    /* printf("After interrupt generated" EOL); */
    /* *(volatile unsigned*) 0x10140018 = 0xdeadbeef; */
    /* printf("After interrupt generated" EOL); */
    /* int tid; */
    /* tid = create(PRIORITY_MAX, nameserver); */
    /* printf("Got %d as TID for name server" EOL, tid); */
    /* tid = create(PRIORITY_MIN, rps_server); */
    /* printf("Got %d as TID for rps server" EOL, tid); */
    /* for (int i = MIN_CLIENT_TID; i <= MAX_CLIENT_TID; i++) { */
    /*     tid = create(PRIORITY_MIN, rps_client); */
    /*     printf("Got %d as TID for client %d" EOL, tid, i); */
    /*     ASSERT(i == tid); */
    /* } */
}

#include "benchmark.h"
int main(int argc, char *argv[]) {
    /* boot(benchmark, 0); */
	boot(init_task, 0);
}
