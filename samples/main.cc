#include "uvco.h"

#include <cppcoro/async_scope.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <iostream>
#include <thread>
#include <vector>

uvco::task<> runtestfs(const char *srcPath, const char *dstPath) {
    uvco::fs fsrc, fdst;

    // Open two files concurrently, no need to serialize them.
    co_await cppcoro::when_all(
            fsrc.open(srcPath, UV_FS_O_RDONLY),
            fdst.open(dstPath, UV_FS_O_CREAT | UV_FS_O_WRONLY));

    constexpr size_t bufsz = 1024;
    std::unique_ptr<char[]> buf(new char[bufsz]);
    for (;;) {
        ssize_t rd = co_await fsrc.read(buf.get(), bufsz);
        if (rd <= 0) break;
        co_await fdst.write(buf.get(), rd);
    }

    // this is optional
    co_await cppcoro::when_all(fsrc.close(), fdst.close());
}

cppcoro::task<> handle_connection(uvco::tcp client) {
    std::cout << "new client connection\n";
    auto [nread, buf] = co_await client.read();
    if (nread > 0) {
        co_await client.write(buf.get(), nread);
    }
    std::cout << "handle client connection end\n";
}

uvco::task<> runtesttcpserver() {
    // Use async_scope, so you can handle client connections parallelly
    cppcoro::async_scope scope;
    std::exception_ptr ex;
    try {
        uvco::tcp server;
        server.listen("0.0.0.0", 4321);
        // demo only, just accept 3 connections
        for (int i = 0; i < 3; i++) {
            uvco::tcp client = co_await server.accept();
            scope.spawn(handle_connection(std::move(client)));
        }
    } catch (...) {
        ex = std::current_exception();
    }
    co_await scope.join();
    if (ex) std::rethrow_exception(ex);
}

cppcoro::task<> handle_connection(uvco::pipe client) {
    std::cout << "new client connection\n";
    auto [nread, buf] = co_await client.read();
    if (nread > 0) {
        co_await client.write(buf.get(), nread);
    }
    std::cout << "handle client connection end\n";
}

uvco::task<> runtestpipeserver() {
    cppcoro::async_scope scope;
    uvco::pipe server;
    server.listen("/tmp/uvcopipetest");
    // demo only, just accept 3 connections
    for (int i = 0; i < 3; i++) {
        uvco::pipe client = co_await server.accept();
        scope.spawn(handle_connection(std::move(client)));
    }
    co_await scope.join();
    std::cout << "test end\n";
}

uvco::task<> runtesttcpclient() {
    uvco::tcp client;
    int rc;
    rc = co_await client.connect("127.0.0.1", 4321);
    std::cout << "con: " << rc << std::endl;
    const char *data = "hello";
    rc = co_await client.write(data, strlen(data));
    std::cout << "write: " << rc << std::endl;
    auto [nread, buf] = co_await client.read();
    std::cout << "got: " << nread << "->\n";
    if (nread > 0) {
        std::cout << std::string(buf.get(), nread) << std::endl;
    }
}

uvco::task<> runtestpipeclient() {
    uvco::pipe client;
    int rc;
    rc = co_await client.connect("/tmp/uvcopipetest");
    std::cout << "con: " << rc << std::endl;
    const char *data = "hello";
    rc = co_await client.write(data, strlen(data));
    std::cout << "write: " << rc << std::endl;
    auto [nread, buf] = co_await client.read();
    std::cout << "got: " << nread << "->\n";
    if (nread > 0) {
        std::cout << std::string(buf.get(), nread) << std::endl;
    }
}

uvco::task<> runtestudpserver() {
    uvco::udp server;
    server.bind("0.0.0.0", 4321);
    auto [nread, buf, addr, flags] = co_await server.recv();
    std::cout << "got: " << nread << "->\n";
    if (nread > 0) {
        std::cout << std::string(buf.get(), nread) << std::endl;
    }
}

uvco::task<> runtestudpclient() {
    uvco::udp client;
    struct sockaddr addr;
    uv_ip4_addr("127.0.0.1", 4321, (sockaddr_in *)&addr);
    const char *data = "hello world";
    int sz = co_await client.send(&addr, data, strlen(data));
    std::cout << "write: " << sz << std::endl;
}

int main(int argc, char **argv) {
    try {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " <src-file-path> <dst-file-path>\n";
            exit(1);
        }
        auto task = runtestfs(argv[1], argv[2]);
        uvco::scheduler::get_default()->schedule(task);
    } catch (const uvco::exception &ex) {
        std::cout << "got exception: " << ex.what() << std::endl;
    }
}
