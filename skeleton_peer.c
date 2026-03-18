#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Cross‑platform socket includes and typedefs.
 * - On Windows (MinGW / MSVC), use Winsock2 and call WSAStartup/WSACleanup.
 * - On Linux / POSIX, use BSD socket headers.
 */
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>

  /* Link with Winsock library when using MSVC.
   * With MinGW, the Makefile will add -lws2_32 instead. */
  #ifdef _MSC_VER
    #pragma comment(lib, "Ws2_32.lib")
  #endif

  typedef SOCKET socket_t;
  #define CLOSESOCK(s) closesocket(s)
#else
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>

  typedef int socket_t;
  #define CLOSESOCK(s) close(s)
#endif

/* NOTE:
 * The assignment skeleton assumes the existence of a LIST constant and a msg buffer to receive the server reply.
 * Here we declare minimal placeholders so the file compiles.
 * TODO: replace these with the real protocol structures that follow the project design.
 */
#ifndef LIST
#define LIST 1
#endif

static int msg = 0;

int main(int argc, char *argv[]) {
    char server_address[50];
    int server_port = 3490;  /* TODO: read this from a configuration file. */

    struct sockaddr_in server_addr;
    socket_t sockid;

#ifdef _WIN32
    /* Initialize Winsock once before using any socket API on Windows. */
    WSADATA wsa_data;
    int wsa_err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_err != 0) {
        fprintf(stderr, "WSAStartup failed with error %d\n", wsa_err);
        return EXIT_FAILURE;
    }
#endif

    /* Create a TCP socket (IPv4, stream). */
    sockid = socket(AF_INET, SOCK_STREAM, 0);
    if (sockid == (socket_t) -1
#ifdef _WIN32
        || sockid == INVALID_SOCKET
#endif
    ) {
        fprintf(stderr, "socket cannot be created\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return EXIT_FAILURE;
    }

    /* For now, assume server is on localhost.
     * TODO: Populate server_address from argv or a config file.
     */
    strcpy(server_address, "127.0.0.1");

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;              /* IPv4 */
    server_addr.sin_port   = htons(server_port);   /* host to network byte order */

    if (inet_pton(AF_INET, server_address, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server address: %s\n", server_address);
        CLOSESOCK(sockid);
#ifdef _WIN32
        WSACleanup();
#endif
        return EXIT_FAILURE;
    }

    /* Connect to the tracker server. */
    if (connect(sockid, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Cannot connect to server\n");
        CLOSESOCK(sockid);
#ifdef _WIN32
        WSACleanup();
#endif
        return EXIT_FAILURE;
    }

    /* If connected successfully, handle the LIST command as an example. */
    if (argc > 1 && !strcmp(argv[1], "list")) {
        /* In the assignment, LIST is part of the higher‑level protocol.
         * Here we simply send a small integer as a placeholder.
         */
        int list_req = htons(LIST);
        int bytes_sent = send(sockid, (const char *) &list_req, sizeof(list_req), 0);
        if (bytes_sent < 0) {
            fprintf(stderr, "Send_request failure\n");
            CLOSESOCK(sockid);
#ifdef _WIN32
            WSACleanup();
#endif
            return EXIT_FAILURE;
        }

        /* Read back a simple integer reply as a placeholder. */
        int bytes_recv = recv(sockid, (char *) &msg, sizeof(msg), 0);
        if (bytes_recv < 0) {
            fprintf(stderr, "Read failure\n");
            CLOSESOCK(sockid);
#ifdef _WIN32
            WSACleanup();
#endif
            return EXIT_FAILURE;
        }
    }

    CLOSESOCK(sockid);
    printf("Connection closed\n");

#ifdef _WIN32
    WSACleanup();
#endif

    return EXIT_SUCCESS;
}
