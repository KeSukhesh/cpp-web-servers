#define BOOST_TEST_MODULE ServerUnitTests
#include <boost/test/included/unit_test.hpp>
#include <boost/asio.hpp>
#include <thread>

using namespace boost::asio::ip;

BOOST_AUTO_TEST_CASE(test_handle_connection_root) {
    boost::asio::io_context io_context;
    tcp::socket socket(io_context);
    socket.connect(tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 7878));

    std::string request = "GET / HTTP/1.1\r\n\r\n";
    boost::asio::write(socket, boost::asio::buffer(request));

    boost::asio::streambuf response;
    boost::asio::read_until(socket, response, "\r\n");

    std::istream response_stream(&response);
    std::string status_line;
    std::getline(response_stream, status_line);
    if (!status_line.empty() && status_line.back() == '\r') {
        status_line.pop_back();
    }
    BOOST_CHECK_EQUAL(status_line, "HTTP/1.1 200 OK");
}

BOOST_AUTO_TEST_CASE(test_handle_connection_404) {
    // Simulate a connection to a nonexistent route and check response
    boost::asio::io_context io_context;
    tcp::socket socket(io_context);
    socket.connect(tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 7878));

    std::string request = "GET /nonexistent HTTP/1.1\r\n\r\n";
    boost::asio::write(socket, boost::asio::buffer(request));

    boost::asio::streambuf response;
    boost::asio::read_until(socket, response, "\r\n");

    std::istream response_stream(&response);
    std::string status_line;
    std::getline(response_stream, status_line);
    if (!status_line.empty() && status_line.back() == '\r') {
        status_line.pop_back();
    }
    BOOST_CHECK_EQUAL(status_line, "HTTP/1.1 404 NOT FOUND");
}

BOOST_AUTO_TEST_CASE(test_handle_connection_sleep) {
    // Simulate a connection to the /sleep route and check the delay
    boost::asio::io_context io_context;
    tcp::socket socket(io_context);
    socket.connect(tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 7878));

    std::string request = "GET /sleep HTTP/1.1\r\n\r\n";

    auto start = std::chrono::steady_clock::now();
    boost::asio::write(socket, boost::asio::buffer(request));

    boost::asio::streambuf response;
    boost::asio::read_until(socket, response, "\r\n");

    std::istream response_stream(&response);
    std::string status_line;
    std::getline(response_stream, status_line);
    if (!status_line.empty() && status_line.back() == '\r') {
        status_line.pop_back();
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;

    BOOST_CHECK_EQUAL(status_line, "HTTP/1.1 200 OK");
    BOOST_CHECK(elapsed_seconds.count() >= 5);
}

