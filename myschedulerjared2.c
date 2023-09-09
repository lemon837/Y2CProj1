#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

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

//  NOTE THAT DEVICE DATA-TRANSFER-RATES ARE MEASURED IN BYTES/SECOND,
//  THAT ALL TIMES ARE MEASURED IN MICROSECONDS (usecs),
//  AND THAT THE TOTAL-PROCESS-COMPLETION-TIME WILL NOT EXCEED 2000 SECONDS
//  (SO YOU CAN SAFELY USE 'STANDARD' 32-BIT ints TO STORE TIMES).

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
    int time_evoked;
    bool completed;
};

struct syscalls current_syscalls[MAX_SYSCALLS_PER_PROCESS];

struct commands {
    char name[MAX_COMMAND_NAME];
    struct syscalls syscall_array[MAX_SYSCALLS_PER_PROCESS];
    int p_id;
    int time_evoked;
    int time_run;
} commands[MAX_COMMANDS];

struct commands empty_command;

struct commands command_array[MAX_COMMANDS];

struct commands RUNNING_queue[1];
struct commands READY_queue[MAX_RUNNING_PROCESSES];
struct commands BLOCKED_queue[MAX_RUNNING_PROCESSES];
struct commands SLEEPING_queue[MAX_RUNNING_PROCESSES];

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
            time_quantum = atoi(strtok(0, " "));            // Iterates once to the second token, assigns to global variable
        }
    }

    for (int i = 0; i < 4; i++) {                           // Adds the device structures to an array of device structures
        device_array[i] = devices[i];
    }

    printf("Devices Found: %i\n", device_num);
    /*for (int i = 0; i < 4; i++) {                           // To test the function, print all the sysconfig variables
        printf("Name: %s ", devices[i].name);
        printf("R: %d ", devices[i].r_speed);
        printf("W: %d\n", devices[i].w_speed);
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

        if (next_line_is_name == true) {                       // If this line was preceded by a "#" line, it is the name of
            strcpy(commands[command_num].name, line);          // the next command
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

    printf("Commands Found: %i\n", command_num);
    /*for (int i = 0; i < 5; i++) {
        printf("\nCommand Name: %s\n", command_array[i].name);
        for (int j = 0; j < 5; j++) {                                                   // CHANGE THIS J VALUE EVENTUALLY!
            printf("exec_time: %i ", command_array[i].syscall_array[j].exec_time);
            printf("name: %s ", command_array[i].syscall_array[j].name);
            printf("io_device: %s ", command_array[i].syscall_array[j].io_device);
            printf("data_transfer: %i\n", command_array[i].syscall_array[j].data_transfer);
        }
    }*/
}

//  ----------------------------------------------------------------------

/*
    Notes:
        For the sleep syscall, the amount of time to sleep is held in the "data_transfer" element of the structure
        For the spawn syscall, the process to be spawn is held in the "io_device" element of the structure
*/

void shift_ready_queue() {
    for (int i = 0; i < MAX_COMMANDS; i++) {
        READY_queue[i] = READY_queue[i+1];
    }
    READY_queue[MAX_COMMANDS - 1] = empty_command;
}

void shift_sleeping_queue() {
    for (int i = 0; i < MAX_COMMANDS; i++) {
        SLEEPING_queue[i] = SLEEPING_queue[i+1];
    }
    SLEEPING_queue[MAX_COMMANDS -1] = empty_command;
}

void enqueue_ready_from_sleeping(int j) {
    for(int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (strcmp(READY_queue[i].name, "") == 0) {
            READY_queue[i] = SLEEPING_queue[j];
            READY_queue[i].syscall_array[0].completed = true;
            SLEEPING_queue[j] = empty_command;
            break;
        }
    }
    printf("@%09d\t clock +10\n", total_time);
    total_time += 10;
    shift_sleeping_queue();
}

void enqueue_ready_from_running() {
    for(int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (strcmp(READY_queue[i].name, "") == 0) {
            READY_queue[i] = RUNNING_queue[0];
            READY_queue[i].syscall_array[0].completed = true;
            RUNNING_queue[0] = empty_command;
            RUNNING_queue[0].time_run = 0;
            break;
        }
    }
    printf("@%09d\t clock +10\n", total_time);
    total_time += 10;
}

void enqueue_blocked(int sleep_time) {
    // Find the next free spot at the end of the blocked queue, put RUNNING there
    for(int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (strcmp(BLOCKED_queue[i].name, "") == 0) {
            BLOCKED_queue[i] = RUNNING_queue[0];
            break;
        }
    }
    printf("@%09d\t clock +10\n", total_time);
    total_time += 10;
    RUNNING_queue[0] = empty_command;
}

void enqueue_sleeping() {
    for(int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
        if (strcmp(SLEEPING_queue[i].name, "") == 0) {
            SLEEPING_queue[i] = RUNNING_queue[0];
            SLEEPING_queue[i].syscall_array[0].time_evoked = total_time;
            break;
        }
    }
    printf("@%09d\t clock +10\n", total_time);
    total_time += 10;
    RUNNING_queue[0] = empty_command;
}

int enqueue_running() {
    // Check if the ready queue has anything in it
    // If it does then queue the next item into running
    if (strcmp(READY_queue[0].name, "") != 0) {
        RUNNING_queue[0] = READY_queue[0];
        printf("@%09d\t p_id%i READY->RUNNING\n", total_time, RUNNING_queue[0].p_id);
        shift_ready_queue();
        int time = 5;
        printf("@%09d\t clock +%i\n", total_time, time);
        RUNNING_queue[0].time_run = 0;
        return time + total_time;
    }
    return total_time;
}

void spawn_proc(int i, char command_name[]) {
    READY_queue[0] = command_array[i];
    READY_queue[0].p_id = p_id;
    printf("@%09d\t p_id%i NEW->READY\n", total_time, p_id);
    p_id++;
}

void execute_commands(void) {
    printf("@%09d\t REBOOTING\n", total_time);
    spawn_proc(0, commands[0].name);
    total_time = enqueue_running();
    int com_run = 0;
    com_run++;

    // empty_command.time_run = 100000000;
    bool should_terminate = false;

    while (total_time < 20000000 && !should_terminate) {

        // If running queue is empty and commands remain in file
        if (strcmp(RUNNING_queue[0].name, "") == 0 && com_run < command_num) {
            spawn_proc(com_run, commands[com_run].name);
            total_time = enqueue_running();
            com_run++;
        }
        
        // If running queue is empty
        if (strcmp(RUNNING_queue[0].name, "") == 0) {
            if (strcmp(READY_queue[0].name, "") != 0) {
                printf("%s\n", READY_queue[0].name);
                total_time = enqueue_running();
            }
            else {
                if (strcmp(SLEEPING_queue[0].name, "") == 0) {
                    printf("@%09d\t no processes remaining - exiting\n", total_time);
                    should_terminate = true;
                }
                else {
                    for (int i = 0; i < MAX_RUNNING_PROCESSES; i++) {
                        if (strcmp(SLEEPING_queue[i].name, "") != 0 && SLEEPING_queue[i].syscall_array[0].data_transfer <= total_time - SLEEPING_queue[i].syscall_array[0].time_evoked) {
                            printf("@%09d\t p_id(%i) BLOCKED->READY\n", total_time, SLEEPING_queue[i].p_id);
                            enqueue_ready_from_sleeping(i);
                        }
                    }
                }
            }
        }

        int i;

        // If running queue item has reached its exec time - execute its first uncompleted syscall 
        if (strcmp(RUNNING_queue[0].name, "") != 0) {
            for (int i = 0; i < MAX_SYSCALLS_PER_PROCESS; i++) {
                if (!RUNNING_queue[0].syscall_array[i].completed) {
                    // Check if the syscall is ready to be executed
                    if (total_time >= RUNNING_queue[0].syscall_array[i].time_evoked + RUNNING_queue[0].syscall_array[i].exec_time) {
                        if (strcmp(RUNNING_queue[0].syscall_array[i].name, "exit") == 0) {
                            if (strcmp(RUNNING_queue[0].name, "shortsleep") == 0 || strcmp(RUNNING_queue[0].name, "longsleep") == 0) {
                                RUNNING_queue[0].syscall_array[i].time_evoked += 50; // Add 50 microseconds for exit syscall for shortsleep and longsleep
                            }
                            printf("@%09d\t p_id(%i) RUNNING->EXIT\n", total_time, RUNNING_queue[0].p_id);
                            RUNNING_queue[0] = empty_command;
                            break; // Exit the loop after handling the exit syscall
                        } else if (strcmp(RUNNING_queue[0].syscall_array[i].name, "sleep") == 0) {
                            printf("@%09d\t p_id(%i) RUNNING->BLOCKED\n", total_time, RUNNING_queue[0].p_id);
                            enqueue_sleeping();
                            break; // Exit the loop after handling the sleep syscall
                        }
                    }
                }
            }
            if (!(strcmp(RUNNING_queue[0].name, "shortsleep") == 0 || strcmp(RUNNING_queue[0].name, "longsleep") == 0)) {
                RUNNING_queue[0].syscall_array[i].time_evoked += RUNNING_queue[0].syscall_array[i].exec_time;
            }
        
        }
        
        if (RUNNING_queue[0].time_run < time_quantum && strcmp(RUNNING_queue[0].name, "") != 0 ) {
            printf("@%09d\t running p_id(%i)\n", total_time, RUNNING_queue[0].p_id);
            RUNNING_queue[0].time_run++;
        }

        if (RUNNING_queue[0].time_run >= time_quantum && strcmp(RUNNING_queue[0].name, "") != 0 ) {
            printf("@%09d\t tq expired - moving p_id(%i) RUNNING->READY\n", total_time, RUNNING_queue[0].p_id);
            enqueue_ready_from_running();
        }

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
    printf("measurements  %i  %i\n", total_time, 0);

    exit(EXIT_SUCCESS);
}