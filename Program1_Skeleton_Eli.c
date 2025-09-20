/*
EECE 446 - Program 1 (h1-counter)
Skeleton + step-by-step implementation notes + GitHub quickstart

*** IMPORTANT: Replace the placeholder names below with your group member names, class and semester in the comment block before submission. ***

Names: <Your Name(s) Here>
Class: EECE 446 Fall 2025

Overview / Strategy (step-by-step)
1) Parse the command-line arguments:
   - Mandatory: chunk size (integer > 4 and <= 1000)
   - Optional: a debug flag like "-d" (must be optional so program runs with only the size)
2) Resolve the host using getaddrinfo with AF_UNSPEC and SOCK_STREAM.
3) Create a socket and connect to the first address that works.
4) Send the HTTP request using a reliable send_all() helper that handles partial sends.
5) Repeatedly read "chunks" of exactly chunk_size bytes using recv_fill().
   - recv() can give fewer bytes than requested, so loop until you either fill the chunk or the server closes the connection.
   - Do NOT concatenate data across chunks. If the tag spans chunk boundary, it must NOT be counted.
6) For each chunk:
   - Null-terminate one byte after the received bytes (buf[bytes_received] = '\0') — this is allowed and sets exactly one byte.
   - Use strstr() (library function) to find occurrences of the exact substring "<h1>" in that chunk.
   - Update total tag count and total bytes.
7) When recv returns 0 (peer closed) and you processed remaining bytes, exit loop and print totals.

Testing note (on ecc-linux):
LD_PRELOAD=/user/home/kkredo/public_bin/libtesting_lib.so ./h1-counter <chunk-size>
This will simulate partial sends/receives so your code must handle them.

Build / compile:
  gcc -Wall -o h1-counter h1-counter.c
The class-provided Makefile should build the same executable name; follow the Makefile when submitting.

.gitignore (recommended):
  # build artifacts
  h1-counter
  *.o
  *.out
  *.exe

GitHub quickstart (CLI):
1) Create a repo on github.com (e.g. "program1-h1-counter").
2) Locally:
   git init
   git add h1-counter.c Makefile README.md
   git commit -m "Initial skeleton for Program 1"
   git branch -M main
   git remote add origin https://github.com/<yourusername>/<repo>.git
   git push -u origin main
3) To invite collaborators: Repo -> Settings -> Manage access -> Invite collaborators
4) Branch/PR workflow: use feature branches and open PRs for teammates.

-------------------------------
Below is a practical skeleton that implements the necessary helpers and main control flow.
It intentionally leaves "polish" (extra logging, extra debug prints) to you so you can learn by filling
in or modifying small pieces.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define REQUEST "GET /~kkredo/file.html HTTP/1.0\r\n\r\n"

/* send_all: ensure all bytes in buffer are sent (handles partial sends) */
static int send_all(int sockfd, const char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(sockfd, buf + total, len - total, 0);
        if (n == -1) {
            if (errno == EINTR) continue; /* retry on signal */
            perror("send");
            return -1;
        }
        total += (size_t)n;
    }
    return 0; /* success */
}

/* recv_fill: attempt to read exactly "want" bytes into buf. It loops until
 * either we have read want bytes, or recv() returns 0 (peer closed), or an error.
 * Returns number of bytes actually read (>=0), or -1 on error.
 */
static ssize_t recv_fill(int sockfd, char *buf, size_t want) {
    size_t total = 0;
    while (total < want) {
        ssize_t n = recv(sockfd, buf + total, want - total, 0);
        if (n == 0) {
            /* peer closed connection */
            break;
        } else if (n == -1) {
            if (errno == EINTR) continue; /* retry */
            perror("recv");
            return -1;
        }
        total += (size_t)n;
        /* Note: loop continues until total == want or peer closes */
    }
    return (ssize_t)total;
}

/* count_h1_in_chunk: expects buf to be a writable buffer with a null terminator at buf[len]
 * (i.e. buf[len] == '\0'). It searches for exact substring "<h1>" and returns occurrences.
 * Uses strstr (library function) as required by the assignment.
 */
static size_t count_h1_in_chunk(char *buf, ssize_t len) {
    if (len <= 0) return 0;
    /* ensure null-terminated: caller must provide space for buf[len] = '\0' */
    buf[len] = '\0'; /* this writes exactly one byte */
    size_t count = 0;
    char *p = buf;
    while ((p = strstr(p, "<h1>")) != NULL) {
        count++;
        p += 4; /* move past the found tag to continue searching */
    }
    return count;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <chunk_size> [ -d ]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int debug = 0;
    if (argc >= 3 && strcmp(argv[2], "-d") == 0) debug = 1;

    char *endptr = NULL;
    long chunk_long = strtol(argv[1], &endptr, 10);
    if (endptr == argv[1] || chunk_long <= 4 || chunk_long > 1000) {
        fprintf(stderr, "Error: chunk_size must be an integer > 4 and <= 1000\n");
        return EXIT_FAILURE;
    }
    size_t chunk_size = (size_t)chunk_long;

    if (debug) fprintf(stderr, "chunk_size=%zu\n", chunk_size);

    /* Resolve server */
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;    /* AF_UNSPEC per assignment */
    hints.ai_socktype = SOCK_STREAM;

    int gai = getaddrinfo("www.ecst.csuchico.edu", "80", &hints, &res);
    if (gai != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai));
        return EXIT_FAILURE;
    }

    int sockfd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) continue; /* try next */
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) break; /* success */
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res);

    if (sockfd == -1) {
        fprintf(stderr, "Could not connect to server\n");
        return EXIT_FAILURE;
    }

    /* Send HTTP GET request reliably */
    if (send_all(sockfd, REQUEST, strlen(REQUEST)) == -1) {
        close(sockfd);
        return EXIT_FAILURE;
    }

    /* Allocate buffer with space for one extra byte for a terminator */
    char *buf = malloc(chunk_size + 1);
    if (!buf) {
        perror("malloc");
        close(sockfd);
        return EXIT_FAILURE;
    }

    size_t total_bytes = 0;
    size_t total_tags = 0;

    while (1) {
        ssize_t got = recv_fill(sockfd, buf, chunk_size);
        if (got == -1) { /* error */
            free(buf);
            close(sockfd);
            return EXIT_FAILURE;
        }
        if (got == 0) {
            /* server closed and no more data */
            break;
        }

        /* Update counters and process this chunk independently */
        total_bytes += (size_t)got;
        total_tags += count_h1_in_chunk(buf, got);

        if (debug) fprintf(stderr, "chunk read=%zd tags so far=%zu bytes so far=%zu\n", got, total_tags, total_bytes);

        /* If we read fewer than chunk_size, the server likely closed; exit loop. */
        if ((size_t)got < chunk_size) break;
    }

    free(buf);
    close(sockfd);

    /* Print results (match assignment style) */
    printf("Number of <h1> tags: %zu\n", total_tags);
    printf("Number of bytes: %zu\n", total_bytes);

    return EXIT_SUCCESS;
}
