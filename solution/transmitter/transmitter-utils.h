#ifndef TRANSMITTER_UTIS_H
#define TRANSMITTER_UTIS_H

#include <liburing.h>
#include <arpa/inet.h>

#define fallthrough __attribute__((fallthrough))

#ifdef DEBUG

static FILE *dptr = NULL;

static inline void setup_debug(const char *filename){
    dptr = fopen(filename, "w");
}

static inline void end_debug(void) {
    if (dptr)
        fclose(dptr);
    dptr = NULL;
}

#define print_debug(fmt, ...)               \
    if (dptr) {                            \
        fprintf(dptr, fmt, ##__VA_ARGS__); \
        fflush(dptr);                      \
    }

#define print_debug_func_finished   print_debug("%s finished.\n", __func__)
#define print_debug_func_entry      print_debug("%s()\n", __func__)

#else

static inline void setup_debug(const char *filename) {return;}
static inline void end_debug(void) {return;}
#define print_debug(fmt, ...)
#define print_debug_func_finished   
#define print_debug_func_entry      

#endif /* DEBUG */

#define EXIT_ERROR(fmt, ...)                                    \
    do {                                                        \
        printf("Error: " fmt "exiting ...\n", ##__VA_ARGS__); \
        exit(EXIT_FAILURE);                                     \
    } while (0)

#define EXIT_ERROR_ERRNO(fmt, ...)                              \
    do {                                                        \
        print_debug("Error: " fmt "Errno: %s\nexiting ...\n", ##__VA_ARGS__, strerror(errno)); \
        printf("Error: " fmt "Errno: %s\nexiting ...\n", ##__VA_ARGS__, strerror(errno));   \
        exit(EXIT_FAILURE);                                     \
    } while (0)


#define cqe_priv(cqe) io_uring_cqe_get_data(cqe)


typedef enum {
    SOCK_OPEN   = 0,
    CONNECT     = 1,
    SEND        = 2,
    SOCK_CLOSE  = 3
} request_type;

static inline
void print_failure(request_type f, struct sockaddr_in who, int errorcode) {
    char ip[INET_ADDRSTRLEN];
    const char *what;

    if (errorcode < 0)
        errorcode *= (-1);

    switch (f)
    {
        case SOCK_OPEN:  what = "socket";    break;
        case CONNECT:    what = "connect";   break;
        case SEND:       what = "send";      break;
        case SOCK_CLOSE: what = "close";     break;
    }

    inet_ntop(AF_INET, &(who.sin_addr), ip, INET_ADDRSTRLEN);
    fprintf(stderr, "%s:%hu - %s error: %s\n",
                    ip, ntohs(who.sin_port), what, strerror(errorcode));
    print_debug("stderr: %s:%hu - %s error: %s\n",
                    ip, ntohs(who.sin_port), what, strerror(errorcode));
}


// typedef void (*cqe_handler_func)(struct io_uring_cqe *cqe);  

// static inline
// void for_each_cqe(struct io_uring *ring, cqe_handler_func *handle){
//     int itt = 0;
//     unsigned long head;
//     struct io_uring_cqe *cqe;

//     io_uring_for_each_cqe(ring, head, cqe) {
//         handle(cqe);
//         itt++;
//     }
//     io_uring_cq_advance(ring, itt);
// }

#define for_each_cqe(ring, body)                                            \
    do {                                                                    \
        int __itt = 0; unsigned int __head = 0; struct io_uring_cqe *cqe;   \
        io_uring_for_each_cqe(ring, __head, cqe) {                          \
            { body }                                                        \
            __itt++;                                                        \
        }                                                                   \
        io_uring_cq_advance(ring, __itt);                                     \
    } while (0) 

#endif /* TRANSMITTER_UTIS_H */

