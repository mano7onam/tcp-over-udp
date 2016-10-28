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

    this->connections_creator = new Connections_Creator(socket_fd, &m_connections, &mtx_connections);
}

TCP_Server::TCP_Server(unsigned short port) {
    socket_fd = 0;
    struct sockaddr_in s_addr;

    socket_fd = socket(PF_INET, SOCK_DGRAM, 0);

    s_addr.sin_family = PF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    unsigned short port_my = port;
    while (1) {
        s_addr.sin_port = htons(port_my);
        if (0 == bind(socket_fd, (struct sockaddr *) &s_addr, sizeof(struct sockaddr_in)))
            break;
        ++port_my;
    }

    this->main_loop_thread = NULL;

    pbuf = 0;
    szbuf = CUSTOM_SERVER_BUFFER_SIZE;
    buf = malloc(szbuf);

    port_counter = 5000;

    listen_flag = false;
    listen_thread = NULL;

    this->connections_creator = new Connections_Creator(socket_fd, &m_connections, &mtx_connections);
}

int TCP_Server::get_server_socket() {
    return connections_creator->pipe_fd[0];
}

void background_send_receive_thread_function(TCP_Server *server) {
    bool have_send_data = false;
    while (server->listen_flag || have_send_data) {
        have_send_data = false;

        struct timeval tv;
        fd_set readfds;
        int max_fd;
        FD_ZERO(&readfds);

        FD_SET(server->socket_fd, &readfds);
        max_fd = server->socket_fd;

        server->mtx_connections.lock();
        for (auto elem : server->connections) {
            Connection* con = elem.second;
            FD_SET(con->socket_fd, &readfds);
            if (max_fd < con->socket_fd)
                max_fd = con->socket_fd;
        }
        server->mtx_connections.unlock();

        if (-1 == max_fd)
            continue;

        tv = {0, CUSTOM_SELECT_TIMEOUT};
        //fprintf(stderr, "Before select\n");
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        //fprintf(stderr, "Activity: %d\n", activity);
        if (activity < 0)
            continue;

        if (FD_ISSET(server->socket_fd, &readfds)) {
            fprintf(stderr, "Have message for server socket\n");
            server->connections_creator->do_receive_message();
        }

        server->mtx_connections.lock();
        for (auto elem : server->connections) {
            Connection* con = elem.second;
            if (FD_ISSET(con->socket_fd, &readfds)) {
                ssize_t res = con->do_background_recv();
                if (res < 0) {
                    fprintf(stderr, "%s\n", "Error when receive to buffer");
                }
            }

            gettimeofday(&tv, NULL);
            int current_time = (int)tv.tv_usec + (int)tv.tv_sec * 1000000;
            int diff_time = abs(current_time - con->last_time_send_message);
            if (diff_time > CUSTOM_PERIOD_SEND_DATA) {
                con->last_time_send_message = current_time;
                ssize_t res = con->do_background_send();
                if (res < 0) {
                    fprintf(stderr, "%s\n", "Error when send from buffer");
                }
            }
        }
        server->mtx_connections.unlock();
    }
}

void TCP_Server::do_listen() {
    listen_flag = true;
    listen_thread = new std::thread(background_send_receive_thread_function, this);
}

/*int TCP_Server::do_accept() {
    Connection* connection = NULL;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    struct sockaddr_in addr;
    struct sockaddr_in his_addr;

    uint32_t source_ip; // ip new connection
    unsigned short source_port; // port new connection
    while (1) {
        fprintf(stderr, "Server:\n");
        ssize_t size = recvfrom(socket_fd, buf, CUSTOM_SERVER_BUFFER_SIZE, 0, (struct sockaddr *) &addr, &addr_size);
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
            connection->flag_active = true;
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

int TCP_Server::do_accept() {
    fprintf(stderr, "Call accept\n");
    Connection* connection = connections_creator->get_connectoin_from_queue();
    fprintf(stderr, "!!! Connection taken from creator\n");

    mtx_connections.lock();
    int pipe_read_side = connection->pipe_buffer_recv->get_read_side();
    connections[pipe_read_side] = connection;
    int con_ip = ntohl(connection->his_addr.sin_addr.s_addr);
    unsigned short con_port = ntohs(connection->his_addr.sin_port);
    m_connections[Ip_Port(con_ip, con_port)] = connection->socket_fd;
    mtx_connections.unlock();

    fprintf(stderr, "New connection has established\n");
    return pipe_read_side;
}

ssize_t TCP_Server::do_recv(int socket_fd, void* buf, size_t size) {
    if (!connections.count(socket_fd)) {
        fprintf(stderr, "%s\n", "No such socket descriptor in connections.");
        return -1;
    }
    Connection* con = connections[socket_fd];

    ssize_t res_size = con->pipe_buffer_recv->do_read_to(buf, size);
    return res_size;
}

ssize_t TCP_Server::do_send(int socket_fd, void* buf, size_t size) {
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
    delete connections_creator;
    free(buf);
}
