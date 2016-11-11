//
// Created by mano on 18.10.16.
//

#include <stdint.h>
#include "tcp_server.h"

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

    pbuf = 0;
    szbuf = CUSTOM_SERVER_BUFFER_SIZE;
    buf = malloc(szbuf);

    listen_flag = false;
    listen_thread = NULL;

    this->connections_creator = new Connections_Creator(socket_fd, &m_connections, &mtx_connections);
}

int TCP_Server::get_server_socket() {
    return connections_creator->pipe_fd[0];
}

int TCP_Server::set_connection_not_active(int id_connection, std::string cause) {
    if (!connections.count(id_connection)) {
        fprintf(stderr, "No such connection\n.");
        return -1;
    }
    fprintf(stderr, "Close connection because: [%s]\n", cause.c_str());
    Connection* con = connections[id_connection];
    con->do_close_connection();
    return 1;
}

void background_send_receive_thread_function(TCP_Server *server) {
    bool have_send_data = false;
    while (server->listen_flag || have_send_data) {
        have_send_data = false;

        struct timeval tv;
        int current_time;
        int diff_time;
        fd_set readfds;
        int max_fd;
        FD_ZERO(&readfds);

        FD_SET(server->socket_fd, &readfds);
        max_fd = server->socket_fd;

        auto elem = server->connections.begin();
        while (elem != server->connections.end()) {
            Connection* con = elem->second;
            if (con->is_ready_to_delete()) {
                server->mtx_connections.lock();
                server->connections.erase(elem++);
                server->m_connections.erase(con->ip_port_id);
                server->mtx_connections.unlock();
                delete con;
                server->cv_close_connections.notify_one();
                continue;
            }

            if (!con->is_active || !con->is_accepted()) {
                ++elem;
                continue;
            }
            if (!con->is_accepted()) {
                fprintf(stderr, "Not accepted\n");
                ++elem;
                continue;
            }

            FD_SET(con->socket_fd, &readfds);
            con->flag_set = true;
            if (max_fd < con->socket_fd) {
                max_fd = con->socket_fd;
            }

            ++elem;
            /*FD_SET(con->pipe_buffer_recv->pipe_fd[1], &readfds);
            //fprintf(stderr, "all set\n");
            if (max_fd < con->pipe_buffer_recv->pipe_fd[1]) {
                max_fd = con->pipe_buffer_recv->pipe_fd[1];
            }*/
        }

        tv = {0, CUSTOM_SELECT_TIMEOUT};
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0) {
            continue;
        }

        if (FD_ISSET(server->socket_fd, &readfds)) {
            fprintf(stderr, "Have message for server socket\n");
            server->mtx_connections.lock();
            server->connections_creator->do_receive_message();
            server->mtx_connections.unlock();
        }

        for (auto elem : server->connections) {
            Connection* con = elem.second;

            if (!con->is_active || !con->flag_set || con->is_ready_to_delete()) {
                fprintf(stderr, "Continue\n");
                continue;
            }
            con->flag_set = false;

            int aaa = FD_ISSET(con->socket_fd, &readfds);
            //int bbb = FD_ISSET(con->pipe_buffer_recv->pipe_fd[1], &readfds);
            //fprintf(stderr, "OOO: %d %d\n", aaa, bbb);
            if (aaa/* && bbb*/) {
                ssize_t res = con->do_background_recv();
                if (res == -1) {
                    fprintf(stderr, "%s\n", "Error when receive to buffer");
                    continue;
                }

                gettimeofday(&tv, NULL);
                current_time = (int)tv.tv_usec + (int)tv.tv_sec * SECOUND_TO_USEC;
                con->last_time_recv_message = current_time;
                if (res == -2) {
                    // todo shutdown mode
                }
                else if (res == -3) {
                    fprintf(stderr, "%s\n", "Error when receive to buffer");
                }
            }
            else {
                gettimeofday(&tv, NULL);
                current_time = (int)tv.tv_usec + (int)tv.tv_sec * SECOUND_TO_USEC;
                diff_time = abs(current_time - con->last_time_recv_message);
                if (con->last_time_recv_message == 0 || con->last_time_recv_message > current_time) {
                    con->last_time_recv_message = current_time;
                }
                else if (diff_time > CUSTOM_RECEIVE_TIMEOUT) {
                    server->set_connection_not_active(elem.first, "Timeout");
                    continue;
                }
            }

            gettimeofday(&tv, NULL);
            current_time = (int)tv.tv_usec + (int)tv.tv_sec * SECOUND_TO_USEC;
            diff_time = abs(current_time - con->last_time_send_message);
            if (diff_time > CUSTOM_PERIOD_SEND_DATA) {
                con->last_time_send_message = current_time;
                ssize_t res = con->do_background_send(SEND_WITH_DATA);
                if (res < 0) {
                    fprintf(stderr, "%s\n", "Error when send from buffer");
                }
                else if (res == 0 && con->is_closed_me) {
                    server->set_connection_not_active(elem.first, "Closed me");
                }
                else if (con->send_buf->get_size()){
                    have_send_data = true;
                }
            }
        }
    }
}

void TCP_Server::do_listen() {
    listen_flag = true;
    listen_thread = new std::thread(background_send_receive_thread_function, this);
}

int TCP_Server::do_accept(struct sockaddr_in & addr) {
    fprintf(stderr, "Call accept\n");
    Connection* connection = connections_creator->get_connectoin_from_queue();
    connection->do_set_accepted(); // set flag that user has received this connection
    fprintf(stderr, "!!! Connection taken from creator\n");

    mtx_connections.lock();
    int pipe_read_side = connection->pipe_buffer_recv->get_read_side();
    connections[pipe_read_side] = connection;
    int con_ip = ntohl(connection->his_addr.sin_addr.s_addr);
    unsigned short con_port = ntohs(connection->his_addr.sin_port);
    Ip_Port ip_port_id = Ip_Port(con_ip, con_port);
    m_connections[ip_port_id] = connection->socket_fd;
    connection->ip_port_id = ip_port_id;
    mtx_connections.unlock();

    fprintf(stderr, "New connection has established\n");
    addr = connection->his_addr;
    return pipe_read_side;
}

ssize_t TCP_Server::do_recv(int socket_fd, void* buf, size_t size) {
    std::unique_lock<std::mutex> lock_connections(mtx_connections);

    if (!connections.count(socket_fd)) {
        fprintf(stderr, "%s\n", "No such socket descriptor in connections.");
        return -1;
    }

    Connection* con = connections[socket_fd];
    if (!con->is_active) {
        con->do_set_ready_to_delete();
        return 0;
    }

    ssize_t res_size = con->pipe_buffer_recv->do_read_to(buf, size);
    return res_size;
}

ssize_t TCP_Server::do_send(int socket_fd, void* buf, size_t size) {
    std::unique_lock<std::mutex> lock_connections(mtx_connections);

    if (!connections.count(socket_fd)) {
        fprintf(stderr, "%s\n", "No such socket descriptor in connections.");
        return -1;
    }

    Connection* con = connections[socket_fd];
    if (con->is_closed_me || con->is_closed_he) {
        fprintf(stderr, "Connections is closed.\n");
        return -1;
    }

    std::unique_lock<std::mutex> lock_send(con->mtx_send);
    while (con->send_buf->is_full()) {
        con->cv_send.wait(lock_send);
    }

    return con->send_buf->push_data(buf, size);
}

int TCP_Server::do_close(int socket_fd) {
    std::unique_lock<std::mutex> lock_connections(mtx_connections);

    if (!connections.count(socket_fd)) {
        fprintf(stderr, "%s\n", "No such socket descriptor in connections.");
        return -1;
    }

    Connection* con = connections[socket_fd];
    con->do_set_ready_to_delete();
    while (connections.count(socket_fd)) {
        cv_close_connections.wait(lock_connections);
    }

    return 1;
}

TCP_Server::~TCP_Server() {
    fprintf(stderr, "Destructor server\n");
    listen_flag = false;
    listen_thread->join();
    delete listen_thread;
    for (auto elem : connections) {
        delete elem.second;
    }
    delete connections_creator;
    free(buf);
}
