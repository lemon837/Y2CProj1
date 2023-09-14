#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

//  CITS2002 Project 1 2023
//  Student1:   22976862    Frederick Leman
//  Student2:   22987324    Jared Teo

//  myscheduler (v1.0)
//  Compile with:  cc -std=c11 -Wall -Werror -o myscheduler myscheduler.c

#define MAX_DEVICES                     4
#define MAX_DEVICE_NAME                 20
#define MAX_COMMANDS                    10
#define MAX_COMMAND_NAME                20
#define MAX_SYSCALLS_PER_PROCESS        40
#define MAX_RUNNING_PROCESSES           50
#define DEFAULT_TIME_QUANTUM            100
#define TIME_CONTEXT_SWITCH             5
#define TIME_CORE_STATE_TRANSITIONS     10
#define TIME_ACQUIRE_BUS                20

// Some definitions for expected starting characters of various lines in the files
#define CHAR_COMMENT                             '#'
#define CHAR_DEVICE                              'd'
#define CHAR_TIME_QUANTUM                        't'
#define CHAR_SYSCALL                            '\t'

int time_quantum = 0;
int device_num = 0;
int command_num = -1;
int total_time = 0;
int p_id = 0;

struct devices {
    char name[MAX_DEVICE_NAME];
    int r_speed;
    int w_speed;
} devices[MAX_DEVICES];

struct devices device_array[MAX_DEVICES];

struct syscalls {
    char name[100];
    int exec_time;
    char io_device[MAX_DEVICE_NAME];
    int data_transfer;
    int dev_read_speed;
    int time_evoked;
    int time_evoking;
    int rw_time_current;
    int rw_time_max;
    bool completed;
};

struct syscalls current_syscalls[MAX_SYSCALLS_PER_PROCESS];

struct commands {
    char name[MAX_COMMAND_NAME];
    struct syscalls syscall_array[MAX_SYSCALLS_PER_PROCESS];
    int p_id;
    int time_run;
    int children[MAX_RUNNING_PROCESSES];
    int num_children;
    int time_blocked;
} commands[MAX_COMMANDS];

struct commands empty_command;
struct commands command_array[MAX_COMMANDS];

struct commands RUNNING_queue[1];
struct commands READY_queue[MAX_RUNNING_PROCESSES];
struct commands BLOCKED_queue[MAX_RUNNING_PROCESSES];
struct commands SLEEPING_queue[MAX_RUNNING_PROCESSES];
struct commands WAIT_queue[MAX_RUNNING_PROCESSES];

int completed_processes[MAX_RUNNING_PROCESSES];
int cpu_utilisation = 0;

//  ----------------------------------------------------------------------

void read_sysconfig(char filename[])
{
    FILE *file = fopen(filename, "r");                      // Opens the sysconfig file
    if (file == NULL) {
        printf("There was an error in opening the sysconfigfile\n");
        exit(EXIT_FAILURE);
    }
    
    char line[128];

    while(fgets(line, sizeof line, file) != NULL) {
        if (line[0] == CHAR_COMMENT) {                      // Skips comment lines beginning with "#"
            continue;
        }

        if (line[0] == CHAR_DEVICE) {                       // Identifies device lines beginning with "d"
            char *token = strtok(line, " ");                // Generates the first token
            int counter = 0;

            while (token != 0) {
                token[strcspn(token, "\r\n")] = 0;          // Remove newline characters

                if (counter == 1) {
                    strcpy(devices[device_num].name, token);
                }

                if (counter == 2) {
                    devices[device_num].r_speed = atoi(token);
                }

                if (counter == 3) {
                    devices[device_num].w_speed = atoi(token);
                }

                counter++;
                token = strtok(0, " ");                     // Gets the next token in the line
            }

        device_num++;
        }

        if (line[0] == CHAR_TIME_QUANTUM) {
            char *token = strtok(line, " ");                // Creates a token of the time quantum line
            time_quantum = atoi(strtok(0, " "));
        }
    }

    for (int i = 0; i < 4; i++) {                           // Adds the device structures to an array of device structures
        device_array[i] = devices[i];
    }

    printf("Devices Found: %i\n", device_num);
    /*for (int i = 0; i < 4; i++) {                           // To test the function, print all the sysconfig variables
        printf("Name: %s ", devices[i].name);
        printf("R: %i ", devices[i].r_speed);
        printf("W: %i\n", devices[i].w_speed);
    }*/
    printf("Time Quantum: %d\n", time_quantum);
}

//  ----------------------------------------------------------------------

void read_commands(char filename[])
{
    FILE *file = fopen(filename, "r");                      // Opens the command file
    if (file == NULL) {
        printf("There was an error in opening the command file");
        exit(EXIT_FAILURE);
    }
    
    char line[128];
    int syscall_num = 0;
    bool next_line_is_name = false;

    while(fgets(line, sizeof line, file) != NULL) {

        if (next_line_is_name == true) {                                        // If this line was preceded by a "#" line, it is the name of the next command 
            line[strcspn(line, "\r\n")] = 0;
            strcpy(commands[command_num].name, line);
            next_line_is_name = false;
            continue;
        }

        if (line[0] == CHAR_COMMENT) {                      // Skips comment lines beginning with "#"
            command_num++;                                  // Each command is preceded by a "#" so we will go to the next command
            syscall_num = 0;
            next_line_is_name = true;                          
            continue;
        }

        char *token = strtok(line, " ");
        int counter = 0;

        while (token != 0) {
            token[strcspn(token, "\r\n")] = 0;

            if (counter == 0) {
                commands[command_num].syscall_array[syscall_num].exec_time = atoi(token);
            }

            if (counter == 1) {
                strcpy(commands[command_num].syscall_array[syscall_num].name, token);
            }

            if (counter == 2) {
                if (isdigit(token[0])) {
                    commands[command_num].syscall_array[syscall_num].data_transfer = atoi(token);
                    break;
                }
                else {
                    strcpy(commands[command_num].syscall_array[syscall_num].io_device, token);
                }
            }

            if (counter == 3) {
                commands[command_num].syscall_array[syscall_num].data_transfer = atoi(token);
            }

            counter++;
            token = strtok(0, " ");
        }
        syscall_num++;
    }

    for (int i = 0; i < command_num; i++) {
        command_array[i] = commands[i];
    }

    for (int i = 0; i < command_num; i++) {
        int prev_exec_time = 0;
        int current_exec_time = 0;
        for (int j = 0; j < MAX_SYSCALLS_PER_PROCESS; j++) {
            if (command_array[i].syscall_array[j].exec_time != 0) {
                current_exec_time = command_array[i].syscall_array[j].exec_time;
                command_array[i].syscall_array[j].exec_time -= prev_exec_time;
                prev_exec_time = current_exec_time;
            }
        }
    }

    printf("Commands Found: %i\n", command_num);
    for (int i = 0; i < 4; i++) {
        printf("\nCommand Name: %s\n", command_array[i].name);
        for (int j = 0; j < 5; j++) {                                                   // CHANGE THIS J VALUE EVENTUALLY!
            printf("exec_time: %i ", command_array[i].syscall_array[j].exec_time);
            printf("name: %s ", command_array[i].syscall_array[j].name);
            printf("io_device: %s ", command_array[i].syscall_array[j].io_device);
            printf("data_transfer: %i\n", command_array[i].syscall_array[j].data_transfer);
        }
    }
}

//  ----------------------------------------------------------------------

void print_exec_times() {
    for (int i = 0; i < MAX_SYSCALLS_PER_PROCESS; i++) {
        printf("sys[%i]:%i ", i, RUNNING_queue[0].syscall_array[i].exec_time);
    }
}

void shift_ready_queue() {
    for (int i = 0; i < MAX_COMMANDS; i++) {
        READY_queue[i] = READY_queue[i+1];
    }
    READY_queue[MAX_COMMANDS - 1] = empty_command;
    printf("UT:%i\n", cpu_utilisation);
}

void shift_wait_queue() {
    for (int i = 0; i < MAX_COMMANDS; i++) {
        WAIT_queue[i] = WAIT_queue[i+1];
    }
    WAIT_queue[MAX_COMMANDS - 1] = empty_command;
    printf("UT:%i\n", cpu_utilisation);
}

void enqueue_waiting() {
    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (strcmp(WAIT_queue[i].name, "") == 0) {
            WAIT_queue[i] = RUNNING_queue[0];
            RUNNING_queue[0] = empty_command;
            break;
        }
    }
    printf("UT:%i\n", cpu_utilisation);
}

void enqueue_ready_from_waiting() {
    printf("@%09d\t p_id(%i) WAITING->READY\n", total_time, WAIT_queue[0].p_id);
    printf("@%09d\t clock +10\n", total_time);
    total_time += TIME_CORE_STATE_TRANSITIONS;

    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (strcmp(READY_queue[i].name, "") == 0) {
            READY_queue[i] = WAIT_queue[0];
            WAIT_queue[0] = empty_command;
            break;
        }
    }
    shift_wait_queue();
    printf("UT:%i\n", cpu_utilisation);
}

void enqueue_ready_from_blocked() {
    int fastest_read_speed = 0;
    int longest_blocked = 0;
    int fastest_process = 0;
    for (int x = 0; x < MAX_RUNNING_PROCESSES; x++) {
        for (int y = 0; y < MAX_SYSCALLS_PER_PROCESS; y++) {
            if (!BLOCKED_queue[x].syscall_array[y].completed) {
                if (BLOCKED_queue[x].syscall_array[y].dev_read_speed >= fastest_read_speed) {
                    fastest_read_speed = BLOCKED_queue[x].syscall_array[y].dev_read_speed;
                }
            }
            break;
        }
    }
    for (int x = 0; x < MAX_RUNNING_PROCESSES; x++) {
        for (int y = 0; y < MAX_SYSCALLS_PER_PROCESS; y++) {
            if (!BLOCKED_queue[x].syscall_array[y].completed && strcmp(BLOCKED_queue[x].name, "") !=0) {
                if (BLOCKED_queue[x].syscall_array[y].dev_read_speed == fastest_read_speed) {
                    if (total_time - BLOCKED_queue[x].time_blocked > longest_blocked) {
                        longest_blocked = total_time - BLOCKED_queue[x].time_blocked;
                        fastest_process = x;
                    }
                }
            }
            break;
        }
    }      
    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (strcmp(READY_queue[i].name, "") == 0) {
            READY_queue[i] = BLOCKED_queue[fastest_process];
            printf("@%09d\t p_id%i BLOCKED->READY\n", total_time, READY_queue[i].p_id);
            printf("@%09d\t clock +10\n", total_time);
            total_time += TIME_CORE_STATE_TRANSITIONS;
            break;
        }
    }
    for (int i = fastest_process; i < MAX_COMMANDS - 1; i++) {
        BLOCKED_queue[i] = BLOCKED_queue[i+1];
    }
    BLOCKED_queue[MAX_COMMANDS - 1] = empty_command;
    printf("UT:%i\n", cpu_utilisation);
}

void enqueue_ready_from_sleeping(int x, int y) {
    int empty_space;
    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (strcmp(READY_queue[i].name, "") == 0) {
            READY_queue[i] = SLEEPING_queue[x];
            READY_queue[i].syscall_array[y].completed = true;
            SLEEPING_queue[x] = empty_command;
            empty_space = i;
            break;
        }
    }
    printf("@%09d\t clock +10\n", total_time);
    total_time += TIME_CORE_STATE_TRANSITIONS;
    printf("UT:%i\n", cpu_utilisation);

    // Shuffle sleeping queue
    for (int i = empty_space; i < MAX_COMMANDS - empty_space; i++) {
        SLEEPING_queue[i] = SLEEPING_queue[i+1];
    }
}

void enqueue_ready_from_running() {
    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (strcmp(READY_queue[i].name, "") == 0) {
            READY_queue[i] = RUNNING_queue[0];
            RUNNING_queue[0] = empty_command;
            RUNNING_queue[0].time_run = 0;
            break;
        }
    }
    printf("@%09d\t clock +10\n", total_time);
    total_time += TIME_CORE_STATE_TRANSITIONS;
    printf("UT:%i\n", cpu_utilisation);
}

void enqueue_blocked() {
    // Find the next free spot at the end of the blocked queue, put RUNNING there
    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (strcmp(BLOCKED_queue[i].name, "") == 0) {
            BLOCKED_queue[i] = RUNNING_queue[0];
            BLOCKED_queue[i].time_blocked = total_time;
            break;
        }
    }
    printf("@%09d\t clock +10\n", total_time);
    total_time += TIME_CORE_STATE_TRANSITIONS;
    RUNNING_queue[0] = empty_command;
    printf("UT:%i\n", cpu_utilisation);
}

void enqueue_sleeping(int j) {
    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (strcmp(SLEEPING_queue[i].name, "") == 0) {
            SLEEPING_queue[i] = RUNNING_queue[0];
            SLEEPING_queue[i].syscall_array[j].time_evoked = total_time;
            break;
        }
    }
    printf("@%09d\t clock +10\n", total_time);
    total_time += TIME_CORE_STATE_TRANSITIONS;
    RUNNING_queue[0] = empty_command;
    printf("UT:%i\n", cpu_utilisation);
}

void enqueue_running() {
    // Check if the ready queue has anything in it
    // If it does then queue the next item into running
    RUNNING_queue[0] = READY_queue[0];
    printf("@%09d\t p_id%i READY->RUNNING\n", total_time, RUNNING_queue[0].p_id);
    shift_ready_queue();
    total_time += TIME_CONTEXT_SWITCH;
    printf("@%09d\t clock +5\n", total_time);
    RUNNING_queue[0].time_run = 0;
    for (int i = 0; i < MAX_SYSCALLS_PER_PROCESS; i++) {
            if (!RUNNING_queue[0].syscall_array[i].completed) {
                RUNNING_queue[0].syscall_array[i].time_evoked = total_time;
                break;
        }
    }
    printf("UT:%i\n", cpu_utilisation);
}

void running_read(int i) {
    for (int j = 0; j < MAX_DEVICES; j++) {
        if (strcmp(RUNNING_queue[0].syscall_array[i].io_device, device_array[j].name) == 0) {
            printf("io:%s name:%s\n", RUNNING_queue[0].syscall_array[i].io_device, device_array[j].name);
            float time_max = RUNNING_queue[0].syscall_array[i].data_transfer*1.0 / ((float)device_array[j].r_speed / 1000000.0);
            RUNNING_queue[0].syscall_array[i].rw_time_max = roundf(time_max);
            RUNNING_queue[0].syscall_array[i].dev_read_speed = device_array[j].r_speed;
            printf("BEGINNING READ PROCESS \t MAX: %2.6f\t p_id(%i)", time_max, RUNNING_queue[0].p_id);
            printf("DEV_READ_SPEED: %i\n", RUNNING_queue[0].syscall_array[i].dev_read_speed);
            break;
        }
    }
    printf("UT:%i\n", cpu_utilisation);
}

void running_write(int i) {
    for (int j = 0; j < MAX_DEVICES; j++) {
        if (strcmp(RUNNING_queue[0].syscall_array[i].io_device, device_array[j].name) == 0) {
            float time_max = RUNNING_queue[0].syscall_array[i].data_transfer*1.0 / ((float)device_array[j].w_speed / 1000000.0);
            RUNNING_queue[0].syscall_array[i].rw_time_max = roundf(time_max);
            RUNNING_queue[0].syscall_array[i].dev_read_speed = device_array[j].r_speed;
            printf("BEGINNING WRITE PROCESS \t MAX: %2.6f\t p_id(%i)\n", time_max, RUNNING_queue[0].p_id);
            break;
        }
    }
    printf("UT:%i\n", cpu_utilisation);
}

void spawn_child(int x) {
    RUNNING_queue[0].num_children++;
    int child_id = p_id;
    p_id++;
    int child_num = 0;

    for (int y = 0; y < MAX_COMMANDS; y++) {
        if (strcmp(RUNNING_queue[0].syscall_array[x].io_device, command_array[y].name) == 0) {
            child_num = y;
            break;
        }
    }

    for (int i = 0; i < MAX_SYSCALLS_PER_PROCESS; i++) {
        if (RUNNING_queue[0].children[i] == 0) {
            RUNNING_queue[0].children[i] = child_id;
            break;
        }
    }
    printf("@%09d\t moving p_id(%i) RUNNING->READY\n", total_time, RUNNING_queue[0].p_id);
    enqueue_ready_from_running();

    RUNNING_queue[0] = command_array[child_num];
    RUNNING_queue[0].p_id = child_id;
    printf("UT:%i\n", cpu_utilisation);
}

void complete_process() {
    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (completed_processes[i] == 0) {
            completed_processes[i] = RUNNING_queue[0].p_id;
            break;
        }
    }
    RUNNING_queue[0] = empty_command;
    printf("UT:%i\n", cpu_utilisation);
}

void check_children() {    
    int counter = 0;
    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        for (int j = 0; j < MAX_RUNNING_PROCESSES; j++) {
            if (completed_processes[i] != 0 && WAIT_queue[0].children[j] != 0) {
                if (completed_processes[i] == WAIT_queue[0].children[j]) {
                    counter++;
                    break;
                }
            }
        }
    }
    if (counter == WAIT_queue[0].num_children) {
        enqueue_ready_from_waiting();
    }
    printf("UT:%i\n", cpu_utilisation);
}

void spawn_first_proc(int i, char command_name[]) {
    READY_queue[0] = command_array[i];
    READY_queue[0].p_id = p_id;
    printf("@%09d\t p_id%i NEW->READY\n", total_time, p_id);
    p_id++;
    printf("UT:%i\n", cpu_utilisation);
}

//  ----------------------------------------------------------------------

void execute_commands(void) {
    printf("@%09d\t REBOOTING\n", total_time);
    spawn_first_proc(0, commands[0].name);
    enqueue_running();

    bool should_terminate = false;
    while (total_time < 2000000000 && !should_terminate) {
        int current = 0;

        if (strcmp(RUNNING_queue[0].name, "") == 0) {
            if (strcmp(READY_queue[0].name, "") == 0) {
                // If all queues are empty, exit with no processes remaining
                if (strcmp(SLEEPING_queue[0].name, "") == 0 && strcmp(BLOCKED_queue[0].name, "") == 0 && strcmp(WAIT_queue[0].name, "") == 0) {
                    printf("@%09d\t no processes remaining - exiting\n", total_time);
                    total_time--;
                    should_terminate = true;
                }
                else {
                    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
                        if (strcmp(SLEEPING_queue[i].name, "") != 0) {
                            for (int j = 0; j < MAX_SYSCALLS_PER_PROCESS; j++) {
                                if (SLEEPING_queue[i].syscall_array[j].data_transfer != 0 && SLEEPING_queue[i].syscall_array[j].data_transfer <= total_time - SLEEPING_queue[i].syscall_array[j].time_evoked) {
                                    printf("@%09d\t p_id(%i) BLOCKED->READY\n", total_time, SLEEPING_queue[i].p_id);
                                    enqueue_ready_from_sleeping(i, j);
                                }
                            }
                        break;
                        }
                    }
                    if (strcmp(WAIT_queue[0].name, "") != 0) {
                        check_children();
                    }
                    if (strcmp(BLOCKED_queue[0].name, "") != 0) {
                        enqueue_ready_from_blocked();
                    }
                }
            }
            // Otherwise, run the next ready process
            else {
                enqueue_running();
            }
        }

        // If running queue item has reached its exec time - execute its first uncompleted syscall
        if (strcmp(RUNNING_queue[0].name, "") != 0) {
            for (int i = 0; i < MAX_SYSCALLS_PER_PROCESS; i++) {
                if (!RUNNING_queue[0].syscall_array[i].completed && strcmp(RUNNING_queue[0].name, "") != 0) {
                    current = i;
                    if (RUNNING_queue[0].syscall_array[current].time_evoking == RUNNING_queue[0].syscall_array[current].exec_time) {
                        cpu_utilisation += RUNNING_queue[0].syscall_array[current].time_evoking;
                        if (strcmp(RUNNING_queue[0].syscall_array[current].name, "exit") == 0) {
                            printf("@%09d\t p_id(%i) RUNNING->EXIT\n", total_time, RUNNING_queue[0].p_id);
                            complete_process();
                            break;
                        }
                        else if (strcmp(RUNNING_queue[0].syscall_array[current].name, "sleep") == 0) {
                            printf("@%09d\t p_id(%i) RUNNING->BLOCKED\n", total_time, RUNNING_queue[0].p_id);
                            enqueue_sleeping(current);
                            break;
                        }
                        else if (strcmp(RUNNING_queue[0].syscall_array[current].name, "read") == 0) {
                            printf("@%09d\t FIRST TIME ACQUIRING DATA BUS clock +20\n", total_time);
                            total_time += TIME_ACQUIRE_BUS;
                            running_read(current);
                            RUNNING_queue[0].syscall_array[current].time_evoking++;
                            printf("@%09d\t p_id(%i) RUNNING->BLOCKED\n", total_time, RUNNING_queue[0].p_id);
                            enqueue_blocked();
                            break;
                        }
                        else if (strcmp(RUNNING_queue[0].syscall_array[current].name, "write") == 0) {
                            printf("@%09d\t FIRST TIME ACQUIRING DATA BUS clock +20\n", total_time);
                            total_time += TIME_ACQUIRE_BUS;
                            running_write(current);
                            RUNNING_queue[0].syscall_array[current].time_evoking++;
                            printf("@%09d\t p_id(%i) RUNNING->BLOCKED\n", total_time, RUNNING_queue[0].p_id);
                            enqueue_blocked();
                            break;
                        }
                        else if (strcmp(RUNNING_queue[0].syscall_array[current].name, "spawn") == 0) {
                            printf("@%09d\t p_id(%i) SPAWNING A CHILD\n", total_time, RUNNING_queue[0].p_id);
                            RUNNING_queue[0].syscall_array[current].completed = true;
                            spawn_child(current);
                            break;
                        }
                        else if (strcmp(RUNNING_queue[0].syscall_array[current].name, "wait") == 0) {
                            RUNNING_queue[0].syscall_array[current].completed = true;
                            if (RUNNING_queue[0].num_children != 0) {
                                printf("@%09d\t p_id(%i) RUNNING->WAIT\n", total_time, RUNNING_queue[0].p_id);
                                enqueue_waiting();
                            }
                            else {
                                printf("@%09d\t p_id(%i) RUNNING->READY\n", total_time, RUNNING_queue[0].p_id);
                                enqueue_ready_from_running();
                            }
                            break;
                        }
                    }
                    else {
                        RUNNING_queue[0].syscall_array[current].time_evoking++;
                        break;
                    }
                }
            }
        }

        // Check the time_run variable, then check the read-write process completion if necessary
        if (strcmp(RUNNING_queue[0].name, "") != 0 ) {
            RUNNING_queue[0].time_run++;
            for (int i = 0; i < MAX_SYSCALLS_PER_PROCESS; i++) {
                if (!RUNNING_queue[0].syscall_array[i].completed) {
                    if (!RUNNING_queue[0].syscall_array[i].completed && RUNNING_queue[0].syscall_array[i].rw_time_max != 0) {
                        if (RUNNING_queue[0].syscall_array[i].rw_time_current < RUNNING_queue[0].syscall_array[i].rw_time_max) {
                            RUNNING_queue[0].syscall_array[i].rw_time_current++;
                            RUNNING_queue[0].syscall_array[current].time_evoking--;
                            break;
                        }
                        else if (RUNNING_queue[0].syscall_array[i].rw_time_current >= RUNNING_queue[0].syscall_array[i].rw_time_max) {
                            RUNNING_queue[0].syscall_array[i].completed = true;
                            printf("@%09d\t FINISHED R/W PROCESS \t MAX: %i\t p_id(%i)\n", total_time, RUNNING_queue[0].syscall_array[i].rw_time_max, RUNNING_queue[0].p_id);
                            RUNNING_queue[0].time_run = 0;
                            break;
                        }
                        else {
                            // If the running process has exceeded the time quantum, append it to the ready queue
                            if (RUNNING_queue[0].time_run >= time_quantum && strcmp(RUNNING_queue[0].name, "") != 0 ) {
                                printf("@%09d\t 000 - moving p_id(%i) RUNNING->READY\n", total_time, RUNNING_queue[0].p_id);
                                enqueue_ready_from_running();
                                break;
                            }
                        }
                    }
                    else {
                        if (RUNNING_queue[0].time_run >= time_quantum && strcmp(RUNNING_queue[0].name, "") != 0 ) {
                            printf("@%09d\t tq expired - moving p_id(%i) RUNNING->READY\n", total_time, RUNNING_queue[0].p_id);
                            enqueue_ready_from_running();
                            break;
                        }
                    }
                    break;
                }
            }
        }
    /*if (total_time % 5 == 0) {
        printf("%i\n", cpu_utilisation);
    }*/
    total_time++; 
    }
}
//  ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
//  ENSURE THAT WE HAVE THE CORRECT NUMBER OF COMMAND-LINE ARGUMENTS
    if(argc != 3) {
        printf("Usage: %s sysconfig-file command-file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

//  READ THE SYSTEM CONFIGURATION FILE
    read_sysconfig(argv[1]);

//  READ THE COMMAND FILE
    read_commands(argv[2]);

//  EXECUTE COMMANDS, STARTING AT FIRST IN command-file, UNTIL NONE REMAIN
    execute_commands();

//  PRINT THE PROGRAM'S RESULTS
    printf("%i\n", cpu_utilisation);
    float cu;
    cu = (float)((cpu_utilisation*1.0) / (total_time*1.0)) * 100.0;
    cpu_utilisation = ceil(cu);
    printf("measurements  %i %i\n", total_time, cpu_utilisation);

    exit(EXIT_SUCCESS);
}