//
// Created by mano on 22.10.16.
//

#ifndef PRJ_PIPE_CREATE_CONNECTIONS_H
#define PRJ_PIPE_CREATE_CONNECTIONS_H

#include "global_definitions.h"
#include "connection.h"

// Write 4 bytes to pipe for each new Connection in queue
// and read 4 bytes when accept respectively

struct Connections_Creator {
    int master_socket; // udp socket receive datagram
    int pipe_fd[2]; // pipe to notify acceptors
    std::mutex mtx; // mutex to multi-thread read from and write to pipe
    std::condition_variable cv_queue; // condition variable to notify get_connection_from_queue
    void* buf;

    std::queue<Connection*> queued_connections; // connections ready to accept
    std::map<Ip_Port, Connection*> wait_connections; // ip, port -> new connection port

    std::map<Ip_Port, int> *m_established_connections; // ip, port -> socket to get a connection
    std::mutex* mtx_connections;

    unsigned short server_port;

    Connections_Creator(int master_socket,
                        std::map<Ip_Port, int> *m_established_connections,
                        std::mutex* mtx_connections);

    int handle_first_handshake(Ip_Port ip_port, ssize_t size_message, struct sockaddr_in his_addr);

    int handle_second_handshake(Ip_Port ip_port, struct sockaddr_in addr);

    // receive message to master socket and analyze it
    int do_receive_message();

    // call from accept in TCP_Server
    Connection* get_connectoin_from_queue();

    ~Connections_Creator();
};


#endif //PRJ_PIPE_CREATE_CONNECTIONS_H
