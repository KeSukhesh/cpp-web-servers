// This C++ code is derived from the rust-lang book by Nicholas Matsakis and Aaron Turon.
// Licensed under the MIT License. See LICENSE file for details.
#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include <fstream>
#include <iomanip>

#include "ThreadPool.h"

using boost::asio::ip::tcp;

std::string read_file_to_string(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file " + filename);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void handle_connection(tcp::socket socket) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now), "%Y-%m-%d %X");
    std::cout << "Worker Thread ID: " << std::this_thread::get_id() << " - Handling request at " << ss.str() << std::endl;

    try {
        boost::asio::streambuf buffer;
        std::istream input_stream(&buffer);
        boost::asio::read_until(socket, buffer, "\r\n");
        std::string request_line;
        std::getline(input_stream, request_line);
        if (!request_line.empty() && request_line.back() == '\r') {
            request_line.pop_back();
        }

        std::string status_line;
        std::string filename;
        if (request_line == "GET / HTTP/1.1") {
            status_line = "HTTP/1.1 200 OK";
            filename = "../src/util/hello.html";
        } else if (request_line == "GET /sleep HTTP/1.1") {
            // Simulate a slow response by sleeping for 5 seconds
            std::this_thread::sleep_for(std::chrono::seconds(5));
            status_line = "HTTP/1.1 200 OK";
            filename = "../src/util/hello.html";
        } else {
            status_line = "HTTP/1.1 404 NOT FOUND";
            filename = "../src/util/404.html";
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

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <number_of_threads>" << std::endl;
            return 1;
        }

        int num_threads = std::stoi(argv[1]);
        if (num_threads <= 0) {
            std::cerr << "The number of threads must be a positive integer." << std::endl;
            return 1;
        }
        ThreadPool pool(num_threads);
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 7878));

        std::cout << "Multithreaded Server Running with " << num_threads << " threads..." << std::endl;
        while (true) {
            auto socket = std::make_shared<tcp::socket>(io_context);
            acceptor.accept(*socket);

            // Use the thread pool to execute the connection handler
            pool.execute([socket]() mutable {
                handle_connection(std::move(*socket));
            });
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
