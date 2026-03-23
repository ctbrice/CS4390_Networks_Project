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

static int start_peer_server(void);
static void stop_peer_server(void);
static unsigned __stdcall peer_accept_loop(void *unused);
static unsigned __stdcall peer_client_thread(void *arg);

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

static int send_all(socket_t sock, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        int n = send(sock, buf + off, (int)(len - off), 0);
        if (n <= 0) return -1;
        off += (size_t)n;
    }
    return 0;
}

static int recv_line(socket_t sock, char *out, size_t cap) {
    size_t used = 0;
    while (used + 1 < cap) {
        char ch;
        int n = recv(sock, &ch, 1, 0);
        if (n <= 0) break;
        out[used++] = ch;
        if (ch == '\n') break;
    }
    out[used] = '\0';
    return (int)used;
}

static void trim_eol(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s list\n"
        "  %s get <filename.track>\n"
        "  %s createtracker <filename> <filesize> <description_no_spaces> <md5>\n"
        "  %s updatetracker <filename> <start_byte> <end_byte>\n",
        prog, prog, prog, prog);
}

static void get_local_ip_for_socket(socket_t sock, char *out, size_t out_sz) {
    struct sockaddr_in local_addr;
    int len = (int)sizeof(local_addr);
    if (getsockname(sock, (struct sockaddr *)&local_addr, &len) == 0) {
        if (inet_ntop(AF_INET, &local_addr.sin_addr, out, (DWORD)out_sz) != NULL) {
            return;
        }
    }
    strncpy(out, "127.0.0.1", out_sz - 1);
    out[out_sz - 1] = '\0';
}

static unsigned __stdcall peer_client_thread(void *arg) {
    socket_t client = *(socket_t *)arg;
    free(arg);

    struct sockaddr_in ca;
    int calen = (int)sizeof(ca);
    if (getpeername(client, (struct sockaddr *)&ca, &calen) == 0) {
        char peeraddr[64];
        if (inet_ntop(AF_INET, &ca.sin_addr, peeraddr, sizeof(peeraddr)) != NULL) {
            printf("[peer-server] connection from %s:%d\n", peeraddr, (int)ntohs(ca.sin_port));
        }
    }

    char line[4096];
    int n = recv_line(client, line, sizeof(line));
    if (n > 0) {
        trim_eol(line);
        printf("[peer-server] request: %s\n", line);
        /* Stub: real P2P chunk GET / 1024-byte limit comes later; keep connection protocol-friendly. */
        const char *stub = "<peer p2p stub: not implemented>\n";
        (void)send_all(client, stub, strlen(stub));
    }

    CLOSESOCK(client);
    return 0;
}

static unsigned __stdcall peer_accept_loop(void *unused) {
    (void)unused;
    printf("[peer-server] accepting on port %d\n", peer_listen_port);

    for (;;) {
        if (g_peer_server_stop)
            break;

        struct sockaddr_in caddr;
        int clen = sizeof(caddr);
        socket_t c = accept(g_listen_sock, (struct sockaddr *)&caddr, &clen);
        if (c == INVALID_SOCKET) {
            if (g_peer_server_stop)
                break;
            continue;
        }

        socket_t *p = (socket_t *)malloc(sizeof(socket_t));
        if (!p) {
            CLOSESOCK(c);
            continue;
        }
        *p = c;
        uintptr_t th = _beginthreadex(NULL, 0, peer_client_thread, p, 0, NULL);
        if (th == 0) {
            free(p);
            CLOSESOCK(c);
            fprintf(stderr, "[peer-server] _beginthreadex failed\n");
            continue;
        }
        CloseHandle((HANDLE)th);
    }

    return 0;
}

static int start_peer_server(void) {
    g_peer_server_stop = 0;
    g_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_sock == INVALID_SOCKET) {
        fprintf(stderr, "[peer-server] socket() failed\n");
        return -1;
    }

    int one = 1;
    if (setsockopt(g_listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one)) == SOCKET_ERROR) {
        fprintf(stderr, "[peer-server] setsockopt(SO_REUSEADDR) failed\n");
    }

    struct sockaddr_in la;
    memset(&la, 0, sizeof(la));
    la.sin_family = AF_INET;
    la.sin_addr.s_addr = htonl(INADDR_ANY);
    la.sin_port = htons((unsigned short)peer_listen_port);

    if (bind(g_listen_sock, (struct sockaddr *)&la, sizeof(la)) == SOCKET_ERROR) {
        fprintf(stderr, "[peer-server] bind(port %d) failed — another peer using this port?\n", peer_listen_port);
        CLOSESOCK(g_listen_sock);
        g_listen_sock = INVALID_SOCKET;
        return -1;
    }

    if (listen(g_listen_sock, SOMAXCONN) == SOCKET_ERROR) {
        fprintf(stderr, "[peer-server] listen() failed\n");
        CLOSESOCK(g_listen_sock);
        g_listen_sock = INVALID_SOCKET;
        return -1;
    }

    uintptr_t th = _beginthreadex(NULL, 0, peer_accept_loop, NULL, 0, NULL);
    if (th == 0) {
        fprintf(stderr, "[peer-server] _beginthreadex(accept) failed\n");
        CLOSESOCK(g_listen_sock);
        g_listen_sock = INVALID_SOCKET;
        return -1;
    }
    g_accept_thread = (HANDLE)th;
    return 0;
}

static void stop_peer_server(void) {
    InterlockedExchange(&g_peer_server_stop, 1);

    if (g_listen_sock != INVALID_SOCKET) {
        CLOSESOCK(g_listen_sock);
        g_listen_sock = INVALID_SOCKET;
    }

    if (g_accept_thread) {
        WaitForSingleObject(g_accept_thread, 15000);
        CloseHandle(g_accept_thread);
        g_accept_thread = NULL;
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

    if (start_peer_server() != 0) {
        fprintf(stderr, "Note: P2P listen server disabled; tracker-only mode for this run.\n");
    }

    /* Create a TCP socket (IPv4, stream). */
    sockid = socket(AF_INET, SOCK_STREAM, 0);
    if (sockid == INVALID_SOCKET) {
        fprintf(stderr, "socket cannot be created\n");
        stop_peer_server();
        WSACleanup();
        return EXIT_FAILURE;
    }

    /* Tracker address comes from clientThreadConfig.cfg */
    strncpy(server_address, tracker_ip, sizeof(server_address) - 1);
    server_address[sizeof(server_address) - 1] = '\0';

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;              /* IPv4 */
    server_addr.sin_port   = htons(tracker_port);  /* host to network byte order */

    if (inet_pton(AF_INET, server_address, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server address: %s\n", server_address);
        CLOSESOCK(sockid);
        stop_peer_server();
        WSACleanup();
        return EXIT_FAILURE;
    }

    /* Connect to the tracker server. */
    if (connect(sockid, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Cannot connect to server\n");
        CLOSESOCK(sockid);
        stop_peer_server();
        WSACleanup();
        return EXIT_FAILURE;
    }

    char local_ip[64];
    get_local_ip_for_socket(sockid, local_ip, sizeof(local_ip));
    

    /*
        Future thread implementation will happen here.
        start_thread(peer_server_thread)
        start_thread(tracker_update_thread)
    */

    /* UPDATE TRACKER*/
    char req[1024];
        snprintf(req, sizeof(req), "<updatetracker %s %s %s %s %d>\n",
                 argv[2], argv[3], argv[4], local_ip, peer_listen_port);
        if (send_all(sockid, req, strlen(req)) != 0) {
            fprintf(stderr, "Send updatetracker failure\n");
            CLOSESOCK(sockid);
            stop_peer_server();
            WSACleanup();
            return EXIT_FAILURE;
        }

        char line[4096];
        int n = recv_line(sockid, line, sizeof(line));
        if (n <= 0) {
            fprintf(stderr, "Read updatetracker reply failure\n");
            CLOSESOCK(sockid);
            stop_peer_server();
            WSACleanup();
            return EXIT_FAILURE;
        }
        trim_eol(line);
        printf("%s\n", line);

    /*
        The following are the manual commands
        TODO: Need to make them automatic.
    */

    if (argc > 1 && !strcmp(argv[1], "list")) {
        /* Send LIST request to the tracker. */
        const char *req = "<REQ LIST>\n";
        if (send_all(sockid, req, strlen(req)) != 0) {
            fprintf(stderr, "Send <REQ LIST> failure\n");
            CLOSESOCK(sockid);
            stop_peer_server();
            WSACleanup();
            return EXIT_FAILURE;
        }

        /* Read and print lines until "<REP LIST END>". */
        char line[4096];
        for (;;) {
            int n = recv_line(sockid, line, sizeof(line));
            if (n <= 0) {
                fprintf(stderr, "Read LIST reply failure\n");
                CLOSESOCK(sockid);
                stop_peer_server();
                WSACleanup();
                return EXIT_FAILURE;
            }
            trim_eol(line);
            printf("%s\n", line);
            if (!strcmp(line, "<REP LIST END>")) break;
        }
    } else if (argc > 2 && !strcmp(argv[1], "get")) {
        char req[512];
        snprintf(req, sizeof(req), "<GET %s >\n", argv[2]);
        if (send_all(sockid, req, strlen(req)) != 0) {
            fprintf(stderr, "Send GET failure\n");
            CLOSESOCK(sockid);
            stop_peer_server();
            WSACleanup();
            return EXIT_FAILURE;
        }

        /* Print GET response until REP GET END is received. */
        char line[4096];
        for (;;) {
            int n = recv_line(sockid, line, sizeof(line));
            if (n <= 0) {
                fprintf(stderr, "Read GET reply failure\n");
                CLOSESOCK(sockid);
                stop_peer_server();
                WSACleanup();
                return EXIT_FAILURE;
            }
            trim_eol(line);
            printf("%s\n", line);
            if (strstr(line, "<REP GET END ") == line) break;
        }
    } else if (argc > 5 && !strcmp(argv[1], "createtracker")) {
        /* Uses local peer_listen_port from serverThreadConfig.cfg as required by protocol. */
        char req[1024];
        snprintf(req, sizeof(req), "<createtracker %s %s %s %s %s %d>\n",
                 argv[2], argv[3], argv[4], argv[5], local_ip, peer_listen_port);
        if (send_all(sockid, req, strlen(req)) != 0) {
            fprintf(stderr, "Send createtracker failure\n");
            CLOSESOCK(sockid);
            stop_peer_server();
            WSACleanup();
            return EXIT_FAILURE;
        }

        char line[4096];
        int n = recv_line(sockid, line, sizeof(line));
        if (n <= 0) {
            fprintf(stderr, "Read createtracker reply failure\n");
            CLOSESOCK(sockid);
            stop_peer_server();
            WSACleanup();
            return EXIT_FAILURE;
        }
        trim_eol(line);
        printf("%s\n", line);
    } else if (argc > 4 && !strcmp(argv[1], "updatetracker")) {
        char req[1024];
        snprintf(req, sizeof(req), "<updatetracker %s %s %s %s %d>\n",
                 argv[2], argv[3], argv[4], local_ip, peer_listen_port);
        if (send_all(sockid, req, strlen(req)) != 0) {
            fprintf(stderr, "Send updatetracker failure\n");
            CLOSESOCK(sockid);
            stop_peer_server();
            WSACleanup();
            return EXIT_FAILURE;
        }

        char line[4096];
        int n = recv_line(sockid, line, sizeof(line));
        if (n <= 0) {
            fprintf(stderr, "Read updatetracker reply failure\n");
            CLOSESOCK(sockid);
            stop_peer_server();
            WSACleanup();
            return EXIT_FAILURE;
        }
        trim_eol(line);
        printf("%s\n", line);
    } else {
        print_usage(argv[0]);
    }

    CLOSESOCK(sockid);
    printf("Connection closed\n");

    stop_peer_server();
    WSACleanup();

    return EXIT_SUCCESS;
}
