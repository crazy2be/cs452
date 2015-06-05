#pragma once

// the name server *must* be the first task after
// the first user task
void nameserver(void);

#define WhoIs whois
int whois(const char *name);

#define RegisterAs
int register_as(const char *name);
