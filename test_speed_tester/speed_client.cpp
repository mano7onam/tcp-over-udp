#include "../tcp_client.h"

#define BUFFER_SIZE (1024)
#define IP_ADDRESS "localhost"
#define PORT 4445

int main(int argc, char *argv[])
{
    struct sockaddr_in addr;
    void *buf = malloc(BUFFER_SIZE + 1);

    unsigned short port_my = PORT;
    unsigned short port_his = PORT - 1;
    TCP_Client tcp_client(port_my);
    int socket_fd = tcp_client.do_connect(IP_ADDRESS, port_his);

    while (1)
    {
        int size = tcp_client.do_send(buf, BUFFER_SIZE);
        if (size <= 0) {
            fprintf(stderr, "Some error.\n");
            break;
        }
        if (size == 0) {
            fprintf(stderr, "Server has been disconnected.\n");
            break;
        }
        //fprintf(stderr, "Size: %d\n", size);
    }

    close(socket_fd);
    free(buf);
    return EXIT_SUCCESS;
}
