#pragma once
#include <cstddef>
#include <vector>
#include <string>
#include <stdexcept>
#include <unistd.h>
#include <sys/uio.h>  // for readv
#include <algorithm>

class HttpReader {
    int fd_;
    std::vector<char> buffer_;
    size_t bufflen_ = 0;
    size_t pos_ = 0;
    static const size_t DEFAULT_BUFSIZE = 16 * 1024; // 16KB buffer

public:
    explicit HttpReader(int fd, size_t buf_size = DEFAULT_BUFSIZE) 
        : fd_(fd), buffer_(buf_size) {}

    // Optimized: Reads until delimiter with buffering
    std::string read_until(const std::string& delimiter) {
        std::string result;
        while (true) {
            // Refill buffer if needed
            if (pos_ >= buffer_.size()) {
                refill_buffer();
                if(bufflen_ == 0) break; // EOF 
            }

            // Scan buffer for delimiter
            size_t remaining = bufflen_ - pos_;
            const char* start = buffer_.data() + pos_;
            
            if (auto it = std::search(start, start + remaining,
                                    delimiter.begin(), delimiter.end());
                it != start + remaining) {
                // Found delimiter
                size_t len = it - start + delimiter.size();
                result.append(start, len);
                pos_ += len;
                return result;
            }

            // Append partial data
            result.append(start, remaining);
            pos_ = buffer_.size(); // Force refill
        }
        return result;
    }

    // Reads exactly N bytes with buffering
    std::vector<char> read_fixed(size_t length) {
        std::vector<char> result;
        result.reserve(length);

        while (result.size() < length) {
            if (pos_ >= buffer_.size()) {
                refill_buffer();
                if (bufflen_ == 0) break; // EOF
            }

            size_t remaining = bufflen_ - pos_;
            size_t needed = length - result.size();
            size_t to_copy = std::min(remaining, needed);

            result.insert(result.end(), 
                        buffer_.begin() + pos_,
                        buffer_.begin() + pos_ + to_copy);
            pos_ += to_copy;
        }

        if (result.size() != length) {
            throw std::runtime_error("Short read");
        }
        return result;
    }

    // Handles chunked transfer encoding
    std::vector<char> read_chunked() {
        std::vector<char> body;
        while (true) {
            // Read chunk size line
            std::string line = read_until("\r\n");
            line.resize(line.size() - 2); // Trim \r\n

            unsigned long chunk_size;
            try {
                chunk_size = std::stoul(line, nullptr, 16);
            } catch (...) {
                throw std::runtime_error("Invalid chunk size");
            }

            if (chunk_size == 0) {
                read_until("\r\n"); // Trailing headers
                break;
            }

            // Read chunk data
            auto chunk = read_fixed(chunk_size);
            body.insert(body.end(), chunk.begin(), chunk.end());
            
            // Read trailing \r\n
            read_until("\r\n");
        }
        return body;
    }

private:
    void refill_buffer() {
        pos_ = 0;
        ssize_t n = read(fd_, buffer_.data(), buffer_.size());
        if (n < 0) throw std::runtime_error("Read error");
        bufflen_ = n;
    }
};