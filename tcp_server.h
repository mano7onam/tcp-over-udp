//
// Created by mano on 18.10.16.
//

#ifndef PRJ_TCP_SERVER_H
#define PRJ_TCP_SERVER_H

#include "global_definitions.h"
#include "connection.h"
#include "connections_creator.h"

struct TCP_Server {
    int socket_fd;
    std::thread* main_loop_thread;

    std::map<int, Connection*> connections;
    std::map<Ip_Port, int> m_connections;
    std::mutex mtx_connections;

    void* buf;
    int pbuf;
    int szbuf;

    bool listen_flag;
    std::thread* listen_thread;

    Connections_Creator *connections_creator;

    TCP_Server(int socket_fd); // it receive ready udp socket

    TCP_Server(unsigned short port);

    int set_connection_not_active(int id_connection, std::string cause);

    int get_server_socket(); // return pipe to select support accept

    void do_listen();

    int do_accept(struct sockaddr_in & addr);

    ssize_t do_recv(int socket_fd, void* buf, size_t size);

    ssize_t do_send(int socket_fd, void* buf, size_t size);

    int do_close(int socket_fd);

    ~TCP_Server();
};



#endif //PRJ_TCP_SERVER_H
