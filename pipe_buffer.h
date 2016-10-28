//
// Created by mano on 22.10.16.
//

#ifndef PRJ_TCP_SOCKET_H
#define PRJ_TCP_SOCKET_H

#include "global_definitions.h"

struct Pipe_Buffer_Exception : public std::exception {
    std::string s;
    Pipe_Buffer_Exception(std::string ss) : s(ss) {}
    ~Pipe_Buffer_Exception() throw () {} // Updated
    const char* what() const throw() { return s.c_str(); }
};

// thread-safe read and write
struct Pipe_Buffer{
    int pipe_fd[2];
    std::mutex mtx;

    Pipe_Buffer() {
        if (pipe(pipe_fd) == -1) {
            perror("Pipe_Buffer");
            throw Pipe_Buffer_Exception("Can't open pipe.");
        }
    }

    int get_read_side() { return pipe_fd[0]; }

    int get_write_side() { return pipe_fd[1]; }

    ssize_t do_read_to(void *buf, size_t size) {
        std::unique_lock<std::mutex> lock(mtx);
        fprintf(stderr, "Read from pipe %d\n", size);
        return read(get_read_side(), buf, size);
    }

    ssize_t do_write_from(void *buf, size_t size) {
        std::unique_lock<std::mutex> lock(mtx);
        fprintf(stderr, "Write to pipe %d\n", size);
        return write(get_write_side(), buf, size);
    }
};

#endif //PRJ_TCP_SOCKET_H
