//
// Created by mano on 22.10.16.
//

#include "connections_creator.h"

Connections_Creator::Connections_Creator(int master_socket,
                                std::map<Ip_Port, int> *m_established_connections,
                                std::mutex* mtx_connections)
{
    this->master_socket = master_socket;
    this->server_port = server_port;
    this->buf = malloc(CUSTOM_SERVER_BUFFER_SIZE);
    this->m_established_connections = m_established_connections;
    this->mtx_connections = mtx_connections;
    pipe(pipe_fd);
}

Connections_Creator::~Connections_Creator() {
    free(buf);
}

int Connections_Creator::handle_first_handshake(Ip_Port ip_port, ssize_t size_message,
                                                struct sockaddr_in his_addr)
{
    if (size_message < 4) {
        fprintf(stderr, "Size of package must be greater than 4\n");
        return -1;
    }
    bool flag_have_first_handshake = (bool)wait_connections.count(ip_port);
    unsigned short his_port = ((unsigned short*)buf)[1]; // offset 2 bytes

    Connection* connection;
    if (!flag_have_first_handshake) {
        try {
            connection = new Connection(CUSTOM_CONNECTION_TMP_BUFFER_SIZE,
                                        CUSTOM_CONNECTION_RECV_BUFFER_SIZE,
                                        CUSTOM_CONNECTION_SEND_BUFFER_SIZE,
                                        4000, his_addr);
            connection->set_his_port(his_port);
        } catch(ConnectionException ex) {
            fprintf(stderr, "%s\n", ex.what());
            delete connection;
            return -1;
        }
        wait_connections[ip_port] = connection;
    }
    else {
        connection = wait_connections[ip_port];
    }

    char handshake_server[4];
    handshake_server[0] = 1;
    handshake_server[1] = 1;
    ((unsigned short*)handshake_server)[1] = connection->my_addr.sin_port;
    sendto(master_socket, (void*)handshake_server, 4, 0,
           (struct sockaddr *) &(his_addr),
           sizeof(struct sockaddr_in));

    return !flag_have_first_handshake;
}

int Connections_Creator::handle_second_handshake(Ip_Port ip_port, struct sockaddr_in addr) {
    if (!wait_connections.count(ip_port)) {
        fprintf(stderr, "No waiting connections\n");
        return -1;
    }

    std::unique_lock<std::mutex> lock(mtx);
    if (queued_connections.size() > CUSTOM_SIZE_CONNECTIONS_QUEUE){
        fprintf(stderr, "Full queue incoming connections\n");
        return 0;
    }

    queued_connections.push(wait_connections[ip_port]);
    wait_connections.erase(ip_port);
    write(pipe_fd[1], buf, 4); // notify poll or select
    cv_queue.notify_one(); // notify get_connection_from_queue function caller
    fprintf(stderr, "Add one queued connection\n");
}

int Connections_Creator::do_receive_message() {
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    bzero(&addr, sizeof(struct sockaddr_in));

    ssize_t size = recvfrom(master_socket, buf, CUSTOM_SERVER_BUFFER_SIZE, 0,
                            (struct sockaddr *) &addr, &addr_size);
    int source_ip = ntohl(addr.sin_addr.s_addr);
    unsigned short source_port = ntohs(addr.sin_port);
    Ip_Port ip_port(source_ip, source_port);

    if (size < 2) {
        fprintf(stderr, "Size of package must be greater than 2\n");
        return -1;
    }

    // check if connection has already established
    mtx_connections->lock();
    if (m_established_connections->count(ip_port)) {
        return 0;
    }
    mtx_connections->unlock();

    char flag1 = ((char*)buf)[0];
    char flag2 = ((char*)buf)[1];
    if (flag1 == 0 && flag2 == 0) {
        fprintf(stderr, "Handle first handshake\n");
        return handle_first_handshake(ip_port, size, addr);
    }
    else if (flag1 == 1 && flag2 == 1) {
        fprintf(stderr, "Handle second handshake\n");
        return handle_second_handshake(ip_port, addr);
    }
    else {
        fprintf(stderr, "Incorrect type of message");
        return -1;
    }
}

Connection* Connections_Creator::get_connectoin_from_queue() {
    std::unique_lock<std::mutex> lock(mtx);
    while (queued_connections.empty()) {
        cv_queue.wait(lock);
    }
    fprintf(stderr, "!!! After lock\n");

    char tmp_buf[5];
    read(pipe_fd[0], (void*)tmp_buf, 4);
    fprintf(stderr, "After read\n");
    Connection* connection = queued_connections.front();
    queued_connections.pop();
    int con_ip = ntohl(connection->his_addr.sin_addr.s_addr);
    unsigned short con_port = ntohs(connection->his_addr.sin_port);
    wait_connections.erase(Ip_Port(con_ip, con_port));

    return connection;
}