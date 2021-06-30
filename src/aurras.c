#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAIN_FIFO "tmp/main_fifo"
#define MAX 1024

/**
 * @brief Creates an expression for a transform instruction, to send to the server
 * @param argv Relevant arguments, like source file, output file and filters
 * @param argc Number of arguments
 * @param pid Pid of the client
 * @return String with the built expression
*/
char * build_expression(char * argv[], int argc, char * pid) {

    char * buffer = malloc(MAX);
    // Type of instruction, input, output,
    snprintf(buffer, MAX, "1,%s,%s,", argv[0], argv[1]);

    // Loop to write the filters separated by " "
    for (int i = 2; i < argc; i++) {
        snprintf(buffer + strlen(buffer), MAX, "%s", argv[i]);
        if (i != argc - 1) snprintf(buffer + strlen(buffer), MAX, " ");
    }

    // Loop to write the pid of the client
    snprintf(buffer + strlen(buffer), MAX, ",%s\n", pid);

    return buffer;
}

/**
 * @brief Function that manages the cases of client actions
 * @param argc Number of arguments
 * @param argv Arguments 
 * @return Status
*/
int main(int argc, char * argv[]) {

    if (argc == 1) { // Case to give guide to user

        char * buffer = malloc(MAX);
        sprintf(buffer, "./aurras status\n./aurras transform input-filename output-filename filter-id-1 filter-id-2 ...\n");
        write(STDOUT_FILENO, buffer, strlen(buffer));
        free(buffer);

    }
    else if (argc == 2 && !strcmp(argv[1], "status")) { // Case to give status on the server

        // Gets pids of the client
        pid_t pid = getpid();
        char * pid_str = malloc(20); 
        snprintf(pid_str, 20, "tmp/%d", pid);
        
        // Creates fifo with the name of the pid in tmp directory
        if (mkfifo(pid_str, 0666) < 0) {
            perror("mkfifo");
            exit(1);
        }

        // Open the client to server fifo
        int fd = open(MAIN_FIFO, O_WRONLY, 0644);
        if (fd < 0) {
            perror("open main fifo");
            exit(1);
        }

        // Sends 0 <=> status and the path to the server to client fifo
        char * buffer = malloc(MAX);
        snprintf(buffer, MAX, "0,tmp/%d", pid);
        write(fd, buffer, strlen(buffer));
        close(fd);

        // Opens server to client fifo
        fd = open(pid_str, O_RDONLY);
        if (fd < 0) {
            perror("open client fifo");
            exit(1);
        }

        free(pid_str);

        // Attempts to read the server to client fifo until it receives the status info and writes it to stdout
        int flag = 1;
        while(flag) {
            ssize_t b_read;
            if ((b_read = read(fd, buffer, MAX)) > 0) { // Talvez mudar isto para abrir denovo caso nao leia para nao rebentar
                write(STDOUT_FILENO, buffer, b_read);
                flag = 0;
            }

        }

        close(fd);

    }
    else if (argc > 4 && !strcmp(argv[1], "transform")) { // Cases of transformation requests

            // Create a server to client fifo with the client pid as name
            pid_t pid = getpid();
            char * pid_str = malloc(MAX);
            snprintf(pid_str, MAX, "tmp/");
            snprintf(pid_str + strlen(pid_str), MAX, "%d", pid);
            if (mkfifo(pid_str, 0666) < 0) {
                perror("mkfifo");
                exit(1);
            }
        
            // Open client to server fifo 
            int fd = open(MAIN_FIFO, O_WRONLY, 0644);
            if (fd < 0) {
                perror("open");
                exit(1);
            }

            // Writes the expression to the client to server fifo
            char * instruction = build_expression(argv + 2, argc - 2, pid_str);
            write(fd, instruction, strlen(instruction));
            close(fd);

            // Writes that client is pending 
            write(STDOUT_FILENO, "Pending\n", 9);

            // Open server to client fifo again
            fd = open(pid_str, O_RDONLY);
            if (fd < 0) {
                perror("open");
                exit(1);
            }
            free(pid_str);

            // Read it until server sends message that it's ready
            char c = ' '; 
            int first = 1;
            while (c != '0') {
                
                // Informs that the server picked up the request 
                if (first && c == '1') {
                    first = 0;
                    write(STDOUT_FILENO, "Processing\n", 12);
                }

                // Attempts to read again
                read(fd, &c, 1); // FAZER VERIFICAÇAO DO READ E REABRIR?

            }

            close(fd);

    }
    else {

        perror("Invalid commands");
        exit(1);
        
    }

    return 0;

}