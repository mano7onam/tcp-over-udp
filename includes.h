//
// Created by mano on 18.10.16.
//

#ifndef PRJ_INCLUDES_H
#define PRJ_INCLUDES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <map>
#include <thread>
#include <set>
#include <queue>
#include <cstdint>
#include <ctime>

#define CUSTOM_SERVER_BUFFER_SIZE 1024
#define CUSTOM_CONNECTION_TMP_BUFFER_SIZE 1024
#define CUSTOM_CONNECTION_RECV_BUFFER_SIZE 8388608
#define CUSTOM_CONNECTION_SEND_BUFFER_SIZE 8388608

#endif //PRJ_INCLUDES_H
