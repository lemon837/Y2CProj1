#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

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

// Define a structure to hold the devices, their speeds, then declare 4 of them
struct {
    char name[MAX_DEVICE_NAME];
    int r_speed;
    int w_speed;
} devices[MAX_DEVICES];

//  ----------------------------------------------------------------------

void read_sysconfig(char filename[])
{
    FILE *file = fopen(filename, "r");                      // Opens the sysconfig file
    if (file == NULL) {
        printf("There was an error in opening the file");
        exit(EXIT_FAILURE);
    }
    
    char line[128];
    printf("Device List:\n");
    int device_num = 0;

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
            printf("\nTime Quantum: %d\n", time_quantum);
        }
    }

    for (int i = 0; i < 4; i++) {                           // To test the function, print all the sysconfig variables
        printf("Name: %s ", devices[i].name);
        printf("R: %d ", devices[i].r_speed);
        printf("W: %d\n", devices[i].w_speed);
    }
}

void read_commands(char filename[])
{

}

//  ----------------------------------------------------------------------

void execute_commands(void)
{

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