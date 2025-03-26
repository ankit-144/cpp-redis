#include "../include/Http.hpp"
#include <iostream>
#include <ostream>
#include <string>
#include <system_error>
#include <cstring>      // Added for strerror()
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>



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
        char buffer[1024];
        
        while (true) {
            // Receive data
            ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
            
            if (bytes_received < 0) {
                throw_system_error("recv failed");
            } else if (bytes_received == 0) {
                // Client disconnected
                break;
            }

            // Echo back
            std::cout.write(buffer, bytes_received) << std::endl;
            std::cout << "Message Received : " << std::string(buffer) << std::endl;
            std::cout << "Received " << bytes_received << " bytes" << std::endl;

            std::string http_reponse = Http::create(200 , "Hello back from server");
            
            ssize_t bytes_sent = send(client_fd, http_reponse.c_str(), http_reponse.size(), 0);
            if (bytes_sent < 0) {
                throw_system_error("send failed");
            }
        }
    }
};

int main() {
    try {
        TCPServer server(8080);
        server.start();
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}