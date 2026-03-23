/*
TODO:
Real file reading (instead of fake 'A' data)
Proper parsing of tracker file
Multi-peer chunk downloading
MD5 verification
Better request parsing (GET start end filename)
Error handling
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>

/* Link with Winsock library when using MSVC.
 * With MinGW, the Makefile links using -lws2_32. */
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif

typedef SOCKET socket_t;
#define CLOSESOCK(s) closesocket(s)

#define BUFFER_SIZE 2048
#define MAX_CHUNK 1024

/* Configuration values */
#define MAX_PATH_LEN 260
char shared_folder[MAX_PATH_LEN] = "./sharedFolder";
char tracker_ip[64] = "127.0.0.1";
int  tracker_port = 5000;
int  refresh_interval = 900; 
int peer_listen_port = 6000;

/* P2P server: accept thread + one worker thread per incoming peer connection. */
static volatile LONG g_peer_server_stop = 0;
static socket_t g_listen_sock = INVALID_SOCKET;
static HANDLE g_accept_thread = NULL;

// Removes comments from a line
static void strip_comment(char *line) {
    char *hash = strchr(line, '#');
    if (hash) *hash = '\0';
}

static int first_token_as_string(const char *line, char *out, size_t out_sz) {
    while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') line++;
    if (*line == '\0') return 0;
    size_t i = 0;
    while (*line && *line != ' ' && *line != '\t' && *line != '\r' && *line != '\n' && i + 1 < out_sz) {
        out[i++] = *line++;
    }
    out[i] = '\0';
    return i > 0;
}

static int first_token_as_int(const char *line, int *out_val) {
    char buf[64];
    if (!first_token_as_string(line, buf, sizeof(buf))) return 0;
    *out_val = atoi(buf);
    return 1;
}

// Loads the client configuration from the clientThreadConfig.cfg file
static void load_client_config(void) {
    FILE *f = fopen("clientThreadConfig.cfg", "r");
    char line[256];
    if (!f) {
        fprintf(stderr, "Warning: cannot open clientThreadConfig.cfg, defaulting.\n");
        return;
    }
    if (fgets(line, sizeof(line), f)) {
        strip_comment(line);
        first_token_as_string(line, tracker_ip, sizeof(tracker_ip)); // Gets tracker IP
    }
    if (fgets(line, sizeof(line), f)) {
        strip_comment(line);
        first_token_as_int(line, &tracker_port); // Gets tracker port
    }
    if (fgets(line, sizeof(line), f)) {
        strip_comment(line);
        int tmp;
        if (first_token_as_int(line, &tmp) && tmp > 0) // Gets refresh interval
            refresh_interval = tmp;
    }
    fclose(f);
}

// Loads the server configuration from the serverThreadConfig.cfg file
static void load_server_config(void) {
    FILE *f = fopen("serverThreadConfig.cfg", "r");
    char line[256];

    if (!f) {
        fprintf(stderr, "Warning: cannot open serverThreadConfig.cfg, defaulting.\n");
        return;
    }

    if (fgets(line, sizeof(line), f)) {
        strip_comment(line);
        first_token_as_int(line, &peer_listen_port); // Gets peer listen port
    }

    if (fgets(line, sizeof(line), f)) {
        strip_comment(line);
        first_token_as_string(line, shared_folder, sizeof(shared_folder)); // Gets shared folder
    }

    fclose(f);
}

static void trim_eol(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

SOCKET connect_to_tracker()
{
    SOCKET sock;
    struct sockaddr_in server;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_addr.s_addr = inet_addr(tracker_ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(tracker_port);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("Connection to tracker failed\n");
        return -1;
    }

    return sock;
}

void handle_LIST() {
    SOCKET sock = connect_to_tracker();
    char buffer[BUFFER_SIZE];

    sprintf(buffer, "<REQ LIST>\n");
    send(sock, buffer, strlen(buffer), 0);

    memset(buffer, 0, BUFFER_SIZE);
    recv(sock, buffer, BUFFER_SIZE, 0);

    printf("Tracker response:\n%s\n", buffer);

    CLOSESOCK(sock);
}

void handle_GET(char* filename) {
    SOCKET sock = connect_to_tracker();
    char buffer[BUFFER_SIZE];

    sprintf(buffer, "<GET %s.track>\n", filename);
    send(sock, buffer, strlen(buffer), 0);

    FILE* fp;
    char localfile[128];
    sprintf(localfile, "%s.track", filename);
    fp = fopen(localfile, "wb");

    while(1)
    {
        int bytes = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes <= 0) break;

        fwrite(buffer, 1, bytes, fp);

        if (strstr(buffer, "REP GET END")    != NULL) {
            break;
        }
    }

    fclose(fp);
    clsoesocket(sock);

    printf("Tracker file saved: %s\n", localfile);

    // TODO: DOWNLOAD CHUNKS
}

void send_updatetracker(char* filename, int start, int end) {
    SOCKET sock = connect_to_tracker();
    char buffer[BUFFER_SIZE];

    sprintf(buffer, "<updatetracker %s %d %d 127.0.0.1 %d>\n", 
        filename, start, end, peer_listen_port);

    send(sock, buffer, strlen(buffer), 0);

    recv(sock, buffer, BUFFER_SIZE, 0);
    printf("Update response: %s\n", buffer);

    CLOSESOCK(sock);
}

void send_createtracker(char* filename, int filesize, char* md5) {
    SOCKET sock = connect_to_tracker();
    char buffer[BUFFER_SIZE];

    sprintf(buffer, "<createtracker %s %d desc %s 127.0.0.1 %d>\n", 
        filename, filesize, md5, peer_listen_port);

    send(sock, buffer, strlen(buffer), 0);

    recv(sock, buffer, BUFFER_SIZE, 0);
    printf("Create response: %s\n", buffer);

    CLOSESOCK(sock);
}

DWORD WINAPI peer_server_thread(LPVOID arg) {
    SOCKET server_sock, client_sock;
    struct sockaddr_in server, client;
    int c = sizeof(struct sockaddr_in);
    char buffer[BUFFER_SIZE];

    server_sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(peer_listen_port);

    bind(server_sock, (struct sockaddr *)&server, sizeof(server));
    listen(server_sock, 5);

    printf("[peer-server] listening on port %d\n", peer_listen_port);

    while(1)
    {
        client_sock = accept(server_sock, (struct sockaddr *)&client, &c);
        if (client_sock == INVALID_SOCKET) {
            if (g_peer_server_stop)
                break;
            continue;
        }

        memset(buffer, 0, BUFFER_SIZE);
        recv(client_sock, buffer, BUFFER_SIZE, 0);

        printf("[peer-server] received from peer: %s\n", buffer);

        // VERY simplified parsing
        if (strstr(buffer, "GET") != NULL) {
            int start = 0, end = 0;
            sscanf(buffer, "GET %d %d", &start, &end);

            if ((end - start) > MAX_CHUNK) {
                send(client_sock, "<GET invalid>\n", 14, 0);
            } else {
                char data[MAX_CHUNK];
                memset(data, 'A', MAX_CHUNK); // fake data
                send(client_sock, data, (end - start), 0);
            }
        }

        CLOSESOCK(client_sock);
    }
}

DWORD WINAPI tracker_update_thread(LPVOID *arg) {
    while(1)
    {
        Sleep(refresh_interval * 1000);

        printf("[tracker-update] refreshing tracker with current peer info...\n");

        /* TODO: send an <updatetracker> request to the tracker with the current peer's info. */
    }

}

void cli_loop() {
    char line[256];

    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        trim_eol(line);
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        } else if (strcmp(line, "LIST") == 0) {
            handle_LIST();
        } else if (strcmp(line, "GET") == 0) {
            char filename[128];
            sscanf(line, "GET %127s", filename);
            handle_GET(filename);
        } else {
            printf("Unknown command. Available commands: LIST, GET <filename>, exit\n");
        }

    }
}

int main(int argc, char *argv[]) {
    char server_address[64];
    load_client_config(); // Laods info from clientThreadconfig.cfg
    load_server_config(); // Laods info from serverThreadconfig.cfg

    struct sockaddr_in server_addr;
    socket_t sockid;

    printf("Tracker: %s:%d, Peer listen: %d, Shared: %s, Interval: %d\n",
        tracker_ip, tracker_port, peer_listen_port, shared_folder, refresh_interval);

    /* Initialize Winsock once before using any socket API on Windows. */
    WSADATA wsa_data;
    int wsa_err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_err != 0) {
        fprintf(stderr, "WSAStartup failed with error %d\n", wsa_err);
        return EXIT_FAILURE;
    }

    HANDLE server_thread, update_thread;

    server_thread = (HANDLE)_beginthreadex(NULL, 0, peer_server_thread, NULL, 0, NULL);
    update_thread = (HANDLE)_beginthreadex(NULL, 0, tracker_update_thread, NULL, 0, NULL);

    cli_loop();

    WSACleanup();

    return EXIT_SUCCESS;
}
