//
// Created by mano on 18.10.16.
//

#ifndef PRJ_CONNECTION_H
#define PRJ_CONNECTION_H

#include "global_definitions.h"
#include "buffer.h"
#include "pipe_buffer.h"

struct ConnectionException : public std::exception {
    std::string s;
    ConnectionException(std::string ss) : s(ss) {}
    ~ConnectionException() throw () {} // Updated
    const char* what() const throw() { return s.c_str(); }
};

struct Connection {
    int socket_fd;
    struct sockaddr_in his_addr;
    struct sockaddr_in my_addr;
    socklen_t addr_size;

    // buffer to receive and to send
    void* tmp_buf;
    int p_tmp_buf;
    int sz_tmp_buf;

    std::mutex mtx_recv;
    std::condition_variable cv_recv;
    int recv_message_number;
    int last_recv_message_size; // size of last chunk of received data
    int last_time_recv_message;
    Buffer* recv_buf;

    std::mutex mtx_send;
    std::condition_variable cv_send;
    int send_message_number; // order number sent data
    int last_time_send_message;
    Buffer* send_buf;

    // descriptor for user's select or poll
    //int pipe_fd[2];
    Pipe_Buffer *pipe_buffer_recv;

    Ip_Port ip_port_id; // todo delete this crutch!!!

    bool is_closed_me;
    bool is_closed_he;
    bool is_active;

    bool flag_accepted;
    std::mutex flag_accepted_mtx;
    void do_set_accepted() {
        flag_accepted_mtx.lock();
        flag_accepted = true;
        flag_accepted_mtx.unlock();
    }
    bool is_accepted() {
        flag_accepted_mtx.lock();
        bool result = flag_accepted;
        flag_accepted_mtx.unlock();
        return result;
    }

    bool flag_set;

    bool flag_ready_to_delete;
    std::mutex flag_ready_to_delete_mtx;
    void do_set_ready_to_delete() {
        flag_ready_to_delete_mtx.lock();
        is_active = false;
        flag_ready_to_delete = true;
        flag_ready_to_delete_mtx.unlock();
    }
    bool is_ready_to_delete() {
        flag_ready_to_delete_mtx.lock();
        bool result = flag_ready_to_delete;
        flag_ready_to_delete_mtx.unlock();
        return result;
    }

    void do_close_connection();

    Connection(int size_tmp_buf, int size_recv_buf, int size_send_buf,
               unsigned short start_port, struct sockaddr_in his_addr);

    ssize_t do_background_recv();

    ssize_t do_background_send(int type_send);

    // todo delete this crutch!!!
    void set_his_port(unsigned short port) { his_addr.sin_port = port; }

    ~Connection();
};


#endif //PRJ_CONNECTION_H
