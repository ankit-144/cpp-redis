// base_tcp_server.hpp (Example Header Name)
#ifndef BASE_TCP_SERVER_HPP
#define BASE_TCP_SERVER_HPP

#include <exception>
#include <iostream>
#include <ostream>
#include <string>
#include <system_error>
#include <cstring>      
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <mutex>        
#include <stdexcept>    


#include <thread>

#include "../utils/http_message.hpp" 
#include "../debug/debug.hpp"       

class TCPServer {
protected: 
    int server_fd;
    const int port;
    std::mutex io_mutex; // Mutex for thread-safe console output

    // Protected helper methods
    virtual void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(io_mutex);
        std::cout << "[TCPBase] " << message << std::endl;
    }

    virtual void log_error(const std::string& message) {
        std::lock_guard<std::mutex> lock(io_mutex);
        std::cerr << "[TCPBase ERROR] " << message << std::endl;
    }

    virtual void close_socket(int fd) {
        if (fd >= 0) {
            DEBUG("Closing socket FD:", fd);
            shutdown(fd, SHUT_RDWR); // Attempt graceful shutdown first
            close(fd);
        }
    }

    void throw_system_error(const std::string& msg) {
        // Using strerror(errno) for a more informative message
        throw std::system_error(errno, std::generic_category(), "[TCPBase] " + msg + ": " + strerror(errno));
    }

    // Core connection handling logic (intended to be blocking)
    virtual void handle_connection(int client_fd) {
        try {
            DEBUG("Base handler started for FD:", client_fd);

            // 1. Parse request (blocking read)
            HttpMessage request = HttpMessage::parse(client_fd);
            DEBUG("Parsed request", request.headers, request.start_line);

            
            std::vector<char> body_to_send = request.body; 
            std::string response_body_str(body_to_send.begin(), body_to_send.end()); 
            std::string headers =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: " + std::to_string(body_to_send.size()) + "\r\n"
                "Connection: close\r\n\r\n";

            DEBUG("Base handler sending response headers:", headers);
            DEBUG("Base handler sending response body:", response_body_str);

            // 3. Send response (blocking write)
            if (!send_all(client_fd, headers.data(), headers.size()) ||
                !send_all(client_fd, body_to_send.data(), body_to_send.size())) {
                 log_error("Failed to send complete response to FD " + std::to_string(client_fd));
            } else {
                 DEBUG("Base handler response sent successfully to FD:", client_fd);
            }

        } catch (const std::exception &e) {
            log_error("Exception during base handle_connection for FD " + std::to_string(client_fd) + ": " + e.what());
             
            try {
                 std::string error_response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                 send_all(client_fd, error_response.data(), error_response.size());
            } catch(...) { /* Ignore errors during error reporting */ }
            
        } catch (...) {
             log_error("Unknown exception during base handle_connection for FD " + std::to_string(client_fd));
             
            try {
                 std::string error_response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                 send_all(client_fd, error_response.data(), error_response.size());
            } catch(...) { /* Ignore errors during error reporting */ }
        }
         DEBUG("Base handler finished for FD:", client_fd);
         // socket will not be closed here 
    }

    
    virtual bool send_all(int socket, const char* data, size_t length) {
        size_t total_sent = 0;
        while (total_sent < length) {
            ssize_t sent = send(socket, data + total_sent, length - total_sent, MSG_NOSIGNAL);
            if (sent == -1) {
                if (errno == EPIPE || errno == ECONNRESET) {
                    log_error("Send failed: Client disconnected (FD: " + std::to_string(socket) + ")");
                } else {
                    log_error("Send error on FD " + std::to_string(socket) + ": " + strerror(errno));
                }
                return false;
            }
             if (sent == 0) {
                
                log_error("Send returned 0 unexpectedly on FD " + std::to_string(socket));
                return false;
            }
            total_sent += sent;
        }
        DEBUG("Sent", total_sent, "bytes to FD:", socket);
        return true;
    }


public:
    TCPServer(int port) : server_fd(-1), port(port) {
         DEBUG("Base TCPServer constructor for port", port);
    }

    
    virtual ~TCPServer() {
        log("Base TCPServer destructor called.");
        close_socket(server_fd); 
    }

    
    
    virtual void start() {
        log("Starting base server setup...");
        if (server_fd >= 0) {
             log("Server already started?");
             return; 
        }

        
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            throw_system_error("socket creation failed");
        }
        DEBUG("Socket created", server_fd);

        
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
            close_socket(server_fd); 
            throw_system_error("setsockopt(SO_REUSEADDR) failed");
        }
        DEBUG("SO_REUSEADDR set");

        
        struct sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            close_socket(server_fd);
            throw_system_error("bind failed on port " + std::to_string(port));
        }
        DEBUG("Socket bound to port", port);

        if (listen(server_fd, SOMAXCONN) < 0) {
            close_socket(server_fd);
            throw_system_error("listen failed");
        }
        DEBUG("Socket listening");
        log("Base server socket setup complete. Listening on port " + std::to_string(port));
    }

    //run virtual - Default implementation is single-threaded accept->handle->close
    virtual void run() {
        log("Running base single-threaded accept loop...");
         if (server_fd < 0) {
             throw std::runtime_error("Server not started before running.");
         }

        while (true) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            // Accept connection (blocking)
            DEBUG("Base run() waiting on accept()...");
            int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                
                 log_error("accept failed: " + std::string(strerror(errno)));
                 // In a real scenario, might need more robust handling (e.g., check for EINTR)
                 // or maybe just break the loop on severe errors.
                 // For simplicity here, we log and continue.
                 if(errno == EINVAL || errno == EBADF || errno == ENOTSOCK) break; // Non-recoverable?
                 std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Avoid busy loop on some errors
                continue;
            }

            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            log("Connection accepted from " + std::string(client_ip) + ":"
                + std::to_string(ntohs(client_addr.sin_port)) + " [FD: " + std::to_string(client_fd) + "]");

            // Handle connection IN THE SAME THREAD
            try {
                handle_connection(client_fd);
            } catch (const std::exception& e) {
                log_error("Unhandled exception from handle_connection in base run loop: " + std::string(e.what()));
            } catch (...) {
                log_error("Unknown unhandled exception from handle_connection in base run loop.");
            }


            // Close connection IN THE SAME THREAD
            close_socket(client_fd);
            log("Connection closed for FD " + std::to_string(client_fd));
        }
        log("Base run loop finished."); 
    }

    
    virtual void stop() {
         log("Base stop() called.");

         if (server_fd >= 0) {
             log("Shutting down listening socket to interrupt accept().");

             shutdown(server_fd, SHUT_RD);

         }
    }

    
    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;
};

#endif 