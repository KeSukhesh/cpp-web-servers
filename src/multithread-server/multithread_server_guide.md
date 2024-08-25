# Turning Our Single-Threaded Web Server into a Multithreaded Server (in C++)

Right now, the server will process each request in turn, meaning it won’t process a second connection until the first is finished processing. If the server received more and more requests, this serial execution would be less and less optimal. If the server receives a request that takes a long time to process, subsequent requests will have to wait until the long request is finished, even if the new requests can be processed quickly. We’ll need to fix this, but first, we’ll look at the problem in action.

### Improving Throughput with a Thread Pool
A thread pool is a group of spawned threads that are waiting and ready to handle a task. When the program receives a new task, it assigns one of the threads in the pool to the task, and that thread will process the task. The remaining threads in the pool are available to handle any other tasks that come in while the first thread is processing. When the first thread is done processing its task, it’s returned to the pool of idle threads, ready to handle a new task. A thread pool allows you to process connections concurrently, increasing the throughput of your server.

We’ll limit the number of threads in the pool to a small number to protect us from Denial of Service (DoS) attacks; if we had our program create a new thread for each request as it came in, someone making 10 million requests to our server could create havoc by using up all our server’s resources and grinding the processing of requests to a halt.

Rather than spawning unlimited threads, then, we’ll have a fixed number of threads waiting in the pool. Requests that come in are sent to the pool for processing. The pool will maintain a queue of incoming requests. Each of the threads in the pool will pop off a request from this queue, handle the request, and then ask the queue for another request. With this design, we can process up to N requests concurrently, where N is the number of threads. If each thread is responding to a long-running request, subsequent requests can still back up in the queue, but we’ve increased the number of long-running requests we can handle before reaching that point.

This technique is just one of many ways to improve the throughput of a web server. Other options you might explore are the fork/join model, the single-threaded async I/O model, or the multi-threaded async I/O model. If you’re interested in this topic, you can read more about other solutions and try to implement them; with a low-level language like Rust (or C++!), all of these options are possible.

Before we begin implementing a thread pool, let’s talk about what using the pool should look like. When you’re trying to design code, writing the client interface first can help guide your design. Write the API of the code so it’s structured in the way you want to call it; then implement the functionality within that structure rather than implementing the functionality and then designing the public API. We’ll use compiler-driven development here, writing the code that calls the functions we want, and then we’ll look at errors from the compiler to determine what we should change next to get the code to work. Before we do that, however, we’ll explore the technique we’re not going to use as a starting point.

#### Updating our CMakeList
Let's make a copy of _src/single-thread-server/main.cpp_ so that we can observe the difference in performance we'll get with our multithreaded server later. After copying our file into a new _src/multithread-server/main.cpp_ file, note that while we still compile inside _/build/**_ with `cmake ..` and `make`, we first need to change change the `CMakeLists.txt` file in the rooty directory to:
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

add_executable(multi-server
    src/multithread-server/main.cpp
)

target_link_libraries(multi-server
    Boost::system
    Threads::Threads
)

add_executable(single-server
    src/single-thread-server/main.cpp
)

target_link_libraries(single-server
    Boost::system
    Threads::Threads
)
```

#### Spawning a Thread for Each Request
Now, let’s explore how our code might look if it did create a new thread for every connection. As mentioned earlier, this isn’t our final plan due to the problems with potentially spawning an unlimited number of threads, but it is a starting point to get a working multithreaded server first. Then we’ll add the thread pool as an improvement, and contrasting the two solutions will be easier. The code below shows the changes to make to `main.cpp` to spawn a thread to handle each stream within the for loop. Make sure you `#include` _\<thread\>_ and _\<memory\>_.

*New* File: _src/multithread-server/main.cpp_
```cpp
int main() {
    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 7878));

        while (true) {
            // Create a socket for the incoming connection
            auto socket = std::make_unique<tcp::socket>(io_context);
            acceptor.accept(*socket);

            // Spawn a new thread to handle the connection
            std::thread([socket = std::move(socket)]() mutable {
                handle_connection(std::move(*socket));
            }).detach();
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
```
If you run this code and load _/sleep_ in your browser, then / in two more browser tabs, you’ll indeed see that the requests to / don’t have to wait for _/sleep_ to finish. However, as we mentioned, this will eventually overwhelm the system because you’d be making new threads without any limit.

#### Creating a Finite Number of Threads
We want our thread pool to work in a similar, familiar way so switching from threads to a thread pool doesn’t require large changes to the code that uses our API.

File: _src/multithread-server/main.cpp_
```cpp
int main() {
    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 7878));

        ThreadPool pool(4); // Create a thread pool with 4 threads

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
```

We create a new `ThreadPool` with a configurable number of threads, in this case four. Then in the loop, `pool.execute()` has a similar interface as `std::thread` in that it takes a closeure the pool should run for each stream. We need to implement the `Threadpool` class (including the `pool.execute()` method) so it takes the closure and gives it a thread in the pool to run. This code won't yet compile, but we'll try so the compiler can guide us in how to fix it.


#### Building a ThreadPool Using Compiler Driven Development
Make the changes above to _src/multithread-server/main.cpp_, and then have a look at the compiler errors we get to drive our development. Here is the first error we get:
```
KeSukhesh: ~/projects/cpp-web-servers/src/multithread-server$ make
main.cpp: In function ‘int main()’:
main.cpp:142:9: error: ‘ThreadPool’ was not declared in this scope; did you mean ‘thread_local’?
  142 |         ThreadPool pool(4); // Create a thread pool with 4 threads
      |         ^~~~~~~~~~
      |         thread_local
```

Great! This error tells us we need a `ThreadPool` type or module, so we'll build one now. We'll add this class to the current file for now, then refactor later for this example's sake.

First, let's make a new directory called `threadpool`, the path to this directory should be _src/multithread-server/threadpool/**_. Our first file will be our `ThreadPool` interface:

*New* File: _src/multithread-server/threadpool/ThreadPool.h_
```cpp
#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>

class ThreadPool {
public:
    explicit ThreadPool(size_t threads);
    ~ThreadPool();

    template<class F>
    void execute(F&& f);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

#include "ThreadPool.tpp" // Include the template implementation
```
This is a standard interface declaration. Note that we left a reference to our "template implementation", this will be the implementation of our `execute()` method which we shall define later. If we `#include "ThreadPool.h"` in our _main.cpp_ file, the next error that we need to address becomes:
```
KeSukhesh: ~/projects/cpp-web-servers/src/multithread-server$ make
main.cpp:23:10: error: ‘void ThreadPool::execute(F&&) [with F = main()::<lambda()>]’, declared using local type ‘main()::<lambda()>’, is used but never defined [-fpermissive]
   23 |     void execute(F&& f);
      |          ^~~~~~~
```
This error indicates that we have declared a `Threadpool::execute` method but have not defined it. This is the template implementation I mentioned earlier.

*New* File: _src/multithread-server/threadpool/ThreadPool.tpp_
```cpp
template<class F>
void ThreadPool::execute(F&& f) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace(std::forward<F>(f));
    }
    condition.notify_one();
}
```
The `execute` method is a template function, which means it can accept any callable object (function, lambda, functor, etc.) as its parameter. The template parameter `F` represents the type of the callable object. Some notes about this code:

- We first call `std::unique_lock<std::mutex> lock(queue_mutex);` to lock the `queue_mutex` to ensure that all access to the task queue is thread-safe. The `unique_luck` is a RAII-style mechanism that locks the mutex when the lock object is created and unlocks it when the lock object goes out of the scope.
- We then check if the threadpool is stopped, in which case we throw a `std::runtime_error` to prevent adding new tasks to a stopped thread pool.
- We then add the task to the queue with `tasks.emplace(std::forward<F>(f));`, where `std::forward<F>(f)` ensures that the callable object `f` is perfectly forwarded, preserving its value category (lavlue or rvalue).
- Lastly, we notify one worker thread with `condition.notify_one()` to wake up a thread, acquire the lock, and execute the task.

If we have a _ThreadPool.h_ interface, we need a _ThreadPool.cpp_ file that implements that _ThreadPool.h_ interface! It needs to include a constructor that creates the specified number of worker threads and a destructor that joins all threads. What this means is explained later below.

*New* File: _src/multithread-server/threadpool/ThreadPool.cpp_
```cpp
#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t threads)
    : stop(false)
{
    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back(
            [this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task();
                }
            }
        );
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers)
        worker.join();
}
```
TODO: Break down this code.

Lastly, lets include the path to our `threadpool` directory in our `CMakeLists.txt` file so that it compiles correctly.
```cmake
cmake_minimum_required(VERSION 3.10)
project(cpp-web-servers)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

set(CMAKE_CXX_STANDARD 20)

// include the threadpool directory
include_directories(${PROJECT_SOURCE_DIR}/src/multithread-server/threadpool)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

// compile main.cpp alongside ThreadPool.cpp
add_executable(multi-server
    src/multithread-server/main.cpp
    src/multithread-server/threadpool/ThreadPool.cpp
)

target_link_libraries(multi-server
    Boost::system
    Threads::Threads
)

add_executable(single-server
    src/single-thread-server/main.cpp
)

target_link_libraries(single-server
    Boost::system
    Threads::Threads
)
```

Our code should now compile!
Congrats! We’ve now completed our project; we have a basic web server that uses a thread pool to respond asynchronously. We’re able to perform a graceful shutdown of the server, which cleans up all the threads in the pool. After changing the `main()` function so that it accepts a parameter `N` to create `N` threads for the server's thread pool, the file now looks like this:

File: _src/multithread-server/main.cpp_
```cpp
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
            // Simulate a slow response by sleeping for 15 seconds
            std::this_thread::sleep_for(std::chrono::seconds(15));
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
```

If we go ahead and run the same manual test we performed on the single-threaded server earlier, where we request _/sleep_ and then _/_, we see that the second request does not need to wait for the first, and that both are handled by different threads! After making some more requests, here is what our logger now outputs:
```
KeSukhesh: ~/projects/cpp-web-servers/src/multithread-server$ ./multi-server
Thread ID: 139841854359104 - Handling request at 2024-08-25 13:28:33
Thread ID: 139841845966400 - Handling request at 2024-08-25 13:28:33
Thread ID: 139841829180992 - Handling request at 2024-08-25 13:28:36
Thread ID: 139841837573696 - Handling request at 2024-08-25 13:28:40
Thread ID: 139841845966400 - Handling request at 2024-08-25 13:28:40
Thread ID: 139841829180992 - Handling request at 2024-08-25 13:28:43
Thread ID: 139841845966400 - Handling request at 2024-08-25 13:28:43
Thread ID: 139841829180992 - Handling request at 2024-08-25 13:28:44
```
Compared to our single-threaded server, who's output looked something like this:
```
KeSukhesh: ~/projects/cpp-web-servers/src/single-thread-server$ ./single-server
Thread ID: 139623378351936 - Handling request at 2024-08-25 13:27:38
Thread ID: 139623378351936 - Handling request at 2024-08-25 13:27:38
Thread ID: 139623378351936 - Handling request at 2024-08-25 13:28:01
Thread ID: 139623378351936 - Handling request at 2024-08-25 13:28:02
Thread ID: 139623378351936 - Handling request at 2024-08-25 13:28:02
Thread ID: 139623378351936 - Handling request at 2024-08-25 13:28:02
```
It is evident that not only are we using multiple threads to handle different requests, but the server is now much faster by doing so! This is where the Rust guide I've been translating ends, so everything past here are simple extensions I decided to throw in :)

Let's look at the different ways we can test our code!