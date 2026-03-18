#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Cross‑platform socket includes and typedefs.
 * - On Linux / POSIX, we retain the original fork‑based, multi‑process skeleton.
 * - On Windows, this file currently only provides a stub main that tells you the
 *   tracker is not yet implemented; you can later replace it with a threaded
 *   implementation if desired.
 */
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>

  typedef SOCKET socket_t;
  #define CLOSESOCK(s) closesocket(s)
#else
  #include <unistd.h>
  #include <errno.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <dirent.h>

  typedef int socket_t;
  #define CLOSESOCK(s) close(s)
#endif

/* Placeholder values and buffers that the original skeleton assumed existed.
 */
#define MAXLINE 4096

static int server_port = 3490;   /* TODO: read from tracker config file. */
static char read_msg[MAXLINE];
static char fname[256];

/* Forward declaration of the per‑client handler for POSIX builds. */
static void peer_handler(int sock_child);


#ifdef _WIN32

/*
 * Windows stub:
 *  The original skeleton uses fork(), which is POSIX‑only and not available
 *  on Windows. To keep your project building under MinGW while you develop
 *  the peer side, we provide a minimal stub here. Later, you can implement
 *  a threaded tracker using CreateThread / _beginthreadex.
 */
int main(void) {
    fprintf(stderr,
            "Tracker skeleton is currently implemented only for POSIX (Linux).\n"
            "On Windows/MinGW, please implement a threaded version in "
            "skeleton_tracker.c or run the tracker under Linux/WSL.\n");
    return EXIT_FAILURE;
}

#else  /* POSIX implementation */

/*
 * POSIX tracker skeleton:
 *  - Creates a TCP listening socket.
 *  - Accepts incoming connections in a loop.
 *  - Forks a child process per client and dispatches to peer_handler().
 */
int main(void) {
    pid_t pid;
    struct sockaddr_in server_addr, client_addr;
    socket_t sockid, sock_child;
    socklen_t clilen = sizeof(client_addr);

    /* Create a TCP socket (IPv4, stream). */
    sockid = socket(AF_INET, SOCK_STREAM, 0);
    if (sockid < 0) {
        printf("socket cannot be created \n");
        exit(0);
    }

    /* Bind the socket to a local port to listen for incoming connections. */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;               /* IPv4 */
    server_addr.sin_port        = htons(server_port);    /* host to network byte order */
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);     /* listen on all interfaces */

    if (bind(sockid, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        printf("bind  failure\n");
        exit(0);
    }

    printf("Tracker SERVER READY TO LISTEN INCOMING REQUEST.... \n");

    /* Start listening; backlog size is arbitrary here (10). */
    if (listen(sockid, 10) < 0) {
        printf(" Tracker  SERVER CANNOT LISTEN\n");
        exit(0);
    }

    /* Accept connections from clients in an infinite loop. */
    while (1) {
        sock_child = accept(sockid, (struct sockaddr *) &client_addr, &clilen);
        if (sock_child < 0) {
            printf("Tracker Cannot accept...\n");
            exit(0);
        }

        /* Fork a child process to handle this particular client. */
        pid = fork();
        if (pid == 0) {
            /* Child process: no need for the listening socket. */
            CLOSESOCK(sockid);
            peer_handler(sock_child);      /* Serve this client.           */
            CLOSESOCK(sock_child);
            exit(0);                       /* Child done.                  */
        }

        /* Parent process: close the connected socket, child is handling it. */
        CLOSESOCK(sock_child);
    }

    /* Not reached in this skeleton. */
    CLOSESOCK(sockid);
    return 0;
}


/*
 * peer_handler:
 *  Child process function to handle a single connected peer.
 *  It:
 *   - Reads a single line/command from the client.
 *   - Dispatches to appropriate handler based on the protocol.
 */
static void peer_handler(int sock_child) {
    int length;

    /* Read a command from the peer. */
    length = (int) read(sock_child, read_msg, MAXLINE - 1);
    if (length <= 0) {
        return;
    }
    read_msg[length] = '\0';

    /* LIST command received. */
    if ((!strcmp(read_msg, "REQ LIST")) ||
        (!strcmp(read_msg, "req list")) ||
        (!strcmp(read_msg, "<REQ LIST>")) ||
        (!strcmp(read_msg, "<REQ LIST>\n"))) {
        /* TODO: implement handle_list_req(sock_child) according to the spec. */
        /* handle_list_req(sock_child); */
        printf("list request handled (skeleton).\n");
    }
    /* GET command received. */
    else if ((strstr(read_msg, "get") != NULL) ||
             (strstr(read_msg, "GET") != NULL)) {
        /* TODO: xtrct_fname(read_msg, " ");              */
        /* TODO: handle_get_req(sock_child, fname);       */
    }
    /* createtracker command received. */
    else if ((strstr(read_msg, "createtracker")   != NULL) ||
             (strstr(read_msg, "Createtracker")   != NULL) ||
             (strstr(read_msg, "CREATETRACKER")   != NULL)) {
        /* TODO: tokenize_createmsg(read_msg);            */
        /* TODO: handle_createtracker_req(sock_child);    */
    }
    /* updatetracker command received. */
    else if ((strstr(read_msg, "updatetracker")   != NULL) ||
             (strstr(read_msg, "Updatetracker")   != NULL) ||
             (strstr(read_msg, "UPDATETRACKER")   != NULL)) {
        /* TODO: tokenize_updatemsg(read_msg);            */
        /* TODO: handle_updatetracker_req(sock_child);    */
    }
}

#endif  /* !_WIN32 */