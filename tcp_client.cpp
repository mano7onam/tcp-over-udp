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

TCP_Client::TCP_Client(unsigned short port) {
    socket_fd = 0;
    struct sockaddr_in s_addr;
    struct sockaddr_in addr_his;
    struct hostent *host_info = NULL;

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

    buf = malloc(CUSTOM_BUFFER_SIZE);
    pbuf = 0;

    listen_flag = false;
    listen_thread = NULL;
}

int TCP_Client::set_connection_not_active(std::string cause) {
        fprintf(stderr, "Close connection because: [%s]\n", cause.c_str());
    connection->do_close_connection();
    listen_flag = false;
    return 1;
}

void background_send_receive_thread_function(TCP_Client *client) {
    bool have_send_data = false;
    while (client->listen_flag || have_send_data) {
        have_send_data = false;

        // for work with time
        struct timeval tv;
        int current_time;
        int diff_time;

        fd_set readfds;
        int max_fd = -1;
        FD_ZERO(&readfds);
        Connection* con = client->connection;
        if (NULL == con)
            break;
        FD_SET(con->socket_fd, &readfds);

        if (max_fd < con->socket_fd)
            max_fd = con->socket_fd;
        if (-1 == max_fd)
            continue;

        tv = {0, CUSTOM_SELECT_TIMEOUT};
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        //fprintf(stderr, "Activity: %d\n", activity);
        if (activity < 0)
            continue;

        client->mtx_connection.lock();
        if (FD_ISSET(con->socket_fd, &readfds)) {
            //fprintf(stderr, "BEFOREEEE BACKGR\n");
            ssize_t res = con->do_background_recv();
            //fprintf(stderr, "AFTER BACKGR\n");
            if (res == -1) {
                fprintf(stderr, "%s\n", "Error when receive to buffer");
            }

            gettimeofday(&tv, NULL);
            current_time = (int)tv.tv_usec + (int)tv.tv_sec * 1000000;
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
            current_time = (int)tv.tv_usec + (int)tv.tv_sec * 1000000;
            diff_time = abs(current_time - con->last_time_recv_message);
            if (con->last_time_recv_message == 0 || con->last_time_recv_message > current_time) {
                con->last_time_recv_message = current_time;
            }
            else if (diff_time > CUSTOM_RECEIVE_TIMEOUT) {
                client->set_connection_not_active("Timeout");
                continue;
            }
        }

        gettimeofday(&tv, NULL);
        current_time = (int)tv.tv_usec + (int)tv.tv_sec * 1000000;
        diff_time = abs(current_time - con->last_time_send_message);
        if (diff_time > CUSTOM_PERIOD_SEND_DATA) {
            con->last_time_send_message = current_time;
            ssize_t res = con->do_background_send(0);
            if (res < 0) {
                fprintf(stderr, "%s\n", "Error when send from buffer");
            }
            else if (res == 0 && con->is_closed_me) {
                client->set_connection_not_active("Closed me");
            }
            else if (res > 0){
                have_send_data = true;
            }
        }
        client->mtx_connection.unlock();
    }
}

struct sockaddr_in get_addr_from_ip_port(std::string ip_addr, unsigned short port) {
    unsigned short port_his = port;
    struct hostent *host_info = NULL;
    struct sockaddr_in addr_his;
    addr_his.sin_family = PF_INET;
    addr_his.sin_port = htons(port_his);
    host_info = gethostbyname(ip_addr.c_str());
    memcpy(&addr_his.sin_addr, host_info->h_addr, host_info->h_length);
    return addr_his;
}

int TCP_Client::do_connect(std::string ip_addr, unsigned short port) {
    struct sockaddr_in his_addr = get_addr_from_ip_port(ip_addr, port);

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
        fprintf(stderr, "Size: %d\n", size);
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
    std::unique_lock<std::mutex> lock_connections(mtx_connection);
    if (!connection->is_active) {
        listen_flag = false;
        return 0;
    }

    //fprintf(stderr, "BEFOREEEEEEEEEEEEEEEEEEEEEEE\n");
    ssize_t res_size = connection->pipe_buffer_recv->do_read_to(buf, size);
    //fprintf(stderr, "AFTERRRRRRRRRRRRRRRRRRRRRRRRRRR\n");

    return res_size;
}

ssize_t TCP_Client::do_send(void *buf, size_t size) {
    std::unique_lock<std::mutex> lock(connection->mtx_send);

    if (connection->is_closed_me || connection->is_closed_he) {
        fprintf(stderr, "Connections is closed.\n");
        return -1;
    }

    while (connection->send_buf->is_full() && connection->is_active) {
        connection->cv_send.wait(lock);
    }
    if (!connection->is_active) {
        listen_flag = false;
        return 0;
    }

    int pushed = connection->send_buf->push_data(buf, size);
    return pushed;
}

TCP_Client::~TCP_Client() {
    fprintf(stderr, "Destructor client\n");
    listen_flag = false;
    listen_thread->join();
    delete listen_thread;
    delete connection;
    free(buf);
}