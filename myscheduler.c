#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

//  CITS2002 Project 1 2023
//  Student1:   22976862    Frederick Leman
//  Student2:   ********    Jared Teo

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

int time_quantum;
int device_num = 0;
int command_num = -1;
int total_time = 0;

// Define a structure to hold the devices, their speeds, then declare 4 of them
// Define an array of device structures of length 4
struct devices {
    char name[MAX_DEVICE_NAME];
    int r_speed;
    int w_speed;
} devices[MAX_DEVICES];

struct devices device_array[MAX_DEVICES];

// Define a structure to hold syscalls
// Define a structure to hold the commands, their speeds, then declare 10 of them
// Within a single command struct, there is an array of syscall structs
// Define an array of command structures of length 10

struct syscalls {
    char name[100];
    int exec_time;
    char io_device[MAX_DEVICE_NAME];
    int data_transfer;
};

struct commands {
    char name[MAX_COMMAND_NAME];
    struct syscalls syscall_array[MAX_SYSCALLS_PER_PROCESS];
    int p_id;
} commands[MAX_COMMANDS];

struct commands command_array[MAX_COMMANDS];

// Probably need a few global arrays for the queues

struct commands RUNNING_queue[1]; 
struct commands READY_queue[10]; 
struct commands BLOCKED_queue[10]; 
int quant_timer;

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
    Notes for Jared:
        For the sleep syscall, the amount of time to sleep is held in the "data_transfer" element of the structure
        For the spawn syscall, the process to be spawn is held in the "io_device" element of the structure
*/

void sleep(int exec_time, int sleepy_time, int p_id) {
    printf("@00000000%i\t going to sleep...\n", total_time);
    total_time += 5;
    BLOCKED_queue[0] = RUNNING_queue[0];
}

void enqueue(struct commands command[], int p_id) {
    READY_queue[0] = *command;
    printf("@00000000%i\t p_id%i NEW->READY\n", total_time, p_id);
}

void execute_ready(void) {
    int p_id = 0;
    printf("@00000000%i\t p_id%i READY->RUNNING\n", total_time, p_id);
    printf("@00000000%i\t clock +5\n", total_time);
    total_time += 5;

    RUNNING_queue[0] = READY_queue[0];

    for (int i = 0; i < 10; i++) {
        if (strcmp(RUNNING_queue[0].syscall_array[i].name, "sleep") == 0) {
            sleep(RUNNING_queue[0].syscall_array[i].exec_time, RUNNING_queue[0].syscall_array[i].data_transfer, p_id);
        }
    }
}

void execute_commands(void)
{
    int p_id = 0;
    printf("@00000000%i\t REBOOTING\n", total_time);

    for (int i = 0; i < command_num; i++) {
        command_array[i].p_id = p_id;
        enqueue(&command_array[i], p_id);               // Change this, to add p_id to the command struct instead of being separate
        p_id++;
    }
    execute_ready();
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
    printf("measurements  %i  %i\n", 0, 0);

    exit(EXIT_SUCCESS);
}