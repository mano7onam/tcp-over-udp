//
// Created by mano on 18.10.16.
//

#include "connection.h"

Connection::Connection(int size_tmp_buf, int size_recv_buf, int size_send_buf,
                       unsigned short start_port, struct sockaddr_in his_addr)
{
    sz_tmp_buf = size_tmp_buf;
    p_tmp_buf = 0;
    tmp_buf = malloc(sz_tmp_buf);

    recv_message_number = 0;
    last_recv_message_size = 0;
    recv_buf = new Buffer(size_recv_buf);
    last_time_recv_message = 0;

    last_time_send_message = 0;
    send_message_number = 0;
    send_buf = new Buffer(size_send_buf);

    this->his_addr = his_addr;

    bzero(&my_addr, sizeof(struct sockaddr_in));
    addr_size = sizeof(struct sockaddr_in);

    socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
    int broadcastEnable = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    if (socket_fd <= 0)
        throw ConnectionException("Can't create socket.");

    try {
        pipe_buffer_recv = new Pipe_Buffer();
    }
    catch (Pipe_Buffer_Exception ex) {
        throw ConnectionException("Can't open pipe.");
    }

    my_addr.sin_family = PF_INET;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // checking ports successively to find free one:
    unsigned short my_port = start_port;
    while (1) {
        my_addr.sin_port = htons(my_port);
        if (0 == bind(socket_fd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr_in)))
            break;
        ++my_port;
    }
    fprintf(stderr, "Result port: %d\n", my_port);

    is_closed_me = false;
    is_closed_he = false;
    is_active = true;
}

void Connection::do_close_connection() {
    is_active = false;
    pipe_buffer_recv->do_write_from(tmp_buf, 4);
}

int get_int_from_buffer(void* buf, int offs) {
    return (int)ntohl(((uint32_t *)((char*)buf + offs))[0]);
}

void push_int_to_buffer(int val, void* buf, int offs) {
    uint32_t nval = htonl((uint32_t)val);
    ((uint32_t*)((char*)buf + offs))[0] = nval;
}

ssize_t Connection::do_background_recv() {
    sockaddr_in addr;
    bzero(&addr, sizeof(struct sockaddr_in));
    ssize_t size = recvfrom(socket_fd, tmp_buf, sz_tmp_buf, 0, (struct sockaddr *) &addr, &addr_size);
    fprintf(stderr, "Size recv: %ld\n", size);
    if (size < 20) {
        fprintf(stderr, "Error size: %ld\n", size);
        return -1;
    }

    for (int i = 0; i < 5; ++i) {
        fprintf(stderr, "re %d ", ((int*)tmp_buf)[i]);
    }

    int type_recv = get_int_from_buffer(tmp_buf, 0);
    if (type_recv == -2) {
        fprintf(stderr, "Receive shutdown connection signal\n");
        return -2; // signal to close receive side connection
    }
    else if (type_recv == -3) {
        fprintf(stderr, "Receive close connection signal\n");
        is_closed_he = true;
        return -3; // signal to close all connection
    }

    // check information about delivery sent messages
    int r_send_message_number = get_int_from_buffer(tmp_buf, 4);
    int r_send_message_size = get_int_from_buffer(tmp_buf, 8);
    if (r_send_message_number == send_message_number) {
        std::unique_lock<std::mutex> lock(mtx_send);
        fprintf(stderr, "Message number: %d\n", r_send_message_number);
        fprintf(stderr, "Last message size: %d\n", r_send_message_size);
        send_buf->move_buffer(r_send_message_size);
        fprintf(stderr, "Size buffer: %d\n", send_buf->get_size());
        ++send_message_number;
    }

    // if datagram have data try to receive it
    int r_recv_message_number = get_int_from_buffer(tmp_buf, 12);
    int r_recv_message_size = get_int_from_buffer(tmp_buf, 16);
    if (r_recv_message_number < 0 || r_recv_message_size < 0) {
        if (r_recv_message_number == -1 && r_recv_message_size == -1)
            return 0;
        else
            return -1;
    }
    if (r_recv_message_number == recv_message_number && r_recv_message_size > 0) {
        if (size < 20 + r_recv_message_size) {
            fprintf(stderr, "Error format receive... R_recv_message: %d\n", r_recv_message_number);
            return -1;
        }
        fprintf(stderr, "Have data number %d, size %d\n", r_recv_message_number, r_recv_message_size);

        ssize_t size_write = pipe_buffer_recv->do_write_from((void *) ((char *) tmp_buf + 20), r_recv_message_size);

        ++recv_message_number;
        last_recv_message_size = r_recv_message_size;
        cv_recv.notify_one();

        return size_write;
    }
    return 0;
}

ssize_t Connection::do_background_send(int type_send) {
    // send type send int value have control flags...
    fprintf(stderr, "Send type: %d\n", type_send);
    push_int_to_buffer(type_send, tmp_buf, 0);

    // send information about delivery message from
    // send that we receive last message and size this message
    push_int_to_buffer(recv_message_number - 1, tmp_buf, 4);
    push_int_to_buffer(last_recv_message_size, tmp_buf, 8);

    // if datagram have data receive it
    std::unique_lock<std::mutex> lock(mtx_send);
    size_t size_sent_data = 0;

    if (!(send_buf->is_empty())) {
        size_sent_data = send_buf->get_data((void*)((char*)tmp_buf + 20), 200, false);
        push_int_to_buffer(send_message_number, tmp_buf, 12);
        push_int_to_buffer(size_sent_data, tmp_buf, 16);
    }
    else {
        push_int_to_buffer(-1, tmp_buf, 12);
        push_int_to_buffer(-1, tmp_buf, 16);
    }

    for (int i = 0; i < 5; ++i) {
        fprintf(stderr, "se %d ", ((int*)tmp_buf)[i]);
    }
    fprintf(stderr, "\n");
    //fprintf(stderr, "Background send size data: %d, num data: %d\n", size_sent_data, send_message_number);
    ssize_t result = sendto(socket_fd, tmp_buf, 20 + size_sent_data, 0,
                        (struct sockaddr *) &(his_addr), sizeof(struct sockaddr_in));

    if (result < 0)
        perror("Background send");

    return size_sent_data;
}

Connection::~Connection() {
    close(socket_fd);
    free(tmp_buf);
    delete recv_buf;
    delete pipe_buffer_recv;
    delete send_buf;
}