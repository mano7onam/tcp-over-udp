#include "tcp_client.h"

#define PORT_HIS 4444
#define PORT_MY 4445
#define IP_ADDRESS "localhost"

int main(int argc, char** argv) {
    int master_socket = 0;
    struct sockaddr_in s_addr;
    struct sockaddr_in addr_his;
    struct hostent *host_info = NULL;
    unsigned short port_his = PORT_HIS;

    master_socket = socket(PF_INET, SOCK_DGRAM, 0);

    s_addr.sin_family = PF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    unsigned short port_my = PORT_MY;
    while (1) {
        s_addr.sin_port = htons(port_my);
        if (0 == bind(master_socket, (struct sockaddr *) &s_addr, sizeof(struct sockaddr_in)))
            break;
        ++port_my;
    }

    addr_his.sin_family = PF_INET;
    addr_his.sin_port = htons(port_his);
    host_info = gethostbyname(IP_ADDRESS);
    memcpy(&addr_his.sin_addr, host_info->h_addr, host_info->h_length);

    TCP_Client tcp_client(master_socket);
    int client_fd = tcp_client.do_connect(addr_his);
    fprintf(stderr, "Client fd: %d\n", client_fd);

    char message[100] = "abcdsdfa\0";
    int size = tcp_client.do_send((void*)message, 50);
    char message1[100] = "oooooooooooooooo\0";
    size = tcp_client.do_send((void*)message1, 50);
    char message2[100] = "tatattatatattataat\0";
    size = tcp_client.do_send((void*)message2, 50);
    char message3[100] = "mano mano mano\0";
    size = tcp_client.do_send((void*)message3, 50);
    char message4[100] = "ututututu utut tut ugdf;gje5q6985690754\0";
    size = tcp_client.do_send((void*)message4, 50);
    char message5[100] = "abcdsddfgdfsgsdfgdsfgsrrrrrrfa\0";
    size = tcp_client.do_send((void*)message5, 50);

    fprintf(stderr, "Pushed: %d\n", size);
    fprintf(stderr, "Size buf: %d\n", tcp_client.connection->send_buf->get_size());

    while (1);

    return 0;
}
