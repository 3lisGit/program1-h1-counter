/*
 * EECE 446 Program 1 - HTML Tag and Byte Counter
 * Fall 2025
 * Authors: Alexander Liu, Elijah Coleman
 * 
 * This program connects to a web server, requests an HTML file,
 * and counts <h1> tags and total bytes received.
 * Started with starter code provided by Prof. Kredo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define SERVER_HOST "www.ecst.csuchico.edu"
#define SERVER_PORT "80"
#define HTTP_REQUEST "GET /~kkredo/file.html HTTP/1.0\r\n\r\n"

/* Function to handle partial send operations */
int send_all(int socket, const char *buffer, int length) {
    int total_sent = 0;
    int bytes_left = length;
    int bytes_sent;
    
    while (total_sent < length) {
        bytes_sent = send(socket, buffer + total_sent, bytes_left, 0);
        if (bytes_sent == -1) {
            perror("send");
            return -1;
        }
        total_sent += bytes_sent;
        bytes_left -= bytes_sent;
    }
    
    return total_sent;
}

/* Function to handle partial recv operations */
int recv_chunk(int socket, char *buffer, int chunk_size) {
    int total_received = 0;
    int bytes_received;
    
    while (total_received < chunk_size) {
        bytes_received = recv(socket, buffer + total_received, 
                            chunk_size - total_received, 0);
        if (bytes_received == -1) {
            perror("recv");
            return -1;
        }
        if (bytes_received == 0) {
            /* Connection closed by server */
            break;
        }
        total_received += bytes_received;
    }
    
    return total_received;
}

/* Function to count <h1> tags in a chunk using library function */
int count_h1_tags(const char *buffer, int length) {
    int count = 0;
    const char *search_tag = "<h1>";
    const char *ptr = buffer;
    char temp_buffer[length + 1];
    
    /* Create null-terminated string for strstr */
    memcpy(temp_buffer, buffer, length);
    temp_buffer[length] = '\0';
    
    /* Use strstr library function to find tags */
    ptr = temp_buffer;
    while ((ptr = strstr(ptr, search_tag)) != NULL) {
        count++;
        ptr += 4; /* Move past the found tag */
    }
    
    return count;
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char *chunk_buffer;
    int chunk_size;
    int total_h1_tags = 0;
    int total_bytes = 0;
    int bytes_received;
    
    /* Check command line arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <chunk_size>\n", argv[0]);
        return 1;
    }
    
    chunk_size = atoi(argv[1]);
    if (chunk_size <= 0 || chunk_size > 1000) {
        fprintf(stderr, "Error: chunk size must be between 1 and 1000\n");
        return 1;
    }
    
    /* Allocate buffer for chunks */
    chunk_buffer = malloc(chunk_size);
    if (chunk_buffer == NULL) {
        perror("malloc");
        return 1;
    }
    
    /* Set up address info hints */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  /* Use AF_UNSPEC as required */
    hints.ai_socktype = SOCK_STREAM;
    
    /* Get address info for the server */
    if ((rv = getaddrinfo(SERVER_HOST, SERVER_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        free(chunk_buffer);
        return 1;
    }
    
    /* Loop through results and connect to the first one we can */
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
        
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("connect");
            continue;
        }
        
        break;
    }
    
    if (p == NULL) {
        fprintf(stderr, "Failed to connect to server\n");
        freeaddrinfo(servinfo);
        free(chunk_buffer);
        return 1;
    }
    
    freeaddrinfo(servinfo);
    
    /* Send HTTP request */
    if (send_all(sockfd, HTTP_REQUEST, strlen(HTTP_REQUEST)) == -1) {
        fprintf(stderr, "Failed to send HTTP request\n");
        close(sockfd);
        free(chunk_buffer);
        return 1;
    }
    
    /* Receive and process data in chunks */
    while ((bytes_received = recv_chunk(sockfd, chunk_buffer, chunk_size)) > 0) {
        /* Count h1 tags in this chunk */
        total_h1_tags += count_h1_tags(chunk_buffer, bytes_received);
        
        /* Count total bytes */
        total_bytes += bytes_received;
        
        /* If we received less than chunk_size, we're done */
        if (bytes_received < chunk_size) {
            break;
        }
    }
    
    if (bytes_received == -1) {
        fprintf(stderr, "Error receiving data\n");
        close(sockfd);
        free(chunk_buffer);
        return 1;
    }
    
    /* Close socket and free memory */
    close(sockfd);
    free(chunk_buffer);
    
    /* Print results */
    printf("Number of <h1> tags: %d\n", total_h1_tags);
    printf("Number of bytes: %d\n", total_bytes);
    
    return 0;
}
