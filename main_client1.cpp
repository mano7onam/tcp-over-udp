#include "tcp_client.h"

#define PORT_HIS 4444
#define PORT_MY 4445
#define IP_ADDRESS "localhost"

int main(int argc, char** argv) {
    unsigned short port_his = PORT_HIS;
    unsigned short port_my = PORT_MY;

    TCP_Client tcp_client(port_my);
    int client_fd = tcp_client.do_connect(IP_ADDRESS, port_his);
    fprintf(stderr, "Client fd: %d\n", client_fd);

    for (int i = 0; i < 100; ++i) {
        char message[100];
        snprintf(message, 50, "Iteration: %d\0", i);
        int size = tcp_client.do_send((void*)message, 50);
        char message1[100] = "22222\0";
        size = tcp_client.do_send((void*)message1, 50);
        char message2[100] = "333333333333\0";
        size = tcp_client.do_send((void*)message2, 50);
        char message3[100] = "444444444444444\0";
        size = tcp_client.do_send((void*)message3, 50);
        char message4[100] = "555555555555555555555555555\0";
        size = tcp_client.do_send((void*)message4, 50);
        usleep(100);
    }

    sleep(5);

    return 0;
}
