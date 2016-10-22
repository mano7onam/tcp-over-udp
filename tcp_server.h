//
// Created by mano on 18.10.16.
//

#ifndef PRJ_TCP_SERVER_H
#define PRJ_TCP_SERVER_H


#pragma once

#include "includes.h"
#include "connection.h"

#define MAX_QUEUE_ACCEPT_SIZE 1000

// RS - "receive server" types:
#define RS_ACCEPT_1 'a'
#define RS_ACCEPT_1_SIZE
#define RS_ACCEPT_2 'b'
#define RS_ACCEPT_2_SIZE

typedef std::pair<int, unsigned short> Ip_Port;

struct TCP_Server {
    int socket_fd;
    std::thread* main_loop_thread;
    std::map<int, Connection*> connections;
    std::map<Ip_Port, int> m_connections;

    std::mutex mtx_accept1;
    std::mutex mtx_accept2;
    std::condition_variable cv_accept1;
    Connection* curent_accept_connection = NULL;
    std::queue<Connection*> queue_accept_connections;
    int port_counter;

    void* buf;
    int pbuf;
    int szbuf;

    bool listen_flag;
    std::thread* listen_thread;

    TCP_Server(int socket_fd); // it receive ready udp socket

    void do_new_connection();

    void do_listen();

    int do_accept();

    ssize_t do_recv(int socket_fd, void* buf, size_t size);

    ssize_t do_send(int socket_fd, void* buf, size_t size);

    ~TCP_Server();
};



#endif //PRJ_TCP_SERVER_H
