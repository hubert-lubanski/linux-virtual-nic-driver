#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <assert.h>

#include <liburing.h>
#include <fcntl.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>
#include "transmitter-utils.h"


static struct sockaddr_in address_from_entry(const char *ip_and_port) {
    struct sockaddr_in result;
    char ip[INET_ADDRSTRLEN];

    const char *ptr = ip_and_port;

    while (*(ptr++) != ':');
    const int len = ptr - ip_and_port - 1;

    memcpy(ip, ip_and_port, len);
    ip[len] = 0;


    result.sin_family = AF_INET;
    result.sin_port = htons(strtoul(ptr, NULL, 10));
    if (errno) {
        return result;
    }

    if (inet_pton(AF_INET, ip, &(result.sin_addr)) != 1) {
        errno = EINVAL;
    }

    return result;   
}



typedef enum {
    NO_SOCKET,
    OPERATIONAL,
    DISCONNECTED,
} client_state;

struct client_info {
    client_state    state;
    int             socket;
    struct sockaddr_in addr;
    size_t          bytes_send;
};

struct request_info {
    struct client_info *cinfo;
    request_type req_type;
};

/************************** Request creation helpers **************************/
static inline
struct io_uring_sqe *create_connect_request(
    struct io_uring *ring,
    struct client_info *cinfo
){
    // printf("Creating connect request\n");
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_connect(
        sqe,
        cinfo->socket,
        (struct sockaddr *)&cinfo->addr, sizeof(struct sockaddr_in)
    );
    io_uring_sqe_set_data(sqe, cinfo);
    

    return sqe;
}

static inline
struct io_uring_sqe *create_socket_request(
    struct io_uring *ring,
    struct client_info *cinfo
){
    // printf("Creating socket request\n");
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_socket(sqe, AF_INET, SOCK_STREAM, 0, 0);
    io_uring_sqe_set_data(sqe, cinfo);
    return sqe;
}

static inline
struct io_uring_sqe *create_send_request(
    struct io_uring *ring,
    struct client_info *cinfo,
    void *msg,
    size_t msglen
){
    // printf("Creating send request\n");
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    // NOTE Maybe some send flags? MSG_DONTWAIT? 
    io_uring_prep_send(sqe, cinfo->socket, msg, msglen, MSG_NOSIGNAL);
    io_uring_sqe_set_data(sqe, cinfo);
    return sqe;
}

static inline
struct io_uring_sqe *create_close_request(
    struct io_uring *ring,
    struct client_info *cinfo
){
    // printf("Creating close request\n");
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_close(sqe, cinfo->socket);
    io_uring_sqe_set_data(sqe, cinfo);
    return sqe;
}

/****************************** IO_uring handlers *****************************/

static inline
void resocket_clients(
    struct io_uring *ring,
    int no_clients,
    struct client_info *clients
){
    print_debug_func_entry;

    int requests = 0;
    for (int i = 0; i < no_clients; ++i) {
        if (clients[i].state == NO_SOCKET) {
            print_debug("Client %d has no socket.\n", i);
            create_socket_request(ring, clients + i);
            requests++;
        }
    }

    print_debug("%s waiting for %d submission to return.\n", __func__, requests);
    io_uring_submit_and_wait(ring, requests);

    print_debug("%s handling %d re-sockets.\n", __func__, requests);
    for_each_cqe(ring,
        struct client_info *c = cqe_priv(cqe);
        c->socket = cqe->res;   // Save socket 

        if (cqe->res < 0) 
            print_failure(SOCK_OPEN, c->addr, cqe->res);
        else {
            c->state = DISCONNECTED;
            print_debug("Opening of socket %d was successful!\n", c->socket);
        }
    );

    print_debug_func_finished;
}

static inline
int connect_and_send_clients(
    struct io_uring *ring,
    int no_clients,
    struct client_info *clients,
    char *line, ssize_t nread
){
    print_debug_func_entry;

    int requests = 0;
    for (int i = 0; i < no_clients; ++i){
        switch (clients[i].state)
        {
            case DISCONNECTED:
            {
                print_debug("Issuing connect on socket %d.\n", clients[i].socket);
                struct io_uring_sqe *sqe =create_connect_request(ring, clients + i);
                sqe->flags |= IOSQE_IO_LINK;
                requests++;
            } fallthrough;
            case OPERATIONAL:
            {   
                print_debug("Issuing send on socket %d.\n", clients[i].socket);
                create_send_request(ring, clients + i, line, nread);
                requests++;
            } break;
            default:
                /* We don't handle NO_SOCKET clients now.*/
                break;
        }
    }

    print_debug_func_finished;
    return requests;
}


static inline
int handle_client_operational(
    struct io_uring *ring, struct client_info *c, int res,
    char *line, ssize_t nread
){
    switch (-res)
    {
        case EBADF:
        case ENOTSOCK:
        case EPIPE:
        case EDESTADDRREQ:
            print_debug("Socket %d is invalid.\n", c->socket);
            close(c->socket);
            c->state = NO_SOCKET;
            break;
            
        case ECONNRESET:
        case ENOTCONN:
            print_debug("Socket %d has disconnected.\n", c->socket);
            c->state = DISCONNECTED;
            break;
        
        default:
            break;
    }

    if (res < 0) {
        print_debug("Sending on socket %d FAILED!\n", c->socket);
        print_failure(SEND, c->addr, res);
    }
    else {
        print_debug("Sending of %d bytes on socket %d was successful!\n", res, c->socket);
        c->bytes_send += res;

        if (c->bytes_send < nread) {
            print_debug("Issuing additional send on socket %d of %zu bytes\n", c->socket, nread - c->bytes_send);
            create_send_request(ring, c, line + c->bytes_send, nread - c->bytes_send);
            return 1;
        }
    }
    return 0;
}


static inline
void handle_results(
    struct io_uring *ring, int requests, char * line, ssize_t nread
){

    print_debug("%s waiting for %d submission to return.\n", __func__, requests);
    io_uring_submit_and_wait(ring, requests);

    int resends = 0;

    for_each_cqe(ring, 
        struct client_info *c = cqe_priv(cqe);

        print_debug("socket %d returns with %s (%d)\n",
                    c->socket, strerror((cqe->res < 0)*(-cqe->res)), cqe->res);

        switch (c->state) {
            case DISCONNECTED: 
            {
                if (cqe->res == -ECANCELED)
                    break; 

                if (cqe->res < 0) {
                    print_debug("socket %d connection FAILED.\n", c->socket);
                    print_failure(CONNECT, c->addr, cqe->res);
                }
                else {
                    print_debug("socket %d connected.\n", c->socket);
                    c->state = OPERATIONAL;
                }
            } break;

            case OPERATIONAL:
            {
                resends += handle_client_operational(ring, c, cqe->res, line, nread);
            } break;
            default:
                /* Do nothing */
                break;
        }
    );

    while (resends > 0) {

        print_debug("%s waiting for %d re-submission to return.\n", __func__, requests);
        io_uring_submit_and_wait(ring, resends);
        resends = 0;

        for_each_cqe(ring, 
            struct client_info *c = cqe_priv(cqe);
            /* Here are only OPERATIONAL clients. */

            resends += handle_client_operational(ring, c, cqe->res, line, nread);
        );
    }


}



int main(int argc, char **argv){

    setup_debug("debug.out");

    if (argc <= 1) 
        EXIT_ERROR("%s ip:port [ip:port] ...\n", argv[0]);
    
    /* getline variables */
    char *line = NULL;
    size_t len;
    ssize_t nread;

    struct io_uring ring;
    
    const int no_clients = argc - 1;
    struct client_info clients[no_clients];

    /* Parsing ip and port for each client */
    for (int i = 1; i < argc; ++i){
        clients[i-1].state = NO_SOCKET;
        clients[i-1].addr = address_from_entry(argv[i]);
        if (errno)
            EXIT_ERROR_ERRNO("Argument does not describe ip:port (%s)\n", argv[i]);
    }
    /* Initialize io_uring ring */
    if (io_uring_queue_init(no_clients * 2,  &ring, 0))
        EXIT_ERROR_ERRNO("Initialization of io_uring queue failed.\n");
    

    /* Main loop
     * 0. read line from stdin
     * 1. resocket all bad sockets
     * 2. prepare connect and send (using linking) requests
     * 3. submit requests and wait for completion of all
     * 4. handle completion results with re-sending if needed (partial sends)
     * */
    print_debug("Main loop.\n");
    size_t lnum = 0;
    while ((nread = getline(&line, &len, stdin)) != -1) {
        lnum++;
        print_debug("Got line (%ld)\n", nread);

        for (int i = 0; i < no_clients; ++i)
            clients[i].bytes_send = 0;

        /* Re-socket NO_SOCKET clients. This handles completion. */
        resocket_clients(&ring, no_clients, clients);

        /* Connect and send */
        int reqs =
            connect_and_send_clients(&ring, no_clients, clients, line, nread);

        handle_results(&ring, reqs, line, nread);

        print_debug("Transmitter line count: %zu\n", lnum);
    }
    /* Getline sets errno if error occured */
    if (errno)
        EXIT_ERROR_ERRNO("getline failed.\n");

    /* Close sockets */
    print_debug("Closing phase.\n");
    for (int i = 0; i < no_clients; ++i){
        if (clients[i].state != NO_SOCKET)
            (void)create_close_request(&ring, clients + i);
    }
    io_uring_submit_and_wait(&ring, no_clients);

    for_each_cqe(&ring, 
        struct client_info *c = cqe_priv(cqe);

        if (cqe->res < 0)
            print_failure(SOCK_CLOSE, c->addr, -(cqe->res));
        else {
            print_debug("Closing of socket %d was successful!\n", c->socket);
        }
    );

    io_uring_queue_exit(&ring);
    free(line);

    end_debug();
    return 0;
}