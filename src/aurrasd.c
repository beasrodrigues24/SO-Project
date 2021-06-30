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

int task = 1;

/**
 * @brief Struct with information for each filter 
*/
typedef struct filters {

    char * filter_name;
    char * filter_path;
    int usage;
    int max;
    struct filters * next;

}* FILTERS;

/**
 * @brief Adds a filter to the struct of filters
 * @param f Struct to add to
 * @param filter_name Name of the filter
 * @param filter_exec Name of the executable to the filter
 * @param max Maximum of instances of usage of the filter 
 * @param filters_folder Path to the filters folder
*/
void add_filter(FILTERS * f, char * filter_name, char * filter_exec, int max, char * filters_folder) {

    // Find last position of the struct to append
    while (*f) f = &(*f)->next;
    *f = malloc(sizeof(struct filters));
    (*f)->filter_name = filter_name;

    // Creates the path to the executable of the filter
    char * filter_path = malloc(MAX);
    snprintf(filter_path, MAX, "%s/%s", filters_folder, filter_exec);
    (*f)->filter_path = filter_path;

    (*f)->max = max;
    (*f)->usage = 0;
    (*f)->next = NULL;

}

/**
 * @brief Configures the server with the config file and the path to the filters_folder
 * @param config_filename File with the information necessary
 * @param filters_folder Path to the filters folder
 * @returns Struct with the filters struct configured
*/
FILTERS configure(char * config_filename, char * filters_folder) {

    // Open the configuration file
    int fd = open(config_filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    FILTERS f = NULL;
    char * buffer = malloc(MAX);
    char *save; // Necessary for strtok_r usage
    while (read(fd, buffer, MAX) > 0) {
        
        char * filter_name = strtok_r(buffer, " ", &save);  
        while (filter_name) {

            char * filter_executable = strdup(strtok_r(NULL, " ", &save));
            int filter_max = atoi(strtok_r(NULL, "\n", &save));
            // Adds a filter to the struct
            add_filter(&f, strdup(filter_name), filter_executable, filter_max, filters_folder);
            filter_name = strtok_r(NULL, " ", &save);

        }
      
    }

    close(fd);
    free(buffer);

    return f;
}

/**
 * @brief Frees a filters struct
 * @param filters Struct with the filters
*/
void free_filters(FILTERS filters) {

    while (filters) {

        FILTERS tmp = filters;
        filters = filters->next;
        free(tmp->filter_name);
        free(tmp->filter_path);
        free(tmp);

    }

}

/**
 * @brief Counts the number of filters in a string of filters
 * @param filters String with the filters
 * @return Counts the number of filters to apply
*/
int count_filters(char * filters) {

    char * buffer = strdup(filters);

    strtok(buffer, " ");
    int count = 1;

    while (strtok(NULL, " "))
        count++;

    free(buffer);

    return count;

}

/**
 * @brief Returns the executable path from the filter name
 * @param filters Struct with all the filters
 * @param filter Filter name
 * @return Executable path
*/
char * finds_executable(FILTERS filters, char * filter) {

    char * ret = NULL;
    
    for (; !ret && filters; filters = filters->next) {

        // If the name matches, return filter executable path
        if (!strcmp(filter, filters->filter_name)) 
            ret = filters->filter_path;

    }

    return ret;

}

/**
 * @brief Changes a usage of a filter to the struct
 * @param filters Pointer to a struct with the filters
 * @param filter Name or path to the filter
 * @param bool If true, add occurance; if false, remove occurance
*/
void alters_usage(FILTERS * filters, char * filter, int bool) {

    int flag = 1;
    for (; flag && *filters; filters = &(*filters)->next) 
        if (!strcmp(filter, (*filters)->filter_name) || !strcmp(filter, (*filters)->filter_path)) {
            if (bool) (*filters)->usage++;
            else (*filters)->usage--;
            flag = 0;
        }

}

/**
 * @brief Gets if it's valid to add another usage of the filter
 * @param filters Struct with the filters
 * @param filter_exec Executable path of the filter
 * @return 1 if valid, 0 if not valid
*/
int valid_usage(FILTERS filters, char * filter_exec) {

    int ret = -1;
    for (; ret == -1 && filters; filters = filters->next) 
        if (!strcmp(filter_exec, filters->filter_path)) 
            ret = filters->usage < filters->max;
        
    return ret;

}

/**
 * @brief Struct that hold the information about the requests in the server
*/
typedef struct requests {

    char * type_operation;
    char * source_path;
    char * output_path;
    char * filters;
    char * pid_str;
    int task;
    pid_t pid;
    struct requests * next;

} *REQUEST;

/**
 * @brief Adds a request to the requests struct
 * @param r Struct with the requests
 * @param request New string with the request
*/
void add_request(REQUEST * r, char * request) {

    // Appends the request
    while (*r) r = &(*r)->next;
    *r = malloc(sizeof(struct requests));

    // Parses the request
    char * save;
    (*r)->type_operation = strdup(strtok_r(request, ",", &save));
    (*r)->source_path = strdup(strtok_r(NULL, ",", &save));
    (*r)->output_path = strdup(strtok_r(NULL, ",", &save));
    (*r)->filters = strdup(strtok_r(NULL, ",", &save));
    (*r)->pid_str = strdup(strtok_r(NULL, "\n", &save));
    (*r)->task = task++;

    (*r)->next = NULL;
}

/**
 * @brief Frees a struct with requests
 * @param r Struct with the requests
*/
void free_request(REQUEST r) {

    free(r->type_operation);
    free(r->source_path);
    free(r->output_path);
    free(r->filters);
    free(r->pid_str);
    free(r);

}

/**
 * @brief Loads the status of the server to a string
 * @param r Struct with the requests
 * @param f Struct with the filters
 * @return Loaded string 
*/
char * load_status(REQUEST r, FILTERS f) {

    char * buffer = malloc(MAX);

    // Adds pending tasks
    for (; r; r = r->next) {

        snprintf(buffer, MAX, "Task #%d: transform %s %s ", r->task, r->source_path, r->output_path);
        char * aux = strdup(r->filters);
        char * filter = strtok(aux, " ");
        while(filter) {

            snprintf(buffer + strlen(buffer), MAX, "%s ", filter);
            filter = strtok(NULL, " ");

        }
        free(aux);
        snprintf(buffer + strlen(buffer), MAX, "\n");
        
    }

    // Adds information on usage of the filters
    for (; f; f = f->next) 
        snprintf(buffer + strlen(buffer), MAX, "Filter %s : %d/%d (running/max)\n", 
                                                f->filter_name, f->usage, f->max);

    // Adds pid of the server
    snprintf(buffer + strlen(buffer), MAX, "Pid: %d\n", getpid());                                            

    return buffer;

}

/**
 * @brief Function that manages server actions
 * @param argc Number of arguments
 * @param argv Arguments 
 * @return Status
*/
int main(int argc, char * argv[]) {

    if (argc == 3) {

        // Configurates the server according to the config file and filters folder path
        char * config_filename = argv[1];
        char * filters_folder = argv[2];
        FILTERS filters = configure(config_filename, filters_folder);

        // Makes client to server fifo
        if (mkfifo(MAIN_FIFO, 0666) < 0) {
            perror("main fifo");
            exit(1);
        }

        int fd = open(MAIN_FIFO, O_RDONLY);
        if (fd < 0) {
            perror("open");
            exit(1);
        }
        REQUEST req = NULL, head = NULL;
        pid_t pid;
        
        while (1) {

            // Free any requests that have ended
            for (REQUEST * tmp = &head; *tmp;) {

                int status;
                if (waitpid((*tmp)->pid, &status, WNOHANG) != (*tmp)->pid) 
                    tmp = &(*tmp)->next; 
                else {

                    REQUEST req_free = *tmp;
                    *tmp = (*tmp)->next;
                    char * string_filters = strdup(req_free->filters);
                    char * filter_used = strtok(string_filters, " ");
                    while (filter_used) {

                        alters_usage(&filters, filter_used, 0);
                        filter_used = strtok(NULL, " ");

                    }
                    free(string_filters);
                    free(*tmp);

                }

            }

            // Allocates space to instructions and reads file
            char * instruction = malloc(MAX);
            if (read(fd, instruction, MAX) > 0) { 

                if (*instruction == '1') // If it's a transform instruction, add it to the requests
                    add_request(&req, strdup(instruction));
                
                else if (*instruction == '0') { // If it's a status instruction, load the status
                        
                    strtok(instruction, ",");
                    char * client_path = strtok(NULL, ",");
                    char * server_status = load_status(head, filters);
                    // Open the server to client fifo to write in
                    int client_fd = open(client_path, O_WRONLY, 0644);
                    if (client_fd < 0) {
                        perror("open client fifo");
                        exit(1);
                    }
                    // Writes the server status
                    write(client_fd, server_status, strlen(server_status));
                    free(server_status);
                    
                }

                free(instruction);
            }
            else {
                
                close(fd);
                fd = open(MAIN_FIFO, O_RDONLY);
                if (fd < 0) {
                    perror("open");
                    exit(1);
                }

            }
       
            while (req) { // Executes requests in line

                // Opens server to client fifo
                int fd_client = open(req->pid_str, O_WRONLY);
                if (fd_client < 0) {
                    perror("open");
                    exit(1);
                }

                // Counts the number of filters in the request
                int n_filters = count_filters(req->filters);

                // Informs the client that the request is processing
                write(fd_client, "1", 1);

                // Opens source file
                int fd_source = open(req->source_path, O_RDONLY);
                if (fd_source < 0) {
                     perror("open source");
                    _exit(1);
                }

                // Opens output file
                int fd_output = open(req->output_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
                if (fd_output < 0) {
                    perror("open");
                    _exit(1);
                }

                char * save;
                char * all_filters = strdup(req->filters);
                char * filter = strtok_r(all_filters, " ", &save);
                // Loads executable paths to the array and does active waiting if one of the filters isn't available
                char * filters_array[n_filters];
                for (int i = 0; i < n_filters; i++) {

                    char * path = finds_executable(filters, filter);
                    filters_array[i] = path;
                    while (valid_usage(filters, path) == 0);
                    filter = strtok_r(NULL, " ", &save);

                }

                // Adds a usage of the filter
                for (int i = 0; i < n_filters; i++) 
                    alters_usage(&filters, filters_array[i], 1);

                free(all_filters);

                if ((pid = fork()) == 0) {

                    // If it's just one filter, there's no need for using pipes to redirect input and output
                    if (n_filters == 1) {
                       
                        // Creates a new process do execute the filter
                        if (fork() == 0) {

                            // dup2 redirects input and output
                            dup2(fd_source, STDIN_FILENO);
                            close(fd_source);
                            dup2(fd_output, STDOUT_FILENO);
                            close(fd_output);
                            // Executes filter
                            execlp(filters_array[0], filters_array[0], NULL);
                            perror("execlp one filter");
                            _exit(1);

                        }

                    }
                    else { // Pipes are necessary

                        int pipes[n_filters - 1][2];

                        // Creates the first pipe
                        if (pipe(pipes[0]) < 0) {
                            perror("pipe");
                            _exit(1);
                        }

                        // Creates another process
                        if (fork() == 0) {

                            // Duplicates fd_source to STDIN_FILENO
                            dup2(fd_source, STDIN_FILENO);
                            close(fd_source);

                            dup2(pipes[0][1], STDOUT_FILENO);
                            close(pipes[0][1]);

                            // Execute the corresponding filter
                            execlp(filters_array[0], filters_array[0], NULL);
                            perror("execlp");
                            _exit(1);

                        }

                        close(pipes[0][1]);

                        // Executes intermediary filters
                        int i;
                        for (i = 1; i < n_filters - 1; i++) {

                            if (pipe(pipes[i]) < 0) {
                                perror("pipe");
                                exit(1);
                            }

                            if (fork() == 0) {

                                close(pipes[i][0]);

                                dup2(pipes[i-1][0], STDIN_FILENO);
                                close(pipes[i-1][0]);

                                dup2(pipes[i][1], STDOUT_FILENO);
                                close(pipes[i][1]);

                                execlp(filters_array[i], filters_array[i], NULL);
                                perror("execlp");
                                _exit(1);

                            }

                            close(pipes[i-1][0]);
                            close(pipes[i][1]);

                        }

                        // Executes last filter and places it in the output 
                        if (fork() == 0) {

                            dup2(pipes[i-1][0], STDIN_FILENO);
                            close(pipes[i-1][0]);

                            dup2(fd_output, STDOUT_FILENO);
                            close(fd_output);

                            execlp(filters_array[i], filters_array[i], NULL);
                            perror("execlp");
                            _exit(1);

                        }

                        close(pipes[i-1][0]);

                    }

                    // Collects the new processes created
                    for (int j = 0; j < n_filters; j++) 
                        wait(NULL);
                    
                    // Writes that the request was finalized
                    write(fd_client, "0", 1);
                    close(fd_client);

                    _exit(1);
                }
                else {
                    
                    req->pid = pid;
                    if (!head) head = req;
                    req = req->next;
                    
                }
            }

        }

        // Frees filters
        free_filters(filters);

    }
    else {
        perror("configure");
        exit(1);
    }


    return 0;
    
}