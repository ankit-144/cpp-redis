#pragma once
#include "http_reader.hpp"
#include <stdexcept>
#include <string>
#include <map>
#include <cctype>

struct HttpMessage {
    std::string start_line;
    std::map<std::string, std::string> headers;
    std::vector<char> body;

    static HttpMessage parse(int fd) {
        HttpReader reader(fd);
        HttpMessage msg;

        // 1. Parse headers
        std::string headers_str = reader.read_until("\r\n\r\n");
        parse_start_line(headers_str, msg);
        parse_headers(headers_str, msg);

        // 2. Parse body
        if (msg.headers.count("transfer-encoding")) {
            if (msg.headers["transfer-encoding"] == "chunked") {
                msg.body = reader.read_chunked();
            }
        } else if (msg.headers.count("content-length")) {
            size_t len = std::stoul(msg.headers["content-length"]);
            msg.body = reader.read_fixed(len);
        }

        return msg;
    }

private:
    static void parse_start_line(const std::string& data, HttpMessage& msg) {
        size_t end = data.find("\r\n");
        if (end == std::string::npos) throw std::runtime_error("Invalid HTTP format");
        msg.start_line = data.substr(0, end);
    }

    static void parse_headers(const std::string& data, HttpMessage& msg) {
        size_t start = data.find("\r\n") + 2;
        
        while (start < data.size()) {
            size_t end = data.find("\r\n", start);
            if (end == std::string::npos) break;

            std::string line = data.substr(start, end - start);
            if (line.empty()) break;

            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;

            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            // Normalize header key (lowercase)
            std::transform(key.begin(), key.end(), key.begin(),
                         [](unsigned char c) { return std::tolower(c); });
            
            // Trim whitespace
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));

            msg.headers[key] = value;
            start = end + 2;
        }
    }
};