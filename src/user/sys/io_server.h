#pragma once

#include <io.h>

// do all of the initialization needed to start an IO server with the given parameters
// (because we need to pass data in, we need to do some message passing in addition
// to just creating the task)
void ioserver(const int channel);
void ioserver_stop(const int channel);
