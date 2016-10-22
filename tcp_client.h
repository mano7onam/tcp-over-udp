//
// Created by mano on 18.10.16.
//

#ifndef PRJ_TCP_CLIENT_H
#define PRJ_TCP_CLIENT_H

#include "global_definitions.h"
#include "connection.h"


#define CUSTOM_BUFFER_SIZE 1024

// RS - "receive server" types:
#define RS_ACCEPT_1 'a'
#define RS_ACCEPT_1_SIZE
#define RS_ACCEPT_2 'b'
#define RS_ACCEPT_2_SIZE

struct TCP_Client {
    int socket_fd = -1;
    std::thread* main_loop_thread = NULL;
    Connection* connection = NULL;

    std::mutex mtx_accept1;
    std::mutex mtx_accept2;
    std::condition_variable cv_accept1;
    Connection* curent_accept_connection = NULL;

    // for starting to find from 5000 port
    int port_counter = 5000;

    void* buf;
    int pbuf;

    bool listen_flag;
    std::thread* listen_thread;

    TCP_Client(int socket_fd);

    int do_connect(struct sockaddr_in addr);

    ssize_t do_recv(void* buf, size_t size);

    ssize_t do_send(void* buf, size_t size);

    ~TCP_Client();
};

#endif //PRJ_TCP_CLIENT_H
