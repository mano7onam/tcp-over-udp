//
// Created by mano on 28.10.16.
//

#include "../tcp_server.h"

#define PORT 4444

int main(int argc, char** argv) {
    unsigned short port = PORT;
    TCP_Server tcp_server(port);
    int server_socket = tcp_server.get_server_socket();
    tcp_server.do_listen();

    std::vector<int> fds;

    void* buf = malloc(100);
    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);

        FD_SET(server_socket, &readfds);
        int max_fd = server_socket;

        for (int i = 0; i < fds.size(); ++i)
            max_fd = std::max(max_fd, fds[i]);

        if (max_fd < 0)
            break;

        for (int i = 0; i < fds.size(); ++i)
            FD_SET(fds[i], &readfds);
        fprintf(stderr, "Before select\n");
        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0)
            continue;
        fprintf(stderr, "After select\n");

        if (FD_ISSET(server_socket, &readfds)) {
            fprintf(stderr, "Can new connection\n");
            int new_con = tcp_server.do_accept();
            fds.push_back(new_con);
            fprintf(stderr, "Receive new connection\n");
        }

        for (int i = 0; i < fds.size(); ++i) {
            if (FD_ISSET(fds[i], &readfds)) {
                exit(0);
                fprintf(stderr, "Before\n");
                //read(fds[i], buf, 4000);
                fprintf(stderr, "After\n");
                ssize_t size = tcp_server.do_recv(fds[i], buf, 50);
                if (size == 0) {
                    fprintf(stderr, "Closed connection %d\n", i);
                    fds.erase(fds.begin() + i);
                    continue;
                }
                ((char*)buf)[size + 1] = '\0';
                fprintf(stderr, "[%d]: ", i);
                fprintf(stderr, "Size: %d, Message: %s\n", size, (char*)buf);

                char message[25] = "qwertyuiopasdfghjklzxcvb";
                size = tcp_server.do_send(fds[i], message, 25);
            }
        }
    }
    free(buf);

    return 0;
}
