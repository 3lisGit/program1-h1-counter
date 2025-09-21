/*
 * EECE 446 Program 1 - HTML Tag and Byte Counter
 * Fall 2025
 * 
 * Authors: Alexander Liu and Elijah Coleman
 * Class: EECE 446
 * Semester: Fall 2025
 * 
 * Based on starter code provided by Prof. Kredo
 */

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <memory>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

using namespace std;

const string SERVER_HOST = "www.ecst.csuchico.edu";
const string SERVER_PORT = "80";
const string HTTP_REQUEST = "GET /~kkredo/file.html HTTP/1.0\r\n\r\n";
const int MAX_CHUNK_SIZE = 1000;

class H1Counter {
private:
    int chunk_size;
    int sockfd;
    unique_ptr<char[]> buffer;
    int total_bytes;
    int total_h1_tags;

public:
    H1Counter(int chunk_sz);
    ~H1Counter();
    
    bool initialize();
    bool connect_to_server();
    bool send_http_request();
    int process_data();
    void print_results() const;
    
private:
    int send_all(const string& data);
    int recv_chunk(char* buf, int chunk_sz);
    int count_h1_tags(const char* buf, int len);
    void print_usage(const string& program_name);
};

H1Counter::H1Counter(int chunk_sz) 
    : chunk_size(chunk_sz), sockfd(-1), total_bytes(0), total_h1_tags(0) {
    buffer = make_unique<char[]>(chunk_size + 1); // +1 for null terminator
}

H1Counter::~H1Counter() {
    if (sockfd >= 0) {
        close(sockfd);
    }
}

bool H1Counter::initialize() {
    if (chunk_size <= 0 || chunk_size > MAX_CHUNK_SIZE) {
        cerr << "Error: Chunk size must be between 1 and " << MAX_CHUNK_SIZE << endl;
        return false;
    }
    
    if (!buffer) {
        cerr << "Error: Failed to allocate buffer" << endl;
        return false;
    }
    
    return true;
}

bool H1Counter::connect_to_server() {
    struct addrinfo hints, *res, *p;
    int status;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    
    // Get address info for the server
    status = getaddrinfo(SERVER_HOST.c_str(), SERVER_PORT.c_str(), &hints, &res);
    if (status != 0) {
        cerr << "getaddrinfo error: " << gai_strerror(status) << endl;
        return false;
    }
    
    // Try to connect to one of the addresses
    for (p = res; p != nullptr; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) {
            continue;
        }
        
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            close(sockfd);
            sockfd = -1;
            continue;
        }
        
        break; // Successfully connected
    }
    
    freeaddrinfo(res);
    
    if (p == nullptr) {
        cerr << "Failed to connect to server" << endl;
        return false;
    }
    
    return true;
}

bool H1Counter::send_http_request() {
    return send_all(HTTP_REQUEST) == 0;
}

int H1Counter::send_all(const string& data) {
    size_t total_sent = 0;
    int bytes_sent;
    size_t len = data.length();
    
    while (total_sent < len) {
        bytes_sent = send(sockfd, data.c_str() + total_sent, len - total_sent, 0);
        if (bytes_sent < 0) {
            perror("send");
            return -1;
        }
        total_sent += bytes_sent;
    }
    
    return 0;
}

int H1Counter::process_data() {
    int bytes_received;
    
    while ((bytes_received = recv_chunk(buffer.get(), chunk_size)) > 0) {
        // Count h1 tags in this chunk
        int h1_count = count_h1_tags(buffer.get(), bytes_received);
        total_h1_tags += h1_count;
        
        // Update total byte count
        total_bytes += bytes_received;
        
        // TODO: Add any debugging output if needed
    }
    
    if (bytes_received < 0) {
        cerr << "Error receiving data" << endl;
        return -1;
    }
    
    return 0;
}

int H1Counter::recv_chunk(char* buf, int chunk_sz) {
    int bytes_received;
    
    // TODO: Implement chunk receiving logic
    // This should handle partial receives and fill the buffer
    // up to chunk_sz bytes or until no more data is available
    
    bytes_received = recv(sockfd, buf, chunk_sz, 0);
    if (bytes_received < 0) {
        perror("recv");
        return -1;
    }
    
    return bytes_received;
}

int H1Counter::count_h1_tags(const char* buf, int len) {
    int count = 0;
    
    // TODO: Implement tag counting using library function
    // Remember: must use library function, not manual loop
    // Hint: Use string functions or C++ string methods
    
    // Create a string from the buffer (handles null termination)
    string data(buf, len);
    
    // Search for <h1> tags using string::find
    size_t pos = 0;
    while ((pos = data.find("<h1>", pos)) != string::npos) {
        count++;
        pos += 4; // Move past this occurrence
    }
    
    return count;
}

void H1Counter::print_results() const {
    cout << "Number of <h1> tags: " << total_h1_tags << endl;
    cout << "Number of bytes: " << total_bytes << endl;
}

void print_usage(const string& program_name) {
    cerr << "Usage: " << program_name << " <chunk_size>" << endl;
    cerr << "  chunk_size: Size in bytes for data chunks (1-" << MAX_CHUNK_SIZE << ")" << endl;
}

int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Parse chunk size
    int chunk_size = atoi(argv[1]);
    
    // Create H1Counter object
    H1Counter counter(chunk_size);
    
    // Initialize and validate
    if (!counter.initialize()) {
        return 1;
    }
    
    // Connect to server
    if (!counter.connect_to_server()) {
        return 1;
    }
    
    // Send HTTP request
    if (!counter.send_http_request()) {
        cerr << "Error sending HTTP request" << endl;
        return 1;
    }
    
    // Process incoming data
    if (counter.process_data() < 0) {
        return 1;
    }
    
    // Print results
    counter.print_results();
    
    return 0;
}
