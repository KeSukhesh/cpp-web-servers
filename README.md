# cpp-web-servers
A simple guide to multithreaded web servers in C++!

This repository sort of "just happened" as I was trying to learn how to actually create a (multithreaded) web server in C++. I came across [this](https://doc.rust-lang.org/book/ch20-00-final-project-a-web-server.html) guide, which teaches the exact thing I needed, but in Rust. Hence, this whole repository is pretty much a byproduct of me translating the Rust guide into a C++ version. I thought I'd leave these markdown files up in case it they help anyone else. All credit for this guide goes the rust-lang book, I'm just a humble translator!

Just like the original guide, this one is also split into two major parts:
- 1) [We build a Single-Threaded Web Server](/src/single-thread-server/single_thread_server_guide.md)
- 2) [We turn our Single-Threaded Server into a Multithreaded Server](src/multithread-server/multithread_server_guide.md)
