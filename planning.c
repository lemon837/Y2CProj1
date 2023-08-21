/* The goal of this project is to implement a program, named myscheduler, to emulate the scheduling of
processes on a single-CPU, multi-device system, employing a pre-emptive process scheduler */

/* 
A Few Details:
1. The CPU is connected to a number of I/O devices of differing speeds, using a single high-speed data bus
2. Only one process can use the data bus at any one time
3. Consider a simplified OS in which only a single process occupies the single CPU at any one time 
*/

/* 
Only a single process can access each I/O device at any one time

If the data bus is in use and a second process also needs to access the data bus, the second process must
be queued until the current transfer is complete

When a data transfer completes, all waiting (queued) processes are considered to determine which process
can next acquire the data bus. If multiple processes are waiting, the one which has been waiting longest, has
the highest priorty to next acquire the data bus
*/

/*
The result is to support MULTIPLE BLOCKED QUEUES
*/

#include <stdio.h>
