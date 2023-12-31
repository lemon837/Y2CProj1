#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

/*  CITS2002 Project 1 2023 */
/*  Student1:   22976862    Frederick Leman */
/*  Student2:   22987324    Jared Teo */
/*  myscheduler (v1.0) */
/*  Compile with:  cc -std=c11 -Wall -Werror -o myscheduler myscheduler.c */

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
#define CHAR_COMMENT                    '#'
#define CHAR_DEVICE                     'd'
#define CHAR_TIME_QUANTUM               't'
#define CHAR_SYSCALL                    '\t'

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
    int time_evoked;                /* Start time of the execution of this syscall on the CPU */
    int time_executing;             /* The amount of time on the CPU executing */
    int rw_time_current;            /* The progress of the current read/write operation */
    int rw_time_max;                /* The expected time taken to finish read/write operation */
    bool completed;
};

struct commands {
    char name[MAX_COMMAND_NAME];
    struct syscalls syscall_array[MAX_SYSCALLS_PER_PROCESS];
    int p_id;
    int time_run;                                       /* Time spent on the cpu, used for time_quantum */
    int children[MAX_RUNNING_PROCESSES];                /* An array holding the p_ids of its children */
    int num_children;
    int time_blocked;                                   /* The time at which a process was moved to BLOCKED_queue */
} commands[MAX_COMMANDS];
struct commands empty_command;
struct commands command_array[MAX_COMMANDS];

struct commands RUNNING_queue[1];
struct commands READY_queue[MAX_RUNNING_PROCESSES];
struct commands BLOCKED_queue[MAX_RUNNING_PROCESSES];
struct commands SLEEPING_queue[MAX_RUNNING_PROCESSES];
struct commands WAIT_queue[MAX_RUNNING_PROCESSES];
int completed_processes[MAX_RUNNING_PROCESSES];         /* An array holding the p_ids of all exited processes, for parental use */

int time_quantum = 100;
int total_time = 0;                                              
int cpu_utilisation = 0;
int p_id = 0;

//  ----------------------------------------------------------------------

void read_sysconfig(char filename[])
{
    int device_num = 0;
    char line[128];

    FILE *file = fopen(filename, "r");              /* Opens the sysconfig file */
    if (file == NULL) {
        printf("There was an error in opening the sysconfigfile\n");
        exit(EXIT_FAILURE);
    }
    while(fgets(line, sizeof line, file) != NULL) {
        if (line[0] == CHAR_COMMENT) {              /* Skips comment lines beginning with '#' */ 
            continue;
        }
        if (line[0] == CHAR_DEVICE) {               /* Identifies device lines beginning with 'd' */
            char *token = strtok(line, " \t");              /* Generates the first token */
            int counter = 0;
            while (token != 0) {
                token[strcspn(token, "\r\n")] = 0;              /* Removes newline and delimiter characters */
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
                token = strtok(0, " \t");               /* Gets the next token in the line */
            }
        device_num++;
        }
        if (line[0] == CHAR_TIME_QUANTUM) {             /* Identifies time_quantum line beginning with 't' */
            char *token = strtok(line, " \t");
            int counter = 0;
            while (token != 0) {
                token[strcspn(token, "\r\n")] = 0;          
                if (counter == 1) {
                    time_quantum = atoi(token);
                }
                token = strtok(0, " \t");
            }
        }
    }
    for (int i = 0; i < 4; i++) {               /* Adds the device structures to an array of device structures */
        device_array[i] = devices[i];
    }
    printf("Devices Found: %i\n", device_num);
    printf("Time Quantum: %d\n", time_quantum);
}

//  ----------------------------------------------------------------------

void read_commands(char filename[])
{
    int command_num = -1;
    char line[128];
    int syscall_num = 0;
    bool next_line_is_name = false;

    FILE *file = fopen(filename, "r");              /* Opens the command file */
    if (file == NULL) {
        printf("There was an error in opening the command file");
        exit(EXIT_FAILURE);
    }
    while(fgets(line, sizeof line, file) != NULL) {
        if (next_line_is_name == true) {                /* If this line was preceded by a "#" line, it is the name of the next command */
            line[strcspn(line, "\r\n")] = 0;
            strcpy(commands[command_num].name, line);
            next_line_is_name = false;
            continue;
        }
        if (line[0] == CHAR_COMMENT) {              /* Skips comment lines beginning with "#" */
            command_num++;                          
            syscall_num = 0;
            next_line_is_name = true;               /* Each command is preceded by a "#", so set true */         
            continue;
        }
        char *token = strtok(line, " \t");              /* Generates the first token */
        int counter = 0;           
        while (token != 0) {
            token[strcspn(token, "\r\n")] = 0;              /* Removes newline and delimiter characters */
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
            token = strtok(0, " \t");               /* Gets the next token in the line */
        }
        syscall_num++;
    }
    for (int i = 0; i < command_num; i++) {
        command_array[i] = commands[i];
    }
    for (int i = 0; i < command_num; i++) {         /* Iterates through the exec_time variables of all commands, */
        int prev_exec_time = 0;                     /* and subtracts the previous exec_time from the current */
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
}

// ----------------------------------------------------------------------

/* Moves all elements of READY_queue down one space */
void shift_ready_queue() {
    for (int i = 0; i < MAX_COMMANDS; i++) {
        READY_queue[i] = READY_queue[i+1];
    }
    READY_queue[MAX_COMMANDS - 1] = empty_command;
}

/* Moves all elements of WAIT_queue down one space */
void shift_wait_queue() {
    for (int i = 0; i < MAX_COMMANDS; i++) {
        WAIT_queue[i] = WAIT_queue[i+1];
    }
    WAIT_queue[MAX_COMMANDS - 1] = empty_command;
}

/* Enqueues the running process into the WAIT_queue */
void enqueue_waiting() {
    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (strcmp(WAIT_queue[i].name, "") == 0) {
            WAIT_queue[i] = RUNNING_queue[0];
            RUNNING_queue[0] = empty_command;
            break;
        }
    }
}

/* Enqueues a process from WAIT_queue into READY_queue */
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
}

/* Enqueues the most desirable process from BLOCKED_queue into READY_queue */
void enqueue_ready_from_blocked() {
    int fastest_read_speed = 0;
    int longest_blocked = 0;
    int fastest_process = 0;
    for (int x = 0; x < MAX_RUNNING_PROCESSES; x++) {               /* Adds all processes tied for the fastest read speed to an array */
        for (int y = 0; y < MAX_SYSCALLS_PER_PROCESS; y++) {
            if (!BLOCKED_queue[x].syscall_array[y].completed) {
                if (BLOCKED_queue[x].syscall_array[y].dev_read_speed >= fastest_read_speed) {
                    fastest_read_speed = BLOCKED_queue[x].syscall_array[y].dev_read_speed;
                }
            }
            break;
        }
    }
    for (int x = 0; x < MAX_RUNNING_PROCESSES; x++) {               /* Iterates through that array, locate the process blocked for longest */
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
    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {               /* Enqueues that process into READY_queue */
        if (strcmp(READY_queue[i].name, "") == 0) {
            READY_queue[i] = BLOCKED_queue[fastest_process];
            printf("@%09d\t p_id%i BLOCKED->READY\n", total_time, READY_queue[i].p_id);
            printf("@%09d\t clock +10\n", total_time);
            total_time += TIME_CORE_STATE_TRANSITIONS;
            break;
        }
    }
    for (int i = fastest_process; i < MAX_COMMANDS - fastest_process; i++) {
        BLOCKED_queue[i] = BLOCKED_queue[i+1];              /* Shifts all elements following the fastest_process down by one */
    }
}

/* Enqueues a process which completed sleeping from SLEEPING_queue into READY_queue */
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
    for (int i = empty_space; i < MAX_COMMANDS - empty_space; i++) {
        SLEEPING_queue[i] = SLEEPING_queue[i+1];            /* Shifts all elements following the empty_space down by one */
    }
}

/* Enqueues the running process into READY_queue */
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
}

/* Enqueues the running process into BLOCKED_queue */
void enqueue_blocked() {
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
}

/* Enqueues the running process into SLEEPING_queue */
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
}

/* Moves a process from READY_queue into RUNNING_queue */
void enqueue_running() {
    RUNNING_queue[0] = READY_queue[0];
    printf("@%09d\t p_id(%i) READY->RUNNING\n", total_time, RUNNING_queue[0].p_id);
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
}

/* Calculates maximum read time based on transfer size and read speed, and assigns dev_read_speed for current io_device */
void running_read(int i) {
    for (int j = 0; j < MAX_DEVICES; j++) {
        if (strcmp(RUNNING_queue[0].syscall_array[i].io_device, device_array[j].name) == 0) {
            float time_max = RUNNING_queue[0].syscall_array[i].data_transfer*1.0 / ((float)device_array[j].r_speed / 1000000.0);
            RUNNING_queue[0].syscall_array[i].rw_time_max = roundf(time_max);
            RUNNING_queue[0].syscall_array[i].dev_read_speed = device_array[j].r_speed;
            printf("@%09d\t BEGINNING READ PROCESS \t TMAX: %2.6f\t p_id(%i)\n", total_time, time_max, RUNNING_queue[0].p_id);
            break;
        }
    }
}

/* Calculates maximum write time based on transfer size and write speed, and assigns dev_read_speed for current io_device */
void running_write(int i) {
    for (int j = 0; j < MAX_DEVICES; j++) {
        if (strcmp(RUNNING_queue[0].syscall_array[i].io_device, device_array[j].name) == 0) {
            float time_max = RUNNING_queue[0].syscall_array[i].data_transfer*1.0 / ((float)device_array[j].w_speed / 1000000.0);
            RUNNING_queue[0].syscall_array[i].rw_time_max = roundf(time_max);
            RUNNING_queue[0].syscall_array[i].dev_read_speed = device_array[j].r_speed;
            printf("@%09d\t BEGINNING WRITE PROCESS \t TMAX: %2.6f\t p_id(%i)\n", total_time, time_max, RUNNING_queue[0].p_id);
            break;
        }
    }
}

/* Spawns a child process */
void spawn_child(int x) {
    int child_id = p_id;
    p_id++;
    int child_num = 0;
    
    RUNNING_queue[0].num_children++;               
    for (int y = 0; y < MAX_COMMANDS; y++) {                    /* Finds the child command in the command_array */
        if (strcmp(RUNNING_queue[0].syscall_array[x].io_device, command_array[y].name) == 0) {
            child_num = y;
            break;
        }
    }
    for (int i = 0; i < MAX_SYSCALLS_PER_PROCESS; i++) {        /* Adds the child's p_id to the parents children array */
        if (RUNNING_queue[0].children[i] == 0) {
            RUNNING_queue[0].children[i] = child_id;
            break;
        }
    }
    printf("@%09d\t moving p_id(%i) RUNNING->READY\n", total_time, RUNNING_queue[0].p_id);
    enqueue_ready_from_running();                               /* Enqueues the parent into READY_queue */
    RUNNING_queue[0] = command_array[child_num];                /* Places the child into the RUNNING_queue */
    RUNNING_queue[0].p_id = child_id;
    printf("@%09d\t p_id(%i) READY->RUNNING\n", total_time, RUNNING_queue[0].p_id);
    printf("@%09d\t clock +5\n", total_time);
    total_time += TIME_CONTEXT_SWITCH;
}

/* Adds an exiting process' p_id to an array, for parental usage */
void complete_process() {
    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (completed_processes[i] == 0) {
            completed_processes[i] = RUNNING_queue[0].p_id;
            break;
        }
    }
    RUNNING_queue[0] = empty_command;
}

/* Checks if all children processes have completed for the waiting parent process */
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
}

//  ----------------------------------------------------------------------

void execute_commands(void) {
    printf("@%09d\t REBOOTING\n", total_time);
    READY_queue[0] = command_array[0];                          /* Enqueues the first command in the file into READY_queue */
    READY_queue[0].p_id = p_id;
    printf("@%09d\t p_id%i NEW->READY\n", total_time, p_id);
    p_id++;
    enqueue_running();                                          /* Then, immediately enqueues it into RUNNING_queue */

    bool should_terminate = false;
    while (total_time < 2000000000 && !should_terminate) {      /* Simulates the passage of time, in microseconds */
        int current = 0;

        if (strcmp(RUNNING_queue[0].name, "") == 0) {
            if (strcmp(READY_queue[0].name, "") == 0) {
                if (strcmp(SLEEPING_queue[0].name, "") == 0 && strcmp(BLOCKED_queue[0].name, "") == 0 && strcmp(WAIT_queue[0].name, "") == 0) {
                    printf("@%09d\t no processes remaining - exiting\n", total_time);               /* If all queues are empty, exit the program */
                    total_time--;
                    should_terminate = true;
                }
                else {                                                                              /* Else, RUNNING_queue and READY_queue are both empty... */
                    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
                        if (strcmp(SLEEPING_queue[i].name, "") != 0) {                              /* Check if any processes sleeping can wake up */
                            for (int j = 0; j < MAX_SYSCALLS_PER_PROCESS; j++) {
                                if (SLEEPING_queue[i].syscall_array[j].data_transfer != 0) {
                                    if (SLEEPING_queue[i].syscall_array[j].data_transfer <= total_time - SLEEPING_queue[i].syscall_array[j].time_evoked) {
                                    printf("@%09d\t p_id(%i) BLOCKED->READY\n", total_time, SLEEPING_queue[i].p_id);
                                    enqueue_ready_from_sleeping(i, j);
                                    }
                                }
                            }
                        break;
                        }
                    }
                    if (strcmp(WAIT_queue[0].name, "") != 0) {              /* Checks if there are any processes in WAIT_queue, if so */
                        check_children();                                   /* checks if all children of a parent have completed */
                    }
                    if (strcmp(BLOCKED_queue[0].name, "") != 0) {           /* Check if there are any processes in BLOCKED_queue, if so */
                        enqueue_ready_from_blocked();                       /* calls a function to find the most favourable process to acquire databus */
                    }
                }
            }
            else {                      /* There is a process in READY_queue, so enqueues next process from READY_queue into RUNNING_queue */
                enqueue_running();
            }
        }
        
        if (strcmp(RUNNING_queue[0].name, "") != 0) {                   /* If there is a process in RUNNING_queue... */
            for (int i = 0; i < MAX_SYSCALLS_PER_PROCESS; i++) {                /* Loop iterates through the syscalls for the running process */
                if (!RUNNING_queue[0].syscall_array[i].completed) {                 /* Find the only first uncompleted syscall, assign it to current */
                    current = i;
                    if (RUNNING_queue[0].syscall_array[current].time_executing == RUNNING_queue[0].syscall_array[current].exec_time) {    /* If execution time has elapsed, handles respective action */
                        cpu_utilisation += RUNNING_queue[0].syscall_array[current].time_executing;
                        if (strcmp(RUNNING_queue[0].syscall_array[current].name, "exit") == 0) {                    /* For the name of the syscall, calls the required function */
                            printf("@%09d\t p_id(%i) RUNNING->EXIT\n", total_time, RUNNING_queue[0].p_id);          /* Break from loop after handling any function */
                            complete_process();
                            break;
                        }
                        else if (strcmp(RUNNING_queue[0].syscall_array[current].name, "sleep") == 0) {
                            printf("@%09d\t p_id(%i) RUNNING->BLOCKED\n", total_time, RUNNING_queue[0].p_id);
                            enqueue_sleeping(current);
                            break;
                        }
                        else if (strcmp(RUNNING_queue[0].syscall_array[current].name, "read") == 0) {
                            running_read(current);
                            RUNNING_queue[0].syscall_array[current].time_executing++;
                            printf("@%09d\t p_id(%i) RUNNING->BLOCKED\n", total_time, RUNNING_queue[0].p_id);
                            enqueue_blocked();
                            break;
                        }
                        else if (strcmp(RUNNING_queue[0].syscall_array[current].name, "write") == 0) {
                            running_write(current);
                            RUNNING_queue[0].syscall_array[current].time_executing++;
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
                            else {              /* If a parent calls wait and has no children, moves it straight to READY_queue */
                                printf("@%09d\t p_id(%i) RUNNING->READY\n", total_time, RUNNING_queue[0].p_id);
                                enqueue_ready_from_running();
                            }
                            break;
                        }
                    }
                    else {
                        RUNNING_queue[0].syscall_array[current].time_executing++;               /* If execution time has not elapsed, increments execution time and continues */
                        break;
                    }
                }
            }
        }

        if (strcmp(RUNNING_queue[0].name, "") != 0 ) {              /* If the process has not yet reached it's execution time, or is performing read/write */
            RUNNING_queue[0].time_run++;                            /* Increments it's time_run, for time_quantum purposes */
            for (int i = 0; i < MAX_SYSCALLS_PER_PROCESS; i++) {
                if (!RUNNING_queue[0].syscall_array[i].completed) {             /* Loop once for only first uncompleted syscall */
                    if (RUNNING_queue[0].syscall_array[i].rw_time_max != 0) {               /* Provided that current syscall is read/write... */
                        if (RUNNING_queue[0].syscall_array[i].rw_time_current == 0) {              /* If it has just begun, increments time to acquire bus */
                            total_time += TIME_ACQUIRE_BUS;
                            printf("@%09d\t FIRST TIME ACQUIRING DATA BUS clock +20\n", total_time);
                        }
                        if (RUNNING_queue[0].syscall_array[i].rw_time_current < RUNNING_queue[0].syscall_array[i].rw_time_max) {    /* Checks if current read/write time has not reached maximum */
                            RUNNING_queue[0].syscall_array[i].rw_time_current++;
                            RUNNING_queue[0].syscall_array[current].time_executing--;
                            break;
                        }
                        if (RUNNING_queue[0].syscall_array[i].rw_time_current >= RUNNING_queue[0].syscall_array[i].rw_time_max) {    /* Checks if current read/write time has reached maximum */
                            RUNNING_queue[0].syscall_array[i].completed = true;
                            printf("@%09d\t FINISHED R/W PROCESS \t MAX: %i\t p_id(%i)\n", total_time, RUNNING_queue[0].syscall_array[i].rw_time_max, RUNNING_queue[0].p_id);
                            RUNNING_queue[0].time_run = 0;              /* Resets the proccess' run time counter */
                            break;
                        }
                    }
                    else {              /* If the current syscall is not read/write... */
                        if (RUNNING_queue[0].time_run >= time_quantum && strcmp(RUNNING_queue[0].name, "") != 0 ) {    /* Checks if the process has run for a time quantum and isn't completed */
                            printf("@%09d\t time run:%i tq expired - moving p_id(%i) RUNNING->READY\n", total_time, RUNNING_queue[0].time_run, RUNNING_queue[0].p_id);
                            RUNNING_queue[0].time_run = 0;
                            enqueue_ready_from_running();               /* Resets the proccess' time_run counter and enqueues it in READY_queue */
                            break;
                        }
                    }
                    break;
                }
            }
        }
    total_time++; 
    }
    float cu;               /* Following the execution, calculate CPU utilisation as a percentage */
    cu = (float)((cpu_utilisation*1.0) / (total_time*1.0)) * 100.0;
    cpu_utilisation = floor(cu);
}

//  ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
    /* Ensure the correct number of command-line arguments have been entered */
    if(argc != 3) {
        printf("Usage: %s sysconfig-file command-file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Read the system configuration file */
    read_sysconfig(argv[1]);

    /* Read the command file */
    read_commands(argv[2]);

    /* Execute commands, starting at first in command-file, until none remain */
    execute_commands();

    /* Print the program's results */
    printf("measurements  %i %i\n", total_time, cpu_utilisation);

    exit(EXIT_SUCCESS);
}