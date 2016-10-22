//
// Created by mano on 22.10.16.
//

#include "pipe_create_server_connections.h"

/*Pipe_Create_Server_Connections::Pipe_Create_Server_Connections(int master_socket,
                               std::map<int, Connection*> *established_connections,
                               std::map<Ip_Port, int> *m_established_connections)
{
    this->master_socket = master_socket;
    this->buf = malloc(CUSTOM_SERVER_BUFFER_SIZE);
    this->established_connections = established_connections;
    this->m_established_connections = m_established_connections;
}

Pipe_Create_Server_Connections::~Pipe_Create_Server_Connections() {
    free(buf);
}

int Pipe_Create_Server_Connections::do_receive_message() {
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    bzero(&addr, sizeof(struct sockaddr_in));
    ssize_t size = recvfrom(master_socket, buf, CUSTOM_SERVER_BUFFER_SIZE, 0,
                            (struct sockaddr *) &addr, &addr_size);
    int source_ip   = ntohl(addr.sin_addr.s_addr);
    unsigned short source_port = ntohs(addr.sin_port);
    Ip_Port ip_port(source_ip, source_port);

    if (size < 2) {
        fprintf(stderr, "Size of package must be greater than 2\n");
        return -1;
    }

    // check if connection already established
    if (m_established_connections->count(ip_port)) {
        //fprintf(stderr, "Connection already established\n");
        return 0;
    }

    char flag_syn = ((char*)buf)[0];
    char flag_ack = ((char*)buf)[1];
    if (flag_syn == 0 && flag_ack == 0) {
        if (size < 4) {
            fprintf(stderr, "Size of package must be greater than 4\n");
            return -1;
        }
        if (wait_connections.count(ip_port)) {
            return 0;
        }
        unsigned short his_port = ((unsigned short*)buf)[1]; // offset 2 bytes
        wait_connections[ip_port] = his_port;
    }
    else if (flag_syn == 1 && flag_ack == 1) {
        if (!wait_connections.count(ip_port)) {
            fprintf(stderr, "Incorrect message\n");
            return -1;
        }
        unsigned short his_port = wait_connections[ip_port];

        Connection* connection;
        try {
            connection = new Connection(CUSTOM_CONNECTION_TMP_BUFFER_SIZE,
                                        CUSTOM_CONNECTION_RECV_BUFFER_SIZE,
                                        CUSTOM_CONNECTION_SEND_BUFFER_SIZE,
                                        4000, addr);
            connection->set_his_port(his_port);
        } catch(ConnectionException ex) {
            fprintf(stderr, "%s\n", ex.what());
            delete connection;
            return -1;
        }
    }
    else {
        fprintf(stderr, "Incorrect type message");
        return -1;
    }

    Connection* connection = NULL;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    struct sockaddr_in addr;
    struct sockaddr_in his_addr;

    uint32_t source_ip; // ip new connection
    unsigned short source_port; // port new connection
    while (1) {
        fprintf(stderr, "Server:\n");

        his_addr = addr;
        source_ip   = ntohl(addr.sin_addr.s_addr);
        source_port = ntohs(addr.sin_port);
        if (m_connections.count(Ip_Port(source_ip, source_port)))
            continue;
        fprintf(stderr, "New connection: Ip: %d, Port: %d\n", source_ip, source_port);
        if (size < 4) {
            fprintf(stderr, "Size of package must be greater than 4\n");
            continue;
        }

        char flag_syn = ((char*)buf)[0];
        char flag_ack = ((char*)buf)[1];

        if (flag_syn != 0 || flag_ack != 0)
            continue;



        break;
    }

    while (1) {
        fprintf(stderr, "Recv second accept message:\n");
        char handshake_server[4];
        handshake_server[0] = 1;
        handshake_server[1] = 1;
        ((unsigned short*)handshake_server)[1] = connection->my_addr.sin_port;
        sendto(socket_fd, (void*)handshake_server, 4, 0,
               (struct sockaddr *) &(his_addr),
               sizeof(struct sockaddr_in));

        fd_set readfds;
        int max_desc;
        FD_ZERO(&readfds);
        FD_SET(socket_fd, &readfds);
        max_desc = socket_fd;
        struct timeval tv = {0, 1000000};
        int activity = select(max_desc + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0 || !FD_ISSET(socket_fd, &readfds))
            continue;

        fprintf(stderr, "%d\n", activity);
        ssize_t size = recvfrom(socket_fd, buf, CUSTOM_SERVER_BUFFER_SIZE, 0,
                                (struct sockaddr *) &addr, &addr_size);
        if (size < 2) {
            fprintf(stderr, "Size of package must be greater than 2\n");
            continue;
        }

        char flag_syn = ((char*)buf)[0];
        char flag_ack = ((char*)buf)[1];
        fprintf(stderr, "Flags: %d %d\n", (int)flag_syn, (int)flag_ack);
        if (flag_syn != 1 || flag_ack != 1)
            continue;

        break;
    }

    connections[connection->pipe_buffer_recv->get_read_side()] = connection;
    m_connections[Ip_Port(source_ip, source_port)] = connection->pipe_buffer_recv->get_read_side();
    return connection->pipe_buffer_recv->get_read_side();
}*/