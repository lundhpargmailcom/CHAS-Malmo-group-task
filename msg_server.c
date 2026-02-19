// msg_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>

#define SOCKET_PATH "/tmp/msg_server.sock"
#define MAX_MSG_LEN 256
#define MAX_MESSAGES 100
#define MAX_CLIENTS 10


typedef struct {
    char messages[MAX_MESSAGES][MAX_MSG_LEN];
    int count;              
    int next_index;         
    pthread_mutex_t lock;   
} MessageList;

MessageList msg_list = {
    .count = 0,
    .next_index = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER
};

typedef enum {
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

// Structured message thats sent through the pipe
typedef struct {
    LogLevel level;
    time_t timestamp;
    char message[MAX_MSG_LEN];
} LogEntry;

// Argument for the client thread
typedef struct
{
    int client_fd; // The clients socket file descriptor
    int client_id; // Unique client-ID
} ClientArgs;

// Global: Writing end of pipe to the logprocess
int log_pipe_fd = -1;

const char *level_names[] = { "INFO", "WARNING", "ERROR" };

static int stats[3] = {0, 0, 0}; // INFO, WARNING, ERROR
static FILE *file = NULL;


// Helpfunction: send log message through the pipe
void send_log(LogLevel level, const char *msg)
{
    if (log_pipe_fd < 0)
        return;

    LogEntry entry;
    entry.level = level;
    entry.timestamp = time(NULL);
    strncpy(entry.message, msg, MAX_MSG_LEN - 1);
    entry.message[MAX_MSG_LEN - 1] = '\0';

    write(log_pipe_fd, &entry, sizeof(LogEntry));
}

void add_message(const char *msg) 
{
    pthread_mutex_lock(&msg_list.lock);

    strncpy(msg_list.messages[msg_list.next_index], msg, MAX_MSG_LEN - 1);

    msg_list.messages[msg_list.next_index][MAX_MSG_LEN - 1] = '\0';
    msg_list.next_index = (msg_list.next_index + 1) % MAX_MESSAGES;

    if (msg_list.count < MAX_MESSAGES) 
    {
        msg_list.count++;
    }

    pthread_mutex_unlock(&msg_list.lock);
}


int get_latest_message(char *buf, int buf_size) 
{
    if (buf == NULL)
        return -1;

    pthread_mutex_lock(&msg_list.lock);

    snprintf(buf, buf_size, "%s", msg_list.messages[msg_list.next_index - 1]);
        
    pthread_mutex_unlock(&msg_list.lock);

    return 0;
}

void *client_handler(void *arg)
{
    ClientArgs *args = (ClientArgs *)arg;

    char buf[MAX_MSG_LEN];
    char log_msg[512];

    snprintf(log_msg, sizeof(log_msg),
             "Client %d connected", args->client_id);
    send_log(LOG_INFO, log_msg);

    ssize_t bytes_read = recv(args->client_fd, buf, sizeof(buf) - 1, 0);

    if (bytes_read > 0)
    {
        buf[bytes_read] = '\0';
        add_message(buf);

        snprintf(log_msg, sizeof(log_msg), "Client %d: %s", args->client_id, buf);
        send_log(LOG_INFO, buf);

        char latest_msg[MAX_MSG_LEN];
        if (get_latest_message(latest_msg, sizeof(latest_msg)) == 0) {
            /* printf("Latest message: %s\r\n", latest_msg); */
            send(args->client_fd, latest_msg, strlen(latest_msg), 0);
        }
    }

    close(args->client_fd);
    free(args);
    
    return NULL;
}

void log_process(int read_fd)
{
    LogEntry entry = {0};

    file = fopen("server.log", "a");
    if (!file)
    {
        perror("fopen server.log");
        send_log(LOG_ERROR, "File not found!");
        exit(EXIT_FAILURE);
    }

    while (read(read_fd, &entry, sizeof(LogEntry)) == sizeof(LogEntry))
    {
        struct tm *time_info;
        char buffer[9];
        time_info = localtime(&entry.timestamp);

        strftime(buffer, sizeof(buffer), "%H:%M:%S", time_info);
        
        fprintf(file, "[%02d:%02d:%02d] [%s] %s\n", time_info->tm_hour, time_info->tm_min, time_info->tm_sec, level_names[entry.level], entry.message);
        stats[entry.level]++;
        fflush(file);

        memset(&entry, 0, sizeof(LogEntry));
    }

    exit(EXIT_SUCCESS);
}

void signalHandler(int sig) 
{
    if (file == NULL) return;

    fprintf(file, "\n------------------- LOG SUMMARY -------------------\n");
    fprintf(file, "[Infos: %d, Warnings: %d, Errors: %d]\n", stats[LOG_INFO], stats[LOG_WARNING], stats[LOG_ERROR]);
    fprintf(file, "Total number of log messages: %d\n", stats[0] + stats[1] + stats[2]);

    fflush(file);
    fclose(file);

    exit(EXIT_SUCCESS);
}



int main()
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGKILL, signalHandler);
    signal(SIGQUIT, signalHandler);

    int pipe_fds[2];
    int pipe_result = pipe(pipe_fds);
    if (pipe_result == -1)
    {
        perror("pipe");
        send_log(LOG_ERROR, "Failed to create pipe");
        return -1;
    }

    pid_t forked_pid = fork();

    if (forked_pid == 0)
    {
        // Child
        close(pipe_fds[1]);

        log_process(pipe_fds[0]);
        
        close(pipe_fds[0]);
    } 
    else if (forked_pid > 0)
    {
        send_log(LOG_INFO, "Server starting");

        // Parent
        close(pipe_fds[0]);
        log_pipe_fd = pipe_fds[1];

        unlink(SOCKET_PATH);

        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) { perror("socket"); send_log(LOG_ERROR, "Failed to create socket"); return 1; }
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
        
        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        {
            perror("bind");
            send_log(LOG_ERROR, "Failed to bind");
            close(sock);
            
            exit(EXIT_FAILURE);
        }
        
        if (listen(sock, MAX_CLIENTS) == -1) 
        {
            perror("listen");
            send_log(LOG_ERROR, "Failed to listen");
            close(sock);
            
            exit(EXIT_FAILURE);
        }

        int client_count = 0;
        while (1)
        {
            int client_fd = accept(sock, NULL, NULL);
            if (client_fd == -1)
            {
                perror("accept");
                send_log(LOG_WARNING, "Max client count has been reached");
                continue;
            }
            
            ClientArgs *args = (ClientArgs*)malloc(sizeof(ClientArgs));
            args->client_fd = client_fd;
            args->client_id = ++client_count;

            pthread_t thread;
            if (pthread_create(&thread, NULL, client_handler, args) != 0)
            {
                perror("pthread_create");
                send_log(LOG_ERROR, "Failed to create thread");
                close(client_fd);
                free(args);
                args = NULL;
                continue;
            }
            pthread_detach(thread);
        }
        
        close(pipe_fds[1]);
        printf("Server listening to %s\n", SOCKET_PATH);
        send_log(LOG_INFO, "Server is listening to socket");
    }
        
    return 0;
}