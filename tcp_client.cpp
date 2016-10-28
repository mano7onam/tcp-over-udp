//
// Created by mano on 18.10.16.
//

#include "tcp_client.h"

TCP_Client::TCP_Client(int socket_fd) {
    this->socket_fd = socket_fd;

    buf = malloc(CUSTOM_BUFFER_SIZE);
    pbuf = 0;

    listen_flag = false;
    listen_thread = NULL;
}

void background_send_receive_thread_function(TCP_Client *client) {
    while (client->listen_flag) {
        // for work with time
        struct timeval tv;

        fd_set readfds;
        int max_fd = -1;
        FD_ZERO(&readfds);
        Connection* con = client->connection;
        FD_SET(con->socket_fd, &readfds);

        if (max_fd < con->socket_fd)
            max_fd = con->socket_fd;
        if (-1 == max_fd)
            continue;

        tv = {0, CUSTOM_SELECT_TIMEOUT};
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0)
            continue;

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
}

int TCP_Client::do_connect(struct sockaddr_in his_addr) {
    socklen_t addr_size = sizeof(struct sockaddr_in);
    struct sockaddr_in addr;

    // create connection
    connection = NULL;
    try {
        connection = new Connection(CUSTOM_CONNECTION_TMP_BUFFER_SIZE,
                                    CUSTOM_CONNECTION_SEND_BUFFER_SIZE,
                                    CUSTOM_CONNECTION_RECV_BUFFER_SIZE,
                                    port_counter, his_addr);
    } catch(ConnectionException ex) {
        fprintf(stderr, "%s\n", ex.what());
        delete connection;
        return -1;
    }

    // send first handshake and receive response - second handshake
    while (1) {
        fprintf(stderr, "%s\n", "Send handshake: ");
        char handshake_client[4];
        handshake_client[0] = 0;
        handshake_client[1] = 0;
        ((unsigned short*)handshake_client)[1] = connection->my_addr.sin_port;
        sendto(socket_fd, (void*)handshake_client, 4, 0,
               (struct sockaddr *) &(connection->his_addr),
               sizeof(struct sockaddr_in));

        fd_set readfds;
        int max_desc;
        FD_ZERO(&readfds);
        FD_SET(socket_fd, &readfds);
        max_desc = socket_fd;
        struct timeval tv = {0, 2000000};
        int activity = select(max_desc + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0 || !FD_ISSET(socket_fd, &readfds))
            continue;

        int size = recvfrom(socket_fd, buf, CUSTOM_BUFFER_SIZE, 0, (struct sockaddr *) &addr, &addr_size);
        if (size < 4) {
            fprintf(stderr, "Size of package must be greater than 4\n");
            continue;
        }

        char flag_syn = ((char*)buf)[0];
        char flag_ack = ((char*)buf)[1];
        unsigned short his_port = ((unsigned short*)buf)[1];
        if (flag_syn != 1 || flag_ack != 1)
            continue;
        connection->set_his_port(his_port);

        break;
    }

    // send third handshake
    // SHOULD BE NOT THERE AND NOT THAT!
    for (int i = 0; i < 5; ++i) {
        char handshake_client[3];
        handshake_client[0] = 1;
        handshake_client[1] = 1;
        sendto(socket_fd, (void*)handshake_client, 2, 0,
               (struct sockaddr *) &(his_addr),
               sizeof(struct sockaddr_in));
        usleep(100);
    }

    // start to receive messages in new connection
    listen_flag = true;
    listen_thread = new std::thread(background_send_receive_thread_function, this);

    //return connection->pipe_fd[0];
    return connection->pipe_buffer_recv->get_read_side();
}

ssize_t TCP_Client::do_recv(void *buf, size_t size) {
    std::unique_lock<std::mutex> lock(connection->mtx_recv);
    while (connection->recv_buf->is_empty()) {
        connection->cv_recv.wait(lock);
    }

    //int res_size = connection->recv_buf->get_data(buf, size, true);

    ssize_t res_size = connection->pipe_buffer_recv->do_read_to(buf, size);

    return res_size;
}

ssize_t TCP_Client::do_send(void *buf, size_t size) {
    fprintf(stderr, "do_send aaa\n");
    std::unique_lock<std::mutex> lock(connection->mtx_send);
    fprintf(stderr, "do_send bbb\n");
    while (connection->send_buf->is_full()) {
        fprintf(stderr, "do_send ccc\n");
        connection->cv_send.wait(lock);
    }

    fprintf(stderr, "bbb\n");
    return connection->send_buf->push_data(buf, size);
}

TCP_Client::~TCP_Client() {
    fprintf(stderr, "Destructor client\n");
    listen_flag = false;
    listen_thread->join();
    delete listen_thread;
    delete connection;
    free(buf);
}