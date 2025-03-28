// main.cpp (Example File Name)
#include "tcp/multithreaded_tcp.hpp"
#include <csignal>
#include <iostream>
#include <cstring> // For memset in signal handler setup

// --- Graceful Shutdown Handling ---
namespace {
    // Use a pointer to the BASE server class for the signal handler
    TCPServer* server_instance_ptr = nullptr;

    void signal_handler(int signal) {
        // Use std::cerr directly in signal handler (more signal-safe than cout potentially)
        std::cerr << "\nCaught signal " << signal << ", initiating graceful shutdown..." << std::endl;
        if (server_instance_ptr) {
            server_instance_ptr->stop(); // Call virtual stop() - will call derived version
        }
        // It's generally safer to avoid complex operations or allocations in signal handlers.
        // Setting a flag for the main loop is often preferred, but calling stop() here is common.
        // Consider re-raising the signal if needed: raise(signal);
        // Or just let the server's run loop terminate due to stop_requested/accept error.
    }
}

int main() {
    // --- Setup Signal Handling ---
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    // Block other signals during handler execution if needed (sa.sa_mask)
    // Use SA_RESTART to auto-restart interrupted syscalls if desired (might interfere with shutdown)
    // sigemptyset(&sa.sa_mask); // Example: Clear mask
    // sa.sa_flags = 0; // Or SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
         std::cerr << "Failed to register SIGINT handler: " << strerror(errno) << std::endl;
         return EXIT_FAILURE;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
         std::cerr << "Failed to register SIGTERM handler: " << strerror(errno) << std::endl;
         return EXIT_FAILURE;
    }
    // Ignore SIGPIPE globally using signal() - easier than handling EPIPE everywhere
    // Or handle EPIPE via send() return value and MSG_NOSIGNAL flag (as done in send_all)
    signal(SIGPIPE, SIG_IGN);
    std::cout << "Registered signal handlers for SIGINT and SIGTERM." << std::endl;


    try {
        // Create an instance of the derived class
        MultiThreadedTCPServer server(8080, 4); // Listen on 8080 with 4 worker threads
        server_instance_ptr = &server; // Set global BASE pointer for signal handler

        server.start(); // Calls derived start() -> base start() -> starts threads
        server.run();   // Calls derived run() -> accept loop dispatching to threads

        server_instance_ptr = nullptr; // Clear pointer after run returns
        std::cout << "Server run() method returned. Main exiting." << std::endl;

    } catch (const std::exception& e) {
        server_instance_ptr = nullptr; // Clear pointer on error too
        // Use std::cerr for errors
        std::cerr << "Server terminated due to exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
         server_instance_ptr = nullptr;
         std::cerr << "Server terminated due to unknown exception." << std::endl;
        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;
}