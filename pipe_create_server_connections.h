//
// Created by mano on 22.10.16.
//

#ifndef PRJ_PIPE_CREATE_CONNECTIONS_H
#define PRJ_PIPE_CREATE_CONNECTIONS_H

#include "global_definitions.h"
#include "connection.h"

// Write 4 bytes to pipe for each new Connection in queue
// and read 4 bytes when accept respectively

struct Pipe_Create_Server_Connections {
    int master_socket;
    int pipe_fd[2];
    void* buf;
    std::queue<Connection*> queued_connections;
    std::map<Ip_Port, unsigned short> wait_connections; // ip, port -> new connection port
    std::map<Ip_Port, int> *m_established_connections;
    std::map<int, Connection*> *established_connections;

    Pipe_Create_Server_Connections(int master_socket,
                                   std::map<int, Connection*> *established_connections,
                                   std::map<Ip_Port, int> *m_established_connections);

    // receive message to master socket and analyze it
    int do_receive_message();

    ~Pipe_Create_Server_Connections();
};


#endif //PRJ_PIPE_CREATE_CONNECTIONS_H
