//
// Created by mano on 18.10.16.
//

#include <stdint.h>
#include "tcp_server.h"

TCP_Server::TCP_Server(int socket_fd) {
    this->socket_fd = socket_fd;
    this->main_loop_thread = NULL;

    pbuf = 0;
    szbuf = CUSTOM_SERVER_BUFFER_SIZE;
    buf = malloc(szbuf);

    port_counter = 5000;

    listen_flag = false;
    listen_thread = NULL;
}

void background_receive_thread_function(TCP_Server* server) {
    while (server->listen_flag) {
        if (server->connections.empty())
            continue;
        fd_set readfds;
        int max_fd = -1;
        FD_ZERO(&readfds);
        for (auto elem : server->connections) {
            Connection* con = elem.second;
            con->do_background_send();
            FD_SET(con->socket_fd, &readfds);
            if (max_fd == -1 || max_fd < con->socket_fd)
                max_fd = con->socket_fd;
        }
        if (-1 == max_fd)
            continue;

        struct timeval tv = {0, 3000000};
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0)
            continue;

        for (auto elem : server->connections) {
            Connection* con = elem.second;
            if (FD_ISSET(con->socket_fd, &readfds)) {
                int res = con->do_background_recv();
                if (res < 0) {
                    fprintf(stderr, "%s\n", "Error when receive to buffer");
                }
            }
        }
    }
}

void TCP_Server::do_listen() {
    listen_flag = true;
    listen_thread = new std::thread(background_receive_thread_function, this);
}

int TCP_Server::do_accept() {
    Connection* connection = NULL;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    struct sockaddr_in addr;
    struct sockaddr_in his_addr;

    uint32_t source_ip; // ip new connection
    unsigned short source_port; // port new connection
    while (1) {
        fprintf(stderr, "Server:\n");
        int size = recvfrom(socket_fd, buf, CUSTOM_SERVER_BUFFER_SIZE, 0, (struct sockaddr *) &addr, &addr_size);
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
        unsigned short his_port = ((unsigned short*)buf)[1]; // offset 2 bytes
        if (flag_syn != 0 || flag_ack != 0)
            continue;

        try {
            connection = new Connection(CUSTOM_CONNECTION_TMP_BUFFER_SIZE,
                                        CUSTOM_CONNECTION_RECV_BUFFER_SIZE,
                                        CUSTOM_CONNECTION_SEND_BUFFER_SIZE,
                                        port_counter, addr);
            connection->set_his_port(his_port);
        } catch(ConnectionException ex) {
            fprintf(stderr, "%s\n", ex.what());
            delete connection;
            return -1;
        }

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
        int size = recvfrom(socket_fd, buf, CUSTOM_SERVER_BUFFER_SIZE, 0, (struct sockaddr *) &addr, &addr_size);
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

    connections[connection->pipe_fd[0]] = connection;
    m_connections[Ip_Port(source_ip, source_port)] = connection->pipe_fd[0];
    return connection->pipe_fd[0];
}

int TCP_Server::do_recv(int socket_fd, void* buf, size_t size) {
    if (!connections.count(socket_fd)) {
        fprintf(stderr, "%s\n", "No such socket descriptor in connections.");
        return -1;
    }
    Connection* con = connections[socket_fd];

    // lock mutex and wait while no data in buffer
    std::unique_lock<std::mutex> lock(con->mtx_recv);
    fprintf(stderr, "Size buf: %d\n", con->recv_buf->get_size());
    while (con->recv_buf->is_empty()) {
        fprintf(stderr, "Wait!!!\n");
        con->cv_recv.wait(lock);
    }
    fprintf(stderr, "Waiting finished!\n");

    // take data from buffer
    int res_size = con->recv_buf->get_data(buf, size, true);

    // read all from pipe to next select
    fprintf(stderr, "Before read\n");
    /*int flags = fcntl(con->pipe_fd[0], F_GETFL, 0);
    fcntl(con->pipe_fd[0], F_SETFL, flags | O_NONBLOCK);*/
    fprintf(stderr, "Size: %d\n", read(con->pipe_fd[0], this->buf, 4000));
    fprintf(stderr, "After read\n");

    // if remain data in buffer read to pipe to next select
    if (con->recv_buf->get_size() != 0) {
        write(con->pipe_fd[1], this->buf, 4);
        fprintf(stderr, "Rest data\n");
    }

    return res_size;
}

int TCP_Server::do_send(int socket_fd, void* buf, size_t size) {
    if (!connections.count(socket_fd)) {
        fprintf(stderr, "%s\n", "No such socket descriptor in connections.");
        return -1;
    }
    Connection* con = connections[socket_fd];

    std::unique_lock<std::mutex> lock(con->mtx_send);
    while (con->send_buf->is_full()) {
        con->cv_send.wait(lock);
    }

    return con->send_buf->push_data(buf, size);
}

TCP_Server::~TCP_Server() {
    fprintf(stderr, "Destructor server\n");
    listen_flag = false;
    listen_thread->join();
    delete listen_thread;
    for (auto elem : connections)
        delete elem.second;
    free(buf);
}
