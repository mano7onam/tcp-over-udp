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

    if (pipe(pipe_fd) == -1)
        throw ConnectionException("Can't open pipe.");

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
}

int get_int_from_buffer(void* buf, int offs) {
    return (int)ntohl(((uint32_t *)((char*)buf + offs))[0]);
}

void push_int_to_buffer(int val, void* buf, int offs) {
    uint32_t nval = htonl((uint32_t)val);
    ((uint32_t*)((char*)buf + offs))[0] = nval;
}

int Connection::do_background_recv() {
    sockaddr_in addr;
    bzero(&addr, sizeof(struct sockaddr_in));
    int size = recvfrom(socket_fd, tmp_buf, sz_tmp_buf, 0, (struct sockaddr *) &addr, &addr_size);
    if (size < 16) {
        fprintf(stderr, "Error size: %d\n", size);
        return -1;
    }

    // check information about delivery sent messages
    int r_send_message_number = get_int_from_buffer(tmp_buf, 0);
    int r_send_message_size = get_int_from_buffer(tmp_buf, 4);
    if (r_send_message_number == send_message_number) {
        std::unique_lock<std::mutex> lock(mtx_send);
        fprintf(stderr, "Message number: %d\n", r_send_message_number);
        fprintf(stderr, "Last message size: %d\n", r_send_message_size);
        send_buf->move_buffer(r_send_message_size);
        fprintf(stderr, "Size buffer: %d\n", send_buf->get_size());
        ++send_message_number;
    }

    // if datagram have data try to receive it
    int r_recv_message_number = get_int_from_buffer(tmp_buf, 8);
    int r_recv_message_size = get_int_from_buffer(tmp_buf, 12);
    if (r_recv_message_number < 0 || r_recv_message_size < 0) {
        if (r_recv_message_number == -1 && r_recv_message_size == -1)
            return 0;
        else
            return -1;
    }
    if (r_recv_message_number == recv_message_number && r_recv_message_size > 0) {
        if (size < 16 + r_recv_message_size) {
            fprintf(stderr, "Error format receive... R_recv_message: %d\n", r_recv_message_number);
            return -1;
        }
        fprintf(stderr, "Have data number %d, size %d\n", r_recv_message_number, r_recv_message_size);
        std::unique_lock<std::mutex> lock(mtx_recv);
        if (recv_buf->get_rest_capacity() < r_recv_message_size){
            cv_recv.notify_one();
            return 0;
        }
        fprintf(stderr, "Pushed data size %d\n", r_recv_message_size);
        recv_buf->push_data((void*)((char*)tmp_buf + 16), r_recv_message_size);
        last_recv_message_size = r_recv_message_size;
        ++recv_message_number;
        write(pipe_fd[1], tmp_buf, 4); // signal if poll or select on pipe
        cv_recv.notify_one();
    }
    //fprintf(stderr, "Num and size: %d %d\n", r_recv_message_number, r_recv_message_size);
    return r_recv_message_size;
}

int Connection::do_background_send() {
    // send information about delivery message from
    // send that we receive last message and size this message
    push_int_to_buffer(recv_message_number - 1, tmp_buf, 0);
    push_int_to_buffer(last_recv_message_size, tmp_buf, 4);

    // if datagram have data receive it
    std::unique_lock<std::mutex> lock(mtx_send);
    int size_sent_data = 0;
    if (!(send_buf->is_empty())) {
        size_sent_data = send_buf->get_data((void*)((char*)tmp_buf + 16), 200, false);
        push_int_to_buffer(send_message_number, tmp_buf, 8);
        push_int_to_buffer(size_sent_data, tmp_buf, 12);
    }
    else {
        push_int_to_buffer(-1, tmp_buf, 8);
        push_int_to_buffer(-1, tmp_buf, 12);
    }
    //fprintf(stderr, "Background send size data: %d, num data: %d\n", size_sent_data, send_message_number);
    ssize_t result = sendto(socket_fd, tmp_buf, 16 + size_sent_data, 0,
                        (struct sockaddr *) &(his_addr), sizeof(struct sockaddr_in));
    if (result < 0)
        perror("Background send");

    return size_sent_data;
}

Connection::~Connection() {
    close(socket_fd);
    free(tmp_buf);
    delete recv_buf;
    delete send_buf;
}