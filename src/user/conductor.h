#pragma once

#define CONDUCTOR_NAME_LEN 20
int conductor(int train_id);
void conductor_get_name(int train_id, char (*buf)[CONDUCTOR_NAME_LEN]);
