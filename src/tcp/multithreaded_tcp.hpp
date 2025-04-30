// multi_threaded_tcp_server.hpp (Example Header Name)
#ifndef MULTI_THREADED_TCP_SERVER_HPP
#define MULTI_THREADED_TCP_SERVER_HPP

#include "tcp.hpp" // Include the base class header
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <chrono>       // For sleep

class MultiThreadedTCPServer : public TCPServer {
private:
    const size_t num_threads;

    // Thread pool components (private to this derived class)
    std::vector<std::thread> workers;
    std::queue<int> client_queue;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop_requested{false}; // Use a different name to avoid confusion

    // Override logging to add derived class identifier
    void log(const std::string& message) override {
        std::lock_guard<std::mutex> lock(io_mutex); // Use base class io_mutex
        std::cout << "[TCPMulti] " << message << std::endl;
    }

    void log_error(const std::string& message) override {
        std::lock_guard<std::mutex> lock(io_mutex); // Use base class io_mutex
        std::cerr << "[TCPMulti ERROR] " << message << std::endl;
    }


    // Function executed by worker threads
    void worker_thread() {
        log("Worker thread started. ID: " + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
        while (true) {
            int client_fd = -1; // Initialize to invalid FD

            
            { 
                std::unique_lock<std::mutex> lock(queue_mutex);
                condition.wait(lock, [this] { return !client_queue.empty() || stop_requested; });

                
                if (stop_requested && client_queue.empty()) {
                     log("Worker thread exiting gracefully.");
                    return; // Exit the thread loop
                }

                // Check if queue has work before accessing front()
                if (!client_queue.empty()) {
                    client_fd = client_queue.front();
                    client_queue.pop();
                    DEBUG("Worker thread picked up client FD:", client_fd);
                } else {
                    // Spurious wakeup or stop requested but queue became empty
                    // before this thread got the lock. Just loop again.
                    DEBUG("Worker thread woke up but queue is empty/stop requested.");
                    continue;
                }
            } // Lock released here

            
            if (client_fd >= 0) {
                log("Worker thread handling connection for FD " + std::to_string(client_fd));

                try {
                    TCPServer::handle_connection(client_fd); 
                } catch (const std::exception& e) {
                     log_error("Worker thread caught unhandled exception from handle_connection: " + std::string(e.what()));
                } catch (...) {
                     log_error("Worker thread caught unknown unhandled exception from handle_connection.");
                }

                TCPServer::close_socket(client_fd);
                log("Worker thread finished and closed FD " + std::to_string(client_fd));
            }
        }
    }

public:
    // Constructor: Initialize base, set threads, check thread count
    MultiThreadedTCPServer(int port, size_t threads = std::thread::hardware_concurrency()) :
        TCPServer(port), // Call base class constructor
        num_threads(threads > 0 ? threads : 4)
    {
        log("MultiThreadedTCPServer constructor for port " + std::to_string(port) +
            " with " + std::to_string(num_threads) + " threads.");
        if (this->num_threads == 0) {
             throw std::runtime_error("Invalid number of threads (0) specified for MultiThreadedTCPServer.");
        }
    }

    // Override Destructor: Ensure stop is called
    ~MultiThreadedTCPServer() override {
        log("MultiThreadedTCPServer destructor called.");
        if (!stop_requested) {
             // Ensure cleanup happens even if stop() wasn't explicitly called
             stop();
        }
    }

    // Override start: Call base start, then start threads
    void start() override {
        if (!workers.empty()) {
             log("Server threads seem to be already started.");
             return;
        }
        log("Starting multi-threaded server...");

        // 1. Call base class start to setup the listening socket
        TCPServer::start(); // This might throw if setup fails

        // 2. Start worker threads *after* base start succeeds
        stop_requested = false; // Ensure stop flag is reset if start is called again
        workers.reserve(num_threads);
        log("Starting " + std::to_string(num_threads) + " worker threads...");
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back(&MultiThreadedTCPServer::worker_thread, this);
        }
        log("Multi-threaded server started successfully.");
    }

    // Override run: Accept loop that dispatches to thread pool
    void run() override {
         log("Running multi-threaded accept loop...");
         if (server_fd < 0) { // Check protected base member
             throw std::runtime_error("Server not started before running.");
         }
         if (workers.empty()) {
              throw std::runtime_error("Worker threads not started before running.");
         }

        while (!stop_requested) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            // Accept connection (blocking call on base server_fd)
             DEBUG("Main thread waiting on accept()...");
            int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);

            if (client_fd < 0) {
                // Check if the error is due to server stopping
                if (stop_requested) {
                    log("Accept interrupted gracefully by stop request.");
                    break; // Exit loop
                }
                 // Check for other common accept errors
                if (errno == EINTR) {
                    DEBUG("accept() interrupted by signal, continuing...");
                    continue; // Interrupted by signal, just retry
                } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                     // Should not happen with blocking sockets, but log if it does
                     log_error("accept() returned EAGAIN/EWOULDBLOCK unexpectedly.");
                     std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Avoid busy loop
                     continue;
                } else {
                    // Log other errors (e.g., EMFILE, ENFILE, ECONNABORTED)
                     log_error("accept failed: " + std::string(strerror(errno)));
                     // Depending on error, might break or sleep/continue
                     std::this_thread::sleep_for(std::chrono::milliseconds(100));
                     continue; // Continue trying to accept
                }
            }

            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            log("Connection accepted from " + std::string(client_ip) + ":"
                + std::to_string(ntohs(client_addr.sin_port)) + " [FD: " + std::to_string(client_fd) + "]");

            
            { // add client_fd by taking RAII lock 
                std::lock_guard<std::mutex> lock(queue_mutex);
                client_queue.push(client_fd);
                DEBUG("Pushed client FD to queue:", client_fd);
            } 

            
            condition.notify_one();
            DEBUG("Notified one worker thread.");
        }

         log("Accept loop finished.");
         // If loop exited, ensure stop logic runs to clean up threads.
         // Calling stop() here might be redundant if destructor handles it,
         // but can be explicit.
         if (!stop_requested) {
            stop(); // Ensure proper cleanup if loop exited for other reasons
         }
    }

    // Override stop: Signal threads and join them
    void stop() override {
        log("Stopping multi-threaded server...");
        if (stop_requested.exchange(true)) {
            log("Stop already requested.");
            return; // Already stopping/stopped
        }

        // Call base stop() first - might shut down listening socket to help unblock accept()
        TCPServer::stop();

        // Notify all waiting worker threads
        log("Notifying all worker threads to stop...");
        condition.notify_all();

        // Wait for all worker threads to finish
        log("Waiting for " + std::to_string(workers.size()) + " worker threads to join...");
        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                 DEBUG("Joining worker thread ID:", worker.get_id());
                 worker.join();
                 DEBUG("Worker thread joined.");
            }
        }
        log("All worker threads joined.");
        workers.clear(); // Clear the vector of threads

         // Clear the queue (optional, threads should have processed/exited)
         std::lock_guard<std::mutex> lock(queue_mutex);
         while(!client_queue.empty()) {
             int fd = client_queue.front();
             client_queue.pop();
             log_error("Found unprocessed FD in queue during stop: " + std::to_string(fd) + ". Closing.");
             TCPServer::close_socket(fd); // Close any leftover FDs
         }

        log("Multi-threaded server stopped.");
    }

    
    MultiThreadedTCPServer(const MultiThreadedTCPServer&) = delete;
    MultiThreadedTCPServer& operator=(const MultiThreadedTCPServer&) = delete;
};


#endif 