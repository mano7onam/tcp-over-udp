#include "../tcp_client.h"

#define LOCALHOST
#ifdef LOCALHOST
#define IP_ADDRESS "localhost"
#define IP_ADDRESS_LEN (10)
#else
#define IP_ADDRESS "10.4.10.70"
    #define IP_ADDRESS_LEN (11)
#endif

#define BUFFER_SIZE (1024)
#define MAX_PATH_SIZE (1024)
#define PORT 4444
#define SIZE_MD5 (32)

#define min(x, y) ((x) < (y) ? (x) : (y))

int bptr = 0;
void* buf = malloc(BUFFER_SIZE + 1);
void* tmpbuf = malloc(BUFFER_SIZE + 1);
char* cwd;
int all_send = 0;
TCP_Client* tcp_client;

void do_open_cwd() {
    int size = pathconf(".", _PC_PATH_MAX) + MAX_PATH_SIZE;
    cwd = (char*)malloc(size);
    getcwd(cwd, size);
}

void send_data() {
    //int size = send(socket_fd, buf, BUFFER_SIZE, 0);
    int size = tcp_client->do_send(buf, BUFFER_SIZE);
    if (size <= 0) {
        //fprintf(stderr, "Sended: %d\n", size);
        //perror("send");
        return;
    }
    int rest = BUFFER_SIZE - size;
    memmove(buf, (char*)buf + size, rest);
    bptr = rest;
    all_send += size;
    fprintf(stderr, "All send %d bytes.\n bptr = %d\n", all_send, bptr);
}

void push_data(void* qbuf, int size) {
    fprintf(stderr, "Push data\n");
    int part, rest;
    do {
        part = min(size, BUFFER_SIZE - bptr);
        rest = size - part;

        memcpy((char*)buf + bptr, qbuf, part);
        bptr += part;
        if (rest != 0)
            memmove(qbuf, (char*)qbuf + part, rest);
        size = rest;

        //fprintf(stderr, "\n\n%s\n", (char*)buf + sizeof(int));
        if (bptr == BUFFER_SIZE)
            send_data();
    }
    while (rest);
}

void final_send() {
    char type_buf[1];
    type_buf[0] = 'x';
    push_data((void*)type_buf, 1);

    fprintf(stderr, "All send %d bytes.\n bptr = %d\n", all_send, bptr);
    fprintf(stderr, "%s\n", "Final send.");

    int size, rest;
    while (bptr != 0) {
        //size = send(socket_fd, buf, bptr, 0);
        fprintf(stderr, "AAA\n");
        size = tcp_client->do_send(buf, bptr);
        fprintf(stderr, "BBB %d\n", size);
        memmove(buf, (char*)buf + size, bptr - size);
        bptr -= size;
        all_send += size;
    }

    fprintf(stderr, "All send %d bytes.\n", all_send);
}

void send_name(char* name) {
    uint32_t size_name = strlen(name) + 1;
    uint32_t size_name_buf[1];
    size_name_buf[0] = htonl(size_name);
    push_data((void*)size_name_buf, sizeof(uint32_t));

    char* cop_name = strdup(name);
    push_data((void*)cop_name, strlen(name) + 1);
    free(cop_name);
}

void send_file_info(char* file_path) {
    unsigned char md5buf[SIZE_MD5 + 1];
    struct stat st;
    int size;

    if (stat(file_path, &st) != 0){
        perror("send_file_info stat");
        return;
    }
    uint32_t size_file_buf[1];
    size_file_buf[0] = htonl((uint32_t)st.st_size);
    push_data((void*)size_file_buf, sizeof(uint32_t));

    char* command = (char*)malloc(strlen(file_path) + 50);
    strcat(strcpy(command, "md5sum "), file_path);
    FILE* pipe = popen(command, "r");
    if (pipe == NULL) {
        perror("send_file_info md5");
        return;
    }
    size = fread((void*)md5buf, sizeof(char), SIZE_MD5, pipe);
    if (size != SIZE_MD5) {
        perror("send_file_info md5");
        return;
    }
    fclose(pipe);
    free(command);
    md5buf[SIZE_MD5] = '\0';
    push_data((void*)md5buf, SIZE_MD5 + 1);
}

void send_file(char* relative_path) {
    fprintf(stderr, "Send file: %s\n", relative_path);
    char type_buf[1];
    type_buf[0] = 'f';
    push_data((void*)type_buf, 1);

    send_name(relative_path);

    int fd;
    char* file_path = NULL;
    file_path = (char*)malloc(strlen(cwd) + strlen(relative_path) + 2);
    if (relative_path[0] == '/') {
        strcpy(file_path, relative_path);
    }
    else {
        strcat(strcat(strcpy(file_path, cwd), "/"), relative_path);
    }


    fprintf(stderr, "%s\n", file_path);
    send_file_info(file_path);
    fd = open(file_path, O_RDONLY, 0);

    int size;
    while ((size = read(fd, tmpbuf, BUFFER_SIZE)) > 0)
        push_data(tmpbuf, size);

    close(fd);
}

void send_directory(char* relative_path) {
    fprintf(stderr, "Send directory: %s\n", relative_path);
    char type_buf[1];
    type_buf[0] = 'd';
    push_data((void*)type_buf, 1);

    send_name(relative_path);

    char* dir_path = (char*)malloc(strlen(cwd) + strlen(relative_path) + 2);
    strcat(strcat(strcpy(dir_path, cwd), "/"), relative_path);

    DIR* pdir = NULL;
    struct stat stat;
    size_t max_fname_length;

    pdir = opendir(dir_path);
    fprintf(stderr, "%s\n", dir_path);
    if (pdir == NULL) {
        perror("send_directory");
        return;
    }

    max_fname_length = pathconf(dir_path, _PC_NAME_MAX);
    while (1) {
        char dirent_allocated_space[sizeof(struct dirent)+max_fname_length+2];
        struct dirent *entry = (struct dirent*)dirent_allocated_space;
        struct dirent *result;

        readdir_r(pdir, entry, &result);
        if (result == NULL)
            break;
        if (strcmp(result->d_name, ".") == 0 || strcmp(result->d_name, "..") == 0)
            continue;

        char* relativepathname;
        char absolutepathname[strlen(dir_path)+strlen(result->d_name)+2];
        strcat(strcat(strcpy(absolutepathname, dir_path), "/"), result->d_name);
        lstat(absolutepathname, &stat);
        relativepathname = strdup(absolutepathname + strlen(dir_path) - strlen(relative_path));

        if (S_ISREG(stat.st_mode))
            send_file(relativepathname);
        else if (S_ISDIR(stat.st_mode))
            send_directory(relativepathname);

        free(relativepathname);
    }

    closedir(pdir);
    free(dir_path);
}

void init_connection(unsigned short port) {
    std::string address = IP_ADDRESS;
    tcp_client = new TCP_Client(port);
    tcp_client->do_connect(address, PORT);
}

void final_function() {
    delete tcp_client;
    free(cwd);
    free(buf);
    free(tmpbuf);
}

int main(int argc, char *argv[])
{
    do_open_cwd();

    int opt = 0;
    char* name;
    char type;
    unsigned short port = 0;
    while ((opt = getopt(argc, argv, "p:f:d:")) != -1) {
        switch (opt) {
            case 'p':
                port = (unsigned short)atoi(optarg);
                break;
            case 'f':
                name = optarg;
                type = 'f';
                printf("%s\n", name);
                break;
            case 'd':
                name = optarg;
                type = 'd';
                printf("%s\n", name);
                break;
            default:
                fprintf(stderr, "Unknown parameter\n");
                exit(EXIT_FAILURE);
        }
    }

    init_connection(port);
    if (type == 'f') {
        send_file(name);
        fprintf(stderr, "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO\n");
        final_send(); // flush
    }
    else if (type == 'd') {
        send_directory(name);
        final_send(); // flush
    }
    final_function();

    return EXIT_SUCCESS;
}
