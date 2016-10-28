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
    ssize_t size_pipe_buf;
    int pipe_fd[2];
    std::mutex mtx;
    std::condition_variable cv;

    Pipe_Buffer() {
        size_pipe_buf = 0;
        if (pipe(pipe_fd) == -1) {
            perror("Pipe_Buffer");
            throw Pipe_Buffer_Exception("Can't open pipe.");
        }
    }

    ~Pipe_Buffer() {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
    }

    int get_read_side() { return pipe_fd[0]; }

    int get_write_side() { return pipe_fd[1]; }

    ssize_t get_size_pipe_buf() {
        std::unique_lock<std::mutex> lock(mtx);
        return size_pipe_buf;
    }

    ssize_t do_read_to(void *buf, size_t size) {
        std::unique_lock<std::mutex> lock(mtx);
        while (size_pipe_buf == 0) {
            cv.wait(lock);
        }
        fprintf(stderr, "Read from pipe %ld\n", size);
        ssize_t real_size = read(get_read_side(), buf, size);
        size_pipe_buf -= real_size;
        return real_size;
    }

    ssize_t do_write_from(void *buf, size_t size) {
        std::unique_lock<std::mutex> lock(mtx);
        fprintf(stderr, "Write to pipe %ld\n", size);
        ssize_t real_size = write(get_write_side(), buf, size);
        size_pipe_buf += real_size;
        cv.notify_one();
        return real_size;
    }
};

#endif //PRJ_TCP_SOCKET_H
