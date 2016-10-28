//
// Created by mano on 18.10.16.
//

#ifndef PRJ_TCP_SERVER_H
#define PRJ_TCP_SERVER_H


#pragma once

#include "global_definitions.h"
#include "connection.h"
#include "connections_creator.h"

#define MAX_QUEUE_ACCEPT_SIZE 1000

// RS - "receive server" types:
#define RS_ACCEPT_1 'a'
#define RS_ACCEPT_1_SIZE
#define RS_ACCEPT_2 'b'
#define RS_ACCEPT_2_SIZE

struct TCP_Server {
    int socket_fd;
    std::thread* main_loop_thread;

    std::map<int, Connection*> connections;
    std::map<Ip_Port, int> m_connections;
    std::mutex mtx_connections;

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

    Connections_Creator *connections_creator;

    TCP_Server(int socket_fd); // it receive ready udp socket

    TCP_Server(unsigned short port);

    int get_server_socket(); // return pipe to select support accept

    void do_listen();

    int do_accept();

    ssize_t do_recv(int socket_fd, void* buf, size_t size);

    ssize_t do_send(int socket_fd, void* buf, size_t size);

    ~TCP_Server();
};



#endif //PRJ_TCP_SERVER_H
