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
    //fprintf(stderr, "Result port: %d\n", my_port);

    is_closed_me = false;
    is_closed_he = false;
    is_active = true;
    flag_set = false;
    flag_ready_to_delete = false;
    flag_accepted = false;
}

void Connection::do_close_connection() {
    is_active = false;
    pipe_buffer_recv->do_write_from(tmp_buf, 4);
    cv_send.notify_one();
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
    ssize_t size = recvfrom(socket_fd, tmp_buf, (size_t)sz_tmp_buf, 0, (struct sockaddr *) &addr, &addr_size);
    //fprintf(stderr, "Size recv: %ld\n", size);
    if (size < 20) {
        //fprintf(stderr, "Error size: %ld\n", size);
        return -1;
    }

    int type_recv = get_int_from_buffer(tmp_buf, 0);
    //fprintf(stderr, "Type recv: %d\n", type_recv);
    if (type_recv == -2) {
        //fprintf(stderr, "Receive shutdown connection signal\n");
        return -2; // signal to close receive side connection
    }
    else if (type_recv == -3) {
        //fprintf(stderr, "Receive close connection signal\n");
        is_closed_he = true;
        return -3; // signal to close all connection
    }

    // check information about delivery sent messages
    int r_send_message_number = get_int_from_buffer(tmp_buf, 4);
    int r_send_message_size = get_int_from_buffer(tmp_buf, 8);

    //fprintf(stderr, "%d %d %d\n", type_recv, r_send_message_number, r_send_message_size);
    if (r_send_message_number == send_message_number) {
        std::unique_lock<std::mutex> lock(mtx_send);
        //fprintf(stderr, "Message number: %d\n", r_send_message_number);
        //fprintf(stderr, "Last message size: %d\n", r_send_message_size);
        send_buf->move_buffer(r_send_message_size);
        //fprintf(stderr, "Size buffer: %d\n", send_buf->get_size());
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
        //fprintf(stderr, "Ravno!\n");
        if (size - 20 > 0) {
            char data[10000];
            memcpy(data, (char*)tmp_buf + 20, size - 20);
            data[size - 20] = '\0';
            //fprintf(stderr, "Data: %s\n", data);
            int hash = 0;
            for (int ii = 0; ii < size - 20; ++ii) {
                hash += ii * data[ii];
            }
            //fprintf(stderr, "Size: %d, Hash: %d\n", size - 20, hash);
        }
        if (size < 20 + r_recv_message_size) {
            //fprintf(stderr, "Error format receive... R_recv_message: %d\n", r_recv_message_number);
            return -1;
        }
        //fprintf(stderr, "Have data number %d, size %d\n", r_recv_message_number, r_recv_message_size);

        //fprintf(stderr, "Before recv from pipe\n");
        ssize_t size_write;
        if (pipe_buffer_recv->get_size_pipe_buf() < 10000) {
            size_write = pipe_buffer_recv->do_write_from((void *) ((char *) tmp_buf + 20), r_recv_message_size);
        }
        else {
            return 0;
        }
        //fprintf(stderr, "After recv from pipe\n");

        ++recv_message_number;
        last_recv_message_size = r_recv_message_size;
        cv_recv.notify_one();

        return size_write;
    }
    return 0;
}

ssize_t Connection::do_background_send(int type_send) {
    // send type send int value have control flags...
    //fprintf(stderr, "Send type: %d\n", type_send);
    push_int_to_buffer(type_send, tmp_buf, 0);

    // send information about delivery message from
    // send that we receive last message and size this message
    push_int_to_buffer(recv_message_number - 1, tmp_buf, 4);
    push_int_to_buffer(last_recv_message_size, tmp_buf, 8);

    // if datagram have data receive it
    std::unique_lock<std::mutex> lock(mtx_send);
    int size_sent_data = 0;

    if (!(send_buf->is_empty())) {
        size_sent_data = send_buf->get_data((void*)((char*)tmp_buf + 20), CUSTOM_CONNECTION_TMP_BUFFER_SIZE - 21, false);
        //fprintf(stderr, "Size sent data: %d\n", size_sent_data);
        char mssg[10000];
        memcpy(mssg, tmp_buf, size_sent_data);
        mssg[size_sent_data] = '\0';
        //fprintf(stderr, "%s\n", mssg);
        int hash = 0;
        for (int ii = 0; ii < size_sent_data; ++ii) {
            hash += ii * mssg[ii];
        }
        //fprintf(stderr, "Size %d, Hash: %d\n", size_sent_data, hash);
        push_int_to_buffer(send_message_number, tmp_buf, 12);
        //fprintf(stderr, "Send message number: %d\n", send_message_number);
        push_int_to_buffer(size_sent_data, tmp_buf, 16);
        if (size_sent_data) {
            cv_send.notify_one();
        }
    }
    else {
        push_int_to_buffer(-1, tmp_buf, 12);
        push_int_to_buffer(-1, tmp_buf, 16);
        cv_send.notify_one();
    }

    //fprintf(stderr, "%d %d %d %d %d\n", type_send, recv_message_number - 1, last_recv_message_size,
    //       send_message_number, size_sent_data);
    //fprintf(stderr, "Background send size data: %d, num data: %d\n", size_sent_data, send_message_number);
    ssize_t result = sendto(socket_fd, tmp_buf, 20 + size_sent_data, 0,
                        (struct sockaddr *) &(his_addr), sizeof(struct sockaddr_in));
    //fprintf(stderr, "Sent successfully\n");

    if (result < 0)
        perror("Background send");

    return size_sent_data;
}

Connection::~Connection() {
    fprintf(stderr, "Connection destructor\n");
    close(socket_fd);
    free(tmp_buf);
    delete recv_buf;
    delete pipe_buffer_recv;
    delete send_buf;
}