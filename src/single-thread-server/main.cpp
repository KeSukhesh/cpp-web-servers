#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

std::string read_file_to_string(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file " + filename);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();  // Read the file contents into the stringstream
    return buffer.str();
}

void handle_connection(tcp::socket socket) {
    try {
        boost::asio::streambuf buffer;
        std::istream input_stream(&buffer);

        // Read the request line
        boost::asio::read_until(socket, buffer, "\r\n");
        std::string request_line;
        std::getline(input_stream, request_line);

        // Remove the trailing '\r' character if present
        if (!request_line.empty() && request_line.back() == '\r') {
            request_line.pop_back();
        }

        std::string status_line;
        std::string filename;
        // Check if the request is for the root "/"
        if (request_line == "GET / HTTP/1.1") {
            status_line = "HTTP/1.1 200 OK";
            filename = "hello.html";
        } else {
            status_line = "HTTP/1.1 404 NOT FOUND";
            filename = "404.html";
        }

        std::string contents = read_file_to_string(filename);
        std::string length = std::to_string(contents.size());
        std::string response = status_line + "\r\nContent-Length: " + length + "\r\n\r\n" + contents;
        boost::asio::write(socket, boost::asio::buffer(response));
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 7878));

        while (true) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            handle_connection(std::move(socket));
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
