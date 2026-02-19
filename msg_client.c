// msg_client.c - Simple test client
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/msg_server.sock"
#define MAX_MSG_LEN 256

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <message>\n", argv[0]);
        return 1;
    }

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        close(sock);
        return 1;
    }

    send(sock, argv[1], strlen(argv[1]), 0);
    printf("Sent: %s\n", argv[1]);

    // Wait for response (broadcast message)
    char buf[MAX_MSG_LEN];
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n > 0)
    {
        buf[n] = '\0';
        printf("Received: %s\n", buf);
    }
    
    close(sock);
    return 0;
}