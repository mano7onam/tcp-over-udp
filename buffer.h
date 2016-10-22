//
// Created by mano on 22.10.16.
//

#ifndef PRJ_BUFFER_H
#define PRJ_BUFFER_H

#include "global_definitions.h"

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

#endif //PRJ_BUFFER_H
