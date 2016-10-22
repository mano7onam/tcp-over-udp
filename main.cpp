#include "tcp_server.h"

#define PORT 4444
#define IP_ADDRESS "localhost"

int main(int argc, char** argv) {
    int	master_socket = 0;
    struct sockaddr_in s_addr;
    struct hostent *host_info = NULL;
    unsigned short port = PORT;

    master_socket = socket(PF_INET, SOCK_DGRAM, 0);

    s_addr.sin_family = PF_INET;
    s_addr.sin_port = htons(port);
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(master_socket, (struct sockaddr *) &s_addr, sizeof(struct sockaddr_in));

    TCP_Server tcp_server(master_socket);
    int client_fd = tcp_server.do_accept();

    return 0;
}
