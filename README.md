# uvco - libuv C++20 coroutine interface

# How to use
If you already linked libuv, then you can simply add the include directory to your project's include path. Or you can add this project as your subproject then link the interface target uvco.

# Samples

## build

```bash
git submodule update --init
cmake -Bbuild -DCMAKE_CXX_COMPILER=g++-11
cmake --build build -v
```

## `fs read/write`
```c++
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

    co_await cppcoro::when_all(fsrc.close(), fdst.close());
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
```

## `tcp socket server`

```c++
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
            uvco::tcp client(server.scheduler());
            int rc = co_await server.accept(&client);
            assert(rc == 0);
            scope.spawn(handle_connection(std::move(client)));
        }
    } catch (...) {
        ex = std::current_exception();
    }
    co_await scope.join();
    if (ex) std::rethrow_exception(ex);
}
```

## `tcp socket client`

```c++
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
```


## `udp socket server`

```c++
uvco::task<> runtestudpserver() {
    uvco::udp server;
    server.bind("0.0.0.0", 4321);
    auto [nread, buf, addr, flags] = co_await server.recv();
    std::cout << "got: " << nread << "->\n";
    if (nread > 0) {
        std::cout << std::string(buf.get(), nread) << std::endl;
    }
}
```

## `udp socket client`

```c++
uvco::task<> runtestudpclient() {
    uvco::udp client;
    struct sockaddr addr;
    uv_ip4_addr("127.0.0.1", 4321, (sockaddr_in *)&addr);
    const char *data = "hello world";
    int sz = co_await client.send(&addr, data, strlen(data));
    std::cout << "write: " << sz << std::endl;
}
```

## `miscellaneous`

```c++
// May you need some rest in coroutines?
co_await uvco::sleep(1000);
```
