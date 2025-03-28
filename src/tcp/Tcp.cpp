#include <exception>
#include <iostream>
#include <ostream>
#include <string>
#include <system_error>
#include <cstring>      // Added for strerror()
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../utils/constants.hpp"
#include "../utils/http_message.hpp"




class TCPServer {
    int server_fd;
    const int port;

    void close_socket(int fd) {
        if (fd >= 0) {
            close(fd);
        }
    }

    void throw_system_error(const std::string& msg) {
        throw std::system_error(errno, std::generic_category(), msg);
    }

public:
    TCPServer(int port) : server_fd(-1), port(port) {}

    ~TCPServer() {
        close_socket(server_fd);
    }

    void start() {
        // Create socket
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            throw_system_error("socket creation failed");
        }

        // Set SO_REUSEADDR
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ){
            throw_system_error("setsockopt failed");
        }

        // Bind
        struct sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            throw_system_error("bind failed");
        }

        // Listen
        if (listen(server_fd, SOMAXCONN) < 0) {
            throw_system_error("listen failed");
        }

        std::cout << "Server listening on port " << port << std::endl;
    }

    void run() {
        while (true) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            // Accept connection
            int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                std::cerr << "accept failed: " << strerror(errno) << std::endl;
                continue;
            }

            // Get client info
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            std::cout << "Connection from " << client_ip << ":" 
                      << ntohs(client_addr.sin_port) << std::endl;

            // Handle connection
            try {
                handle_connection(client_fd);
            } catch (const std::exception& e) {
                std::cerr << "Connection error: " << e.what() << std::endl;
            }

            close_socket(client_fd);
            std::cout << "Connection closed" << std::endl;
        }
    }

private:
    void handle_connection(int client_fd) {
        
        try {

            HttpMessage request = HttpMessage::parse(client_fd);

            std::vector<char> body = request.body;
            std::string headers = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: " + std::to_string(body.size()) + "\r\n"
                "Connection: close\r\n\r\n";
            
            // 3. Send all data safely
            if (!send_all(client_fd, headers.data(), headers.size()) ||
                !send_all(client_fd, body.data(), body.size())) {
                std::cerr << "Failed to send complete response\n";
            }

        } catch(const std::exception &e) {
            std::cerr << "Exception : " << e.what() << "\n" ;
        }
    }

    bool send_all(int socket, const char* data, size_t length) {
        size_t total_sent = 0;
        while (total_sent < length) {
            ssize_t sent = send(socket, data + total_sent, length - total_sent, 0);
            if (sent == -1) return false;  // Error
            if (sent == 0) return false;   // Connection closed
            total_sent += sent;
        }
        return true;
    }
};
