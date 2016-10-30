#include "../tcp_server.h"

#define BUFFER_SIZE (100000000)
#define PORT 4444
#define INTERVAL 1000000
#define MAX_CONNECT 100

using namespace std;

typedef long long ll;

vector<ll> bits_receiveds;
vector<ll> tnows;
vector<ll> tavgs;
vector<ll> times;
vector<int> csokets;
vector<struct sockaddr_in> addresses;

void do_print_speed_test(int sig) {
    bool was_active = false;
    for (int i = 0; i < csokets.size(); ++i) {
        if (csokets[i] == 0)
            continue;

        ++times[i];
        was_active = true;
        tnows[i] = (bits_receiveds[i] * 8ll) / 10000ll;
        tavgs[i] = (ll)(tavgs[i] * ((times[i] - 1) / (double)times[i]) + tnows[i] / (double)times[i]);
        fprintf(stderr, "%s:%d - Now: %lld.%lld(Mb/s), Avg: %lld.%lld(Mb/s)\n",
                inet_ntoa(addresses[i].sin_addr), ntohs(addresses[i].sin_port),
                tnows[i] / 100ll, tnows[i] % 100ll, tavgs[i] / 100ll, tavgs[i] % 100ll);
        fprintf(stderr, "Bits recv: %lld\n", bits_receiveds[i]);

        bits_receiveds[i] = 0;
    }
    if (was_active)
        fprintf(stderr, "\n");

    alarm(1);
}

int main(int argc, char *argv[])
{
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    void* buf = malloc(BUFFER_SIZE + 1);

    unsigned short port = PORT;
    TCP_Server tcp_server(port);
    int master_socket = tcp_server.get_server_socket();
    tcp_server.do_listen();
    fprintf(stderr, "Waiting connections...\n");

    signal(SIGALRM, do_print_speed_test);
    alarm(1);
    bool flaggggg = true;
    while (flaggggg)
    {
        fd_set readfds; // set of socket descriptions
        int max_sd;

        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        for (int i = 0; i < csokets.size(); ++i) {
            int sd = csokets[i];
            if (sd)
                FD_SET(sd, &readfds);
            if(sd > max_sd)
                max_sd = sd;
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0)
            continue;

        if (FD_ISSET(master_socket, &readfds)) {
            int new_socket = tcp_server.do_accept(addr);
            fprintf(stderr, "New connection, socket fd is %d, ip is : %s, port : %d\n",
                    new_socket, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

            bool was_added = false;
            for (int i = 0; i < csokets.size(); ++i) {
                if (csokets[i])
                    continue;
                csokets[i] = new_socket;
                addresses[i] = addr;
                bits_receiveds[i] = 0;
                tnows[i] = 0;
                tavgs[i] = 0;
                times[i] = 0;
                was_added = true;
                break;
            }
            if (!was_added) {
                csokets.push_back(new_socket);
                addresses.push_back(addr);
                bits_receiveds.push_back(0);
                tnows.push_back(0);
                tavgs.push_back(0);
                times.push_back(0);
            }
        }

        for (int i = 0; i < csokets.size(); ++i){
            int sd = csokets[i];
            if (!sd)
                continue;

            if (FD_ISSET(sd, &readfds)) {
                ssize_t size_read = 0;
                if ((size_read = tcp_server.do_recv(sd, buf, BUFFER_SIZE)) <= 0){
                    getpeername(sd, (struct sockaddr*)&addr, &addr_size);
                    printf("Host disconnected, ip: %s, port: %d\n",
                           inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                    close(sd);
                    csokets[i] = 0;
                }
                else {
                    bits_receiveds[i] += (ll)size_read;
                    //fprintf(stderr, "!!! Size recv: %d\n"), size_read;
                }
            }
        }
    }

    close(master_socket);
    free(buf);
    return EXIT_SUCCESS;
}
