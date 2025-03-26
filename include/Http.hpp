// Http.hpp
#pragma once  // Ensures the header is included only once
#include <map>
#include <string>

class Http {
public:
    
    static std::string create(
        int status_code,
        const std::string &content, 
        const std::string content_type = "text/plain",
        const std::map<std::string, std::string> &headers = {}
    ) {
        std::string response = "";

        response += "HTTP/1.1 " + std::to_string(status_code) + " " 
            + get_status_message(status_code) + "\r\n";
        
        response += "Content-Type: " + content_type + "\r\n";
        response += "Content-Length: " + std::to_string(content.size()) + "\r\n";

        response += "Connection: close\r\n";

        for(auto &[key, value]: headers) {
            response += key + ": " + value + "\r\n";
        }

        // end of response 

        response += "\r\n";

        response += content;

        return response;

    }

private:

    static std::string get_status_message(int status_code) {
        switch(status_code) {
            case 200: return "OK";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            default: return "Unknown";
        }
    }
};