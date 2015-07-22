#pragma once

// fire and forget send, non-blocking
void send_async(int tid, void *msg, unsigned msglen);
