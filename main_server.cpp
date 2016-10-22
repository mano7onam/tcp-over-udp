#include "tcp_server.h"

#define PORT 4444
#define IP_ADDRESS "localhost"

int main(int argc, char** argv) {
    int	master_socket = 0;
    struct sockaddr_in s_addr;
    struct hostent *host_info = NULL;

    master_socket = socket(PF_INET, SOCK_DGRAM, 0);

    s_addr.sin_family = PF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    unsigned short port_my = PORT;
    while (1) {
        s_addr.sin_port = htons(port_my);
        if (0 == bind(master_socket, (struct sockaddr *) &s_addr, sizeof(struct sockaddr_in)))
            break;
        ++port_my;
    }

    TCP_Server tcp_server(master_socket);
    tcp_server.do_listen();

    int fds[2];
    int num_fds = 2;
    for (int i = 0; i < num_fds; ++i)
        fds[i] = tcp_server.do_accept();

    void* buf = malloc(100);
    while (1) {
        fd_set readfds;
        int max_fd = -1;
        for (int i = 0; i < num_fds; ++i)
            max_fd = std::max(max_fd, fds[i]);
        if (max_fd < 0)
            break;

        FD_ZERO(&readfds);
        for (int i = 0; i < num_fds; ++i)
            FD_SET(fds[i], &readfds);
        fprintf(stderr, "Before select\n");
        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0)
            continue;
        fprintf(stderr, "After select\n");

        for (int i = 0; i < num_fds; ++i) {
            if (FD_ISSET(fds[i], &readfds)) {
                read(fds[i], buf, 4);
                fprintf(stderr, "Message from [%d]:\n", i);
                int size = tcp_server.do_recv(fds[i], buf, 50);
                ((char*)buf)[size + 1] = '\0';
                fprintf(stderr, "Size: %d, Message: %s\n", size, (char*)buf);
            }
        }
    }
    free(buf);

    return 0;
}
