// SoulFu Master Server
// A simple TCP server that tracks hosted games for matchmaking.
// Clients connect, register their game, or query for available games.
//
// Protocol (all values big-endian):
//   Packet format: [1 byte type] [payload...]
//
//   Type 0x01 - REGISTER_GAME
//     Client -> Server: [type][2 bytes port]
//     Server adds/updates the game entry for this client's IP
//
//   Type 0x02 - UNREGISTER_GAME
//     Client -> Server: [type]
//     Server removes the game entry for this client's IP
//
//   Type 0x03 - QUERY_GAMES
//     Client -> Server: [type]
//     Server -> Client: [type][2 bytes count][for each: 4 bytes IP, 2 bytes port, 2 bytes players]
//
//   Type 0x04 - UPDATE_PLAYERS
//     Client -> Server: [type][2 bytes player_count]
//     Server updates the player count for this client's game
//
// Build: cc -o master_server master_server.c
// Usage: ./master_server [port]  (default port: 30628)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 30628
#define MAX_GAMES 256
#define MAX_CLIENTS 64
#define GAME_TIMEOUT 120  // Seconds before a game is removed if not refreshed
#define RECV_BUF_SIZE 256

#define MSG_REGISTER    0x01
#define MSG_UNREGISTER  0x02
#define MSG_QUERY       0x03
#define MSG_UPDATE      0x04

typedef struct {
    unsigned int ip;
    unsigned short port;
    unsigned short players;
    time_t last_seen;
    int active;
} game_entry_t;

static game_entry_t games[MAX_GAMES];
static int num_games = 0;
static int running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

static int find_game_by_ip(unsigned int ip)
{
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].active && games[i].ip == ip)
            return i;
    }
    return -1;
}

static int add_game(unsigned int ip, unsigned short port)
{
    int idx = find_game_by_ip(ip);
    if (idx >= 0) {
        games[idx].port = port;
        games[idx].last_seen = time(NULL);
        return idx;
    }
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!games[i].active) {
            games[i].ip = ip;
            games[i].port = port;
            games[i].players = 1;
            games[i].last_seen = time(NULL);
            games[i].active = 1;
            num_games++;
            return i;
        }
    }
    return -1;
}

static void remove_game(unsigned int ip)
{
    int idx = find_game_by_ip(ip);
    if (idx >= 0) {
        games[idx].active = 0;
        num_games--;
    }
}

static void expire_games(void)
{
    time_t now = time(NULL);
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].active && (now - games[i].last_seen) > GAME_TIMEOUT) {
            struct in_addr addr;
            addr.s_addr = games[i].ip;
            printf("[INFO] Game from %s expired\n", inet_ntoa(addr));
            games[i].active = 0;
            num_games--;
        }
    }
}

static void handle_client(int client_fd, struct sockaddr_in *client_addr)
{
    unsigned char buf[RECV_BUF_SIZE];
    unsigned char reply[4 + MAX_GAMES * 8];
    int n;
    unsigned int client_ip = client_addr->sin_addr.s_addr;
    struct in_addr addr;
    addr.s_addr = client_ip;

    n = recv(client_fd, buf, RECV_BUF_SIZE, 0);
    if (n <= 0) {
        close(client_fd);
        return;
    }

    switch (buf[0]) {
    case MSG_REGISTER:
        if (n >= 3) {
            unsigned short port = (buf[1] << 8) | buf[2];
            int idx = add_game(client_ip, port);
            if (idx >= 0) {
                printf("[INFO] Game registered from %s:%d\n", inet_ntoa(addr), port);
            }
        }
        break;

    case MSG_UNREGISTER:
        remove_game(client_ip);
        printf("[INFO] Game unregistered from %s\n", inet_ntoa(addr));
        break;

    case MSG_QUERY: {
        printf("[INFO] Game query from %s\n", inet_ntoa(addr));
        int count = 0;
        int pos = 3;  // Skip header + count
        for (int i = 0; i < MAX_GAMES && count < 255; i++) {
            if (games[i].active) {
                // IP (4 bytes, network byte order already)
                memcpy(&reply[pos], &games[i].ip, 4);
                pos += 4;
                // Port (2 bytes big-endian)
                reply[pos++] = (games[i].port >> 8) & 0xFF;
                reply[pos++] = games[i].port & 0xFF;
                // Players (2 bytes big-endian)
                reply[pos++] = (games[i].players >> 8) & 0xFF;
                reply[pos++] = games[i].players & 0xFF;
                count++;
            }
        }
        reply[0] = MSG_QUERY;
        reply[1] = (count >> 8) & 0xFF;
        reply[2] = count & 0xFF;
        send(client_fd, reply, pos, 0);
        break;
    }

    case MSG_UPDATE:
        if (n >= 3) {
            unsigned short players = (buf[1] << 8) | buf[2];
            int idx = find_game_by_ip(client_ip);
            if (idx >= 0) {
                games[idx].players = players;
                games[idx].last_seen = time(NULL);
                printf("[INFO] Game from %s updated: %d players\n", inet_ntoa(addr), players);
            }
        }
        break;
    }

    close(client_fd);
}

int main(int argc, char *argv[])
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int port = DEFAULT_PORT;
    int opt = 1;

    if (argc > 1)
        port = atoi(argv[1]);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    memset(games, 0, sizeof(games));

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 16) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("SoulFu Master Server running on port %d\n", port);
    printf("Press Ctrl+C to stop\n");

    while (running) {
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int ret = select(server_fd + 1, &readfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        if (ret == 0) {
            // Timeout - expire old games
            expire_games();
            continue;
        }

        if (FD_ISSET(server_fd, &readfds)) {
            client_len = sizeof(client_addr);
            client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
            if (client_fd >= 0) {
                handle_client(client_fd, &client_addr);
            }
        }

        expire_games();
    }

    printf("\nShutting down...\n");
    close(server_fd);
    return 0;
}
