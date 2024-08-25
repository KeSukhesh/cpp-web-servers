# Building a Single-Threaded Web Server in C++

### Firstly, a Little About TCP and HTTP
The two main protocols involved in web servers are _Hypertext Transfer Protocol_ _(HTTP)_ and _Transmission Control Protocol_ _(TCP)_. Both protocols are _request-response_ protocols, meaning a _client_ initiates requests and a _server_ listens to the requests and provides a response to the client. The contents of those requests and responses are defined by the protocols.

TCP is the lower-level protocol that describes the details of how information gets from one server to another but doesn’t specify what that information is. HTTP builds on top of TCP by defining the contents of the requests and responses. It’s technically possible to use HTTP with other protocols, but in the vast majority of cases, HTTP sends its data over TCP. We’ll work with the raw bytes of TCP and HTTP requests and responses.

#### Listening to the TCP Connection
*New* File: _src/single-thread-server/main.cpp_
```cpp
#include <iostream>
#include <thread>
#include <vector>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

void handle_client(tcp::socket socket) {
    std::cout << "Connection established!" << std::endl;
    // You can handle the client here, e.g., read/write data
}

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 7878));
        std::vector<std::thread> threads;

        while (true) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            threads.emplace_back(std::thread(handle_client, std::move(socket)));
        }

          for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
```
Notes about this code:
- `boost/asio.hpp` is a networking library, where `boost::asio` is used to handle TCP connections in C++. It provides a similar level of abstraction as Rust's `TcpListener`.
- `tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 7878));` creates a TCP listener on `127.0.0.1:7878`.
- `acceptor.accept(socket);` waits for an incoming connection and accepts it.
- `threads.emplace_back(std::thread(handle_client, std::move(socket)));` spawns a new thread to handle each client connection.

In this snippet, we can listen for TCP connections at the address `127.0.0.1:7878`. In the address, the section before the colon is an IP address representing your computer, and `7878` is the port. We use this port because HTTP isn't normally accepted on this port so our server is unlikely to conflict with any other web server you might have running on your machine.

#### Setting up our CMake and Makefiles

Create a new `CMakeLists.txt` file in the root directory:

*New* File: _CmakeLists.txt_
```cmake
cmake_minimum_required(VERSION 3.10)
project(cpp-web-servers)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

set(CMAKE_CXX_STANDARD 20)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

add_executable(single-server
    src/single-thread-server/main.cpp
)

target_link_libraries(single-server
    Boost::system
    Threads::Threads
)
```
Compiling with this makefile is the equivalent of compiling our program with `g++ -std=c++20 -o main main.cpp -lboost_system -lpthread`. Create a new folder in our root directory called _build_ and `cd` into `build`. Now run `cmake ..`, you should see something similar to this output:
```
KeSukhesh: ~/projects/cpp-web-servers/build$ cmake ..
-- Configuring done (0.0s)
-- Generating done (0.0s)
-- Build files have been written to: /home/kesukhesh/projects/cpp-web-servers/build
```
Then run `make`, and you should see something similar to:
```
KeSukhesh: ~/projects/cpp-web-servers/build$ make
[ 33%] Building CXX object CMakeFiles/single-thread-server.dir/src/single-thread-server/main.cpp.o
[ 66%] Linking CXX executable single-server
[100%] Built target single-server
```

Now, open up `127.0.0.1:7878` on your browser, you should be able to see a message that says "_127.0.0.1 didn't send any data - ERR_EMPTY_RESPONSE_". This might differ slightly depending on what browser you use. You should see "_Connection Established!_" printing in your terminal.

Sometimes, you’ll see multiple messages printed for one browser request; the reason might be that the browser is making a request for the page as well as a request for other resources, like the _favicon.ico_ icon that appears in the browser tab.

It could also be that the browser is trying to connect to the server multiple times because the server isn’t responding with any data. When the contents of the `while(true)` loop goes out of scope and is dropped at the end of the loop, the connection is closed as part of the `drop` implementation. Browsers sometimes deal with closed connections by retrying, because the problem might be temporary. The important factor is that we’ve successfully gotten a handle to a TCP connection!

#### Reading the Request
Let’s implement the functionality to read the request from the browser! To separate the concerns of first getting a connection and then taking some action with the connection, we’ll start a new function for processing connections. In this new `handle_connection()` function, we’ll read data from the TCP stream and print it so we can see the data being sent from the browser.

File: _src/single-thread-server/main.cpp_
```cpp
#include <iostream>
#include <string>
#include <vector>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

void handle_connection(tcp::socket socket) {
    try {
        boost::asio::streambuf buffer;
        std::istream input_stream(&buffer);
        std::vector<std::string> http_request;

        // Read data until an error or EOF
        boost::asio::read_until(socket, buffer, "\r\n");

        while (true) {
            std::string request_line;
            std::getline(input_stream, request_line);

            // Stop reading if the line is empty (end of headers)
            if (request_line == "\r") {
                break;
            }

            http_request.push_back(request_line);
        }

        // Print the request
        std::cout << "Request: " << std::endl;
        for (const auto& request_line : http_request) {
            std::cout << request_line << std::endl;
        }
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

```
Notes about this code:
- Once again, we're using Boost.Asio for handling TCP connections.
- The `handle_connection()` method handles the connection by reading from the `tcp::socket`. It reads the incoming HTTP request line by line and stores it in a `std::vector<string`.
- `boost::asio::streambuf buffer;`  buffers the data read from the socket. This allows us to read the data line by line.
- `boost::asio::read_until(socket, buffer, "\r\n");` reads the data from the socket until it encounters a newline. We then use `std::getline` to extract each line and add it to the `http_request` vector.
- The browser signals the end of an HTTP request by sending two newline characters in a row, so to get one request from the stream, we take lines until we get a line that is the empty string. Once we’ve collected the lines into the vector, we’re printing them out using pretty debug formatting so we can take a look at the instructions the web browser is sending to our server.
- We then print the collected lines to the console.

When we run this code, we get the following in our terminal:
```
KeSukhesh: ~/projects/cpp-web-servers/src/single-thread-server$ ./single-server
Request:
GET / HTTP/1.1
Host: localhost:7878
Connection: keep-alive
sec-ch-ua: "Not)A;Brand";v="99", "Google Chrome";v="127", "Chromium";v="127"
sec-ch-ua-mobile: ?0
sec-ch-ua-platform: "Windows"
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/127.0.0.0 Safari/537.36
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7
Sec-Fetch-Site: none
Sec-Fetch-Mode: navigate
Sec-Fetch-User: ?1
Sec-Fetch-Dest: document
Accept-Encoding: gzip, deflate, br, zstd
Accept-Language: en-GB,en-US;q=0.9,en;q=0.8
```
Let's take a closer look at HTTP Requests.

HTTP is a text-based protocol, and a request takes this format:

```
Method Request-URI HTTP-Version CRLF
headers CRLF
message-body
```

The first line is the _request line_ that holds information about what the client is requesting. The first part of the request line indicates the _method_ being used, such as `GET` or `POST`, which describes how the client is making this request. Our client used a `GET` request, which means it is asking for information.

The next part of the request line is _/_, which indicates the _Uniform Resource Identifier_ _(URI)_ the client is requesting: a URI is almost, but not quite, the same as a _Uniform Resource Locator_ _(URL)_. The difference between URIs and URLs isn’t important for our purposes in this chapter, but the HTTP spec uses the term URI, so we can just mentally substitute URL for URI here.

The last part is the HTTP version the client uses, and then the request line ends in a _CRLF sequence_. (CRLF stands for _carriage return_ and _line feed_, which are terms from the typewriter days!) The CRLF sequence can also be written as `\r\n`, where `\r` is a carriage return and `\n` is a line feed. The CRLF sequence separates the request line from the rest of the request data. Note that when the CRLF is printed, we see a new line start rather than `\r\n`.

Looking at the request line data we received from running our program so far, we see that `GET` is the method, _/_ is the request URI, and `HTTP/1.1` is the version.

After the request line, the remaining lines starting from `Host:` onward are headers. `GET` requests have no body.

Now that we know what the browser is asking for, let’s send back some data!

#### Writing a Response
We’re going to implement sending data in response to a client request. Responses have the following format:
```
HTTP-Version Status-Code Reason-Phrase CRLF
headers CRLF
message-body
```

The first line is a _status line_ that contains the HTTP version used in the response, a numeric status code that summarizes the result of the request, and a reason phrase that provides a text description of the status code. After the CRLF sequence are any headers, another CRLF sequence, and the body of the response.

Here is an example response that uses HTTP version 1.1, has a status code of 200, an OK reason phrase, no headers, and no body:
`HTTP/1.1 200 OK\r\n\r\n`

The status code 200 is the standard success response. The text is a tiny successful HTTP response. Let’s write this to the stream as our response to a successful request! From the `handle_connection` function, remove the `println!` that was printing the request data and replace it with the code in Listing 20-3.

We added the following two lines to the end of our `try` block in the `handle_connection()` method:
```cpp
// Prepare the HTTP response
std::string response = "HTTP/1.1 200 OK\r\n\r\n";

// Send the HTTP response
boost::asio::write(socket, boost::asio::buffer(response));
```
Notes about this code:
- `boost::asio::write(socket, boost::asio::buffer(response));` sends the response back to the client using the socket.
- The `boost::asio::buffer(response)` converts the string to a buffer that can be sent over the network.

With these changes, let’s run our code and make a request. We’re no longer printing any data to the terminal, so we won’t see any output. When you load _127.0.0.1:7878_ in a web browser, you should get a blank page instead of an error. You’ve just hand-coded receiving an HTTP request and sending a response!
#### Let's Return Real HTML

Let's implement the functionality for returning more than a blank page.

File: src/single-thread-server/hello.html
```html
<!DOCTYPE html>
<html lang="en">
    <head>
        <meta charset="utf-8">
        <title>Hello!</title>
    </head>
    <body>
        <h1>Hello!</h1>
        <p>Hi from Rust</p>
    </body>
</html>
```

This is a minimal HTML5 document with a heading and some text. To return this from the server when a request is received, we’ll modify `handle_connection()` as shown below to read the HTML file, add it to the response as a body, and send it.

First, we changed the previous two lines to these:
```cpp
// Read the HTML file into a string
std::string contents = read_file_to_string("../src/util/hello.html");

// Create the HTTP response
std::string status_line = "HTTP/1.1 200 OK";
std::string length = std::to_string(contents.size());
std::string response = status_line + "\r\nContent-Length: " + length + "\r\n\r\n" + contents;

// Send the HTTP response
boost::asio::write(socket, boost::asio::buffer(response));
```
And after importing the `fstream` and `sstream` libraries, our `read_file_to_string()` method looks like this:
```cpp
std::string read_file_to_string(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file " + filename);
    }

    std::stringstream buffer;
    // Read the file contents into the stringstream
    buffer << file.rdbuf();
    // Convert the stringstream into a string and return it 
    return buffer.str();  
}
```
Notes about this code:
- The function `read_file_to_string()` reads the entire contents of the HTML file we wrote earlier into a `std::string`. It uses `std::stringstream` as a buffer to read the file contents.
- We then prepare the HTTP status line and concatenate the response into a single `std::string` like usual.

After recompiling, and running our binary file, our HTML file is (or at least, should be) displayed on our browser!

Currently, we’re ignoring the request data in `http_request` and just sending back the contents of the HTML file unconditionally. That means if you try requesting _127.0.0.1:7878/something-else_ in your browser, you’ll still get back this same HTML response. At the moment, our server is very limited and does not do what most web servers do. We want to customize our responses depending on the request and only send back the HTML file for a well-formed request to _/_.

#### Validating the Request and Selectively Responding

Right now, our web server will return the HTML in the file no matter what the client requested. Let’s add functionality to check that the browser is requesting _/_ before returning the HTML file and return an error if the browser requests anything else. For this we need to modify `handle_connection()`, as shown below.

```cpp
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

        std::string response;

        // Check if the request is for the root "/"
        if (request_line == "GET / HTTP/1.1") {
            std::string status_line = "HTTP/1.1 200 OK";
            std::string contents = read_file_to_string("../src/util/hello.html");
            std::string length = std::to_string(contents.size());
           response = status_line + "\r\nContent-Length: " + length + "\r\n\r\n" + contents;
        } else {
            // Respond with a 404 Not Found for any other request
            std::string status_line = "HTTP/1.1 404 NOT FOUND";
	      std::string contents = "<html><body><h1>404 Not Found</h1></body></html>";
            std::string length = std::to_string(contents.size());
            response = status_line + "\r\nContent-Length: " + length + "\r\n\r\n" + contents;
        }

        // Send the HTTP response
        boost::asio::write(socket, boost::asio::buffer(response));
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
```
This new code checks the content of the request received against what we know a request for _/_ looks like and adds `if` and `else` blocks to treat requests differently. Some Notes:
- **For the root `/`:** If the request line matches `"GET / HTTP/1.1"`, the server responds with the contents of `hello.html`.
- **For other paths:** The server responds with a simple `404 Not Found` message.

When you run this code now and request _127.0.0.1:7878_; you should get the HTML in hello.html. If you make any other request, such as _127.0.0.1:7878/something-else_, you'll get a connection error saying `404 Not Found` message.

By making a new simple HTML5 file: 404.html
```html
<!DOCTYPE html>
<html lang="en">
    <head>
        <meta charset="utf-8">
        <title>Hello!</title>
    </head>
    <body>
        <h1>Oops!</h1>
        <p>Sorry, I don't know what you're asking for.</p>
    </body>
</html>
```
and changing the `contents` of the response for any other request to `std::string contents = read_file_to_string("../src/util/404.html");`. Our browser should now show the 404 html page when any other request is submitted.

#### After a Touch of Refactoring
Since there is some duplicate code, we can clean up the `handle_connection()`, which now looks like:
```cpp
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
```
Awesome! We now have a simple web server in ~75 lines of C++ code that responds to one request with a page of content and responds to all other requests with a 404 response. Currently, our server runs in a single thread, which means it can only serve one request at a time.

### Simulating a Slow Request in the Current Server Implementation
We’ll look at how a slow-processing request can affect other requests made to our current server implementation. Listing 20-10 implements handling a request to /sleep with a simulated slow response that will cause the server to sleep for 5 seconds before responding. Make sure to `#include` _\<chrono\>_, _\<iomanip\>_, and _\<thread\>_.

Filename: src/single-thread-server/main.cpp
```cpp
void handle_connection(tcp::socket socket) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now), "%Y-%m-%d %X");
    std::cout << "Thread ID: " << std::this_thread::get_id() << " - Handling request at " << ss.str() << std::endl;
    try {
    ...
    // --snip--
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
    // --snip--
    ...
    }
}
```
We first added a simple logger to our `handle_connection()` method, which gives us a visual confirmation of when different threads handle different connections. We then changed the if statement that checks for what the `request_line` is. Try matching a request to _/sleep_ and note how the server will sleep for 5 seconds before succesfully rendering the HTML page. You can see how primitive our server is: real libraries would handle the recognition of multiple requests in a much less verbose way!

After running `cmake ..` and `make` inside _/build/**_, start the server using `./single-server`. Then open two browser windows: one for http://127.0.0.1:7878/ and the other for http://127.0.0.1:7878/sleep. If you enter the / URI a few times, as before, you’ll see it respond quickly. But if you enter /sleep and then load /, you’ll see that / waits until sleep has slept for its full 5 seconds before loading. If you find that 5 seconds is not long enough for you to notice anything, try changing the parameter to `std::chrono::seconds` to 10 or 15 seconds.

For example, request the _/sleep_ page, and immediately request the normal page and note how the second request has to wait for the first to finish. I changed the sleep duration to 15 seconds to make this more noticeable. Here's the output from the logger:
```
KeSukhesh: ~/projects/cpp-web-servers/src/single-thread-server$ ./single-server
Thread ID: 140130192308032 - Handling request at 2024-08-25 12:46:59
Thread ID: 140130192308032 - Handling request at 2024-08-25 12:47:14
```
First, note that both requests use the same thread. Second, that the second request had to wait the 15 seconds before it could be responded to. Try this out for yourself, feel free to make multiple requests and play around with it. There are multiple techniques we could use to avoid requests backing up behind a slow request; the one we’ll implement is a thread pool. Head over to part 2 where we fix our server to handle multiple requests at once.

[Turning Our Single-Threaded Web Server into a Multithreaded Server (in C++)](../multithread-server/multithread_server_guide.md)
