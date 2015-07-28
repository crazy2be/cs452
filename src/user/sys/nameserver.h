#pragma once

// the name server *must* be the first task after
// the first user task
void nameserver(void);

#define WhoIs whois
#define whois(...) ASSERTOK(try_whois(__VA_ARGS__))
int try_whois(const char *name);

#define RegisterAs
void register_as(const char *name);

void nameserver_dump_names(void);
