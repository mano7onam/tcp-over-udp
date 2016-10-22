//
// Created by mano on 18.10.16.
//

#ifndef PRJ_CONNECTION_H
#define PRJ_CONNECTION_H


#pragma once
#include "includes.h"

struct ConnectionException : public std::exception {
    std::string s;
    ConnectionException(std::string ss) : s(ss) {}
    ~ConnectionException() throw () {} // Updated
    const char* what() const throw() { return s.c_str(); }
};

class Buffer {
    int p = 0;
    int sz = 0;
    void* buf = NULL;

public:
    Buffer(int size) {
        p = 0;
        sz = size;
        buf = malloc(size);
    }

    void* get_start() { return (void*)((char*)buf + p);	}

    int get_rest_capacity() { return sz - p;	}

    int get_size() { return p; }

    bool is_full() { return p == sz; }

    bool is_empty() { return p == 0; }

    void move_pointer(int offs) { p += offs; }

    int push_data(void* source_buf, int size) {
        int can_push = std::min(get_rest_capacity(), size);
        if (can_push == 0)
            return 0;
        memcpy(get_start(), source_buf, can_push);
        move_pointer(can_push);
        return can_push;
    }

    void move_buffer(int size) {
        if (size > sz)
            return;
        memmove(buf, (void*)((char*)buf + size), sz - size);
        p -= size;
    }

    int get_data(void* dest_buf, int want_take, bool flag_move) {
        int can_give = std::min(p, want_take);
        memcpy(dest_buf, buf, can_give);
        if (flag_move)
            move_buffer(can_give);
        return can_give;
    }

    void set_pointer(int new_p) { p = new_p; }

    ~Buffer() {
        free(buf);
    }
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
    Buffer* recv_buf;

    std::mutex mtx_send;
    std::condition_variable cv_send;
    int send_message_number; // order number sent data
    Buffer* send_buf;

    // descriptor for user's select or poll
    int pipe_fd[2];

    Connection(int size_tmp_buf, int size_recv_buf, int size_send_buf,
               unsigned short start_port, struct sockaddr_in his_addr);

    int do_background_recv();

    int do_background_send();

    // todo delete this kostyl!!!
    void set_his_port(unsigned short port) { his_addr.sin_port = port; }

    ~Connection();
};


#endif //PRJ_CONNECTION_H
