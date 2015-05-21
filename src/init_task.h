#pragma once

/**
 * @file
 *
 * The `init_task()` and `init_task_priority` definitions are intended to allow
 * easily building the kernel with swappable user tasks.
 * This permits building both a real program which performs the tasks laid out
 * in the assignment specification, as well as a test program which starts
 * the kernel, and does some stress testing.
 *
 * In the build script, depending on which program is being built, different
 * definitions of `init_task()` and `init_task_priority` are linked in.
 *
 * The TID assigned to the init task is always 0.
 * The parent TID of the init task is also 0.
 * This is special, in that the init task is the only task
 * for which `tid() == parent_tid()`.
 */

/**
 * This function is started as the first user task, and is run
 * after the kernel is done initialization.
 *
 * Typically, the responsibility of the init task is to start up
 * any other tasks which are needed in order to run the program,
 * and then exit.
 */
void init_task(void);

/**
 * The init task will be started with the priority specified here.
 */
extern const int init_task_priority;
