#ifndef UVCO_INCLUDE_H
#define UVCO_INCLUDE_H

#include <uv.h>
#include <coroutine>
#include <memory>

#include <deque>

// TODO comment

namespace uvco {

class scheduler;
using scheduler_ptr = std::shared_ptr<scheduler>;

class exception : public std::exception {
public:
    exception(std::string what) : what_(std::move(what)) {}
    exception(const std::string &where, int rc)
        : what_(where + ": " + uv_strerror(rc)) {}

    const char *what() const noexcept override { return what_.c_str(); }

protected:
    std::string what_;
};

template <typename T>
class task;

namespace impl {

class task_promise_base {
public:
    constexpr std::suspend_never initial_suspend() noexcept { return {}; }

    constexpr std::suspend_always final_suspend() noexcept { return {}; }

    void unhandled_exception() { exception_ = std::current_exception(); }

protected:
    std::exception_ptr exception_;
};

template <typename T>
class task_promise : public task_promise_base {
    using coroutine_handle_t = std::coroutine_handle<task_promise<T>>;

public:
    auto get_return_object() noexcept {
        return coroutine_handle_t::from_promise(*this);
    }

    void return_value(T &&result) noexcept { result_ = std::move(result); }

    T &&result() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
        return std::move(result_);
    }

    T result_;
};

template <>
class task_promise<void> : public task_promise_base {
    using coroutine_handle_t = std::coroutine_handle<task_promise<void>>;

public:
    auto get_return_object() noexcept {
        return coroutine_handle_t::from_promise(*this);
    }

    void return_void() {}

    void result() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }
};

} // namespace impl

template <typename T = void>
class [[nodiscard]] task {
public:
    using promise_type = impl::task_promise<T>;
    using coroutine_handle_t = std::coroutine_handle<promise_type>;

    task(coroutine_handle_t coh) noexcept : coh_(coh) {}

    task(const task &) = delete;

    task(task &&rhs) noexcept : coh_(rhs.coh_) { rhs.coh_ = {}; }

    task &operator=(const task &rhs) = delete;

    task &operator=(task &&rhs) {
        if (this != &rhs) {
            if (coh_) {
                coh_.destroy();
            }
            coh_ = rhs.coh_;
            rhs.coh_ = {};
        }
        return *this;
    }

    ~task() {
        if (coh_) {
            coh_.destroy();
        }
    }

    decltype(auto) result() { return coh_.promise().result(); }

private:
    std::coroutine_handle<promise_type> coh_;
};

class scheduler {
public:
    static scheduler_ptr get_default() {
        static scheduler_ptr inst(new scheduler(uv_default_loop()));
        return inst;
    }

    static scheduler_ptr create() {
        auto *lp = new uv_loop_t;
        uv_loop_init(lp);
        auto loop = LoopPtr{lp, [](uv_loop_t *l) {
                                uv_loop_close(l);
                                delete l;
                            }};
        return scheduler_ptr{new scheduler{std::move(loop)}};
    }

    template <typename Task>
    decltype(auto) schedule(Task &&task) {
        uv_run(loop(), UV_RUN_DEFAULT);
        return task.result();
    }

private:
    friend class component;

    using LoopDeleter = void (*)(uv_loop_t *);
    using LoopPtr = std::unique_ptr<uv_loop_t, LoopDeleter>;

    scheduler(uv_loop_t *loop) : loop_(loop, [](auto *) {}) {}
    scheduler(LoopPtr loop) : loop_(std::move(loop)) {}

    uv_loop_t *loop() const { return loop_.get(); }

    LoopPtr loop_;
};

class component {
public:
    component() : sched(scheduler::get_default()) {}
    component(const scheduler_ptr &scheduler) : sched(scheduler) {}

    component(const component &) = delete;
    component(component &&rhs) : sched(std::move(rhs.sched)) {}

    component &operator=(const component &) = delete;
    component &operator=(component &&rhs) {
        if (this != &rhs) {
            sched = std::move(rhs.sched);
        }
        return *this;
    }

    const scheduler_ptr &scheduler() { return sched; }

protected:
    uv_loop_t *loop() const { return sched->loop(); }

    scheduler_ptr sched;
};

class fs : public component {
public:
    using component::component;

    fs(fs &&rhs) : component(std::move(rhs)), file(rhs.file) { rhs.file = -1; }

    fs &operator=(fs &&rhs) {
        if (this != &rhs) {
            component::operator=(std::move(rhs));
            if (file != -1) {
                uv_fs_t req;
                uv_fs_close(loop(), &req, file, NULL);
            }
            file = rhs.file;
            rhs.file = -1;
        }
        return *this;
    }

    struct file_op;
    struct open_op;
    struct read_op;
    struct write_op;
    struct close_op;

    open_op open(const char *path, int flags, int mode = 0644) {
        return {this, path, flags, mode};
    }

    read_op read(char *data, size_t size, int64_t offset = -1) {
        uv_buf_t buf = uv_buf_init(data, size);
        return {this, buf, offset};
    }

    write_op write(const char *data, size_t size, int64_t offset = -1) {
        uv_buf_t buf = uv_buf_init((char *)data, size);
        return {this, buf, offset};
    }

    close_op close() { return {this}; }

    struct file_op {
    protected:
        ~file_op() {
            if (req) {
                uv_fs_req_cleanup(req);
                delete req;
            }
        }

        uv_fs_t *req{nullptr};
    };

    struct open_op : public file_op {
        open_op(fs *f, const char *path, int flags, int mode)
            : f(f), path(path), flags(flags), mode(mode) {}

        constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> coh) noexcept {
            this->coh = coh;
            req = new uv_fs_t;
            req->data = this;
            uv_fs_open(f->loop(), req, path, flags, mode, open_cb);
        }

        void await_resume() const {
            if (req->result < 0) {
                throw exception("fs::open", req->result);
            }
        }

    private:
        static void open_cb(uv_fs_t *req) {
            auto *self = (open_op *)req->data;
            fs *f = self->f;
            f->file = self->req->result;
            self->coh.resume();
        }

        fs *f;
        const char *path;
        int flags;
        int mode;

        std::coroutine_handle<> coh;
    };

    struct read_op : public file_op {
        read_op(fs *f, uv_buf_t buf, int64_t offset)
            : f(f), buf(buf), offset(offset) {}

        constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> coh) noexcept {
            this->coh = coh;
            req = new uv_fs_t;
            req->data = this;
            uv_fs_read(f->loop(), req, f->file, &buf, 1, offset, read_cb);
        }

        ssize_t await_resume() const noexcept { return req->result; }

    private:
        static void read_cb(uv_fs_t *req) {
            read_op *self = (read_op *)req->data;
            self->coh.resume();
        }

        fs *f;
        uv_buf_t buf;
        int64_t offset;
        std::coroutine_handle<> coh;
    };

    struct write_op : public file_op {
        write_op(fs *f, uv_buf_t buf, int64_t offset)
            : f(f), buf(buf), offset(offset) {}

        constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> coh) noexcept {
            this->coh = coh;
            req = new uv_fs_t;
            req->data = this;
            uv_fs_write(f->loop(), req, f->file, &buf, 1, offset, write_cb);
        }

        ssize_t await_resume() const noexcept { return req->result; }

    private:
        static void write_cb(uv_fs_t *req) {
            write_op *self = (write_op *)(req->data);
            self->coh.resume();
        }

        fs *f;
        uv_buf_t buf;
        int64_t offset;
        std::coroutine_handle<> coh;
    };

    struct close_op : public file_op {
        close_op(fs *f) : f(f) {}

        constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> coh) noexcept {
            this->coh = coh;
            req = new uv_fs_t;
            req->data = this;
            uv_fs_close(f->loop(), req, f->file, close_cb);
        }

        void await_resume() const noexcept {}

    private:
        static void close_cb(uv_fs_t *req) {
            close_op *self = (close_op *)req->data;
            self->f->file = -1;
            self->coh.resume();
        }

        fs *f;
        std::coroutine_handle<> coh;
    };

    ~fs() {
        if (file != -1) {
            uv_fs_t req;
            uv_fs_close(loop(), &req, file, NULL);
        }
    }

private:
    uv_file file{-1};
};

template <typename U>
class handle : public component {
public:
    handle() : pointer(nullptr) {}

    handle(U *pointer) : pointer(pointer) {}

    handle(const scheduler_ptr &sched) : component(sched), pointer(nullptr) {}

    handle(const scheduler_ptr &sched, U *pointer)
        : component(sched), pointer(pointer) {
        if (pointer) {
            pointer->data = this;
        }
    }

    handle(handle &&rhs) : component(std::move(rhs)), pointer(rhs.pointer) {
        if (pointer) {
            pointer->data = this;
        }
        rhs.pointer = nullptr;
    }

    handle &operator=(handle &&rhs) {
        if (this != &rhs) {
            component::operator=(std::move(rhs));
            if (pointer) {
                uv_close((uv_handle_t *)pointer, NULL);
                delete pointer;
            }
            pointer = rhs.pointer;
            if (pointer) {
                pointer->data = this;
            }
            rhs.pointer = nullptr;
        }
        return *this;
    }

protected:
    virtual ~handle() {
        if (pointer) {
            uv_close((uv_handle_t *)pointer, NULL);
            delete pointer;
        }
    }

    U *get() const { return pointer; }

private:
    U *pointer;
};

template <typename T, typename U>
class stream : public handle<U> {
public:
    using Base = handle<U>;
    using Base::Base;
    using ReadData = std::tuple<ssize_t, std::unique_ptr<char[]>>;

    struct connect_op;
    struct accept_op;
    struct read_op;
    struct write_op;

    accept_op accept(T *client) { return {this, client}; }

    read_op read() { return {this}; }

    write_op write(const char *data, size_t size) { return {this, data, size}; }

    struct connect_op {
        connect_op(T *socket) : socket(socket) {}

        constexpr bool await_ready() const noexcept { return false; }

        int await_resume() const noexcept { return status; }

        static void connect_cb(uv_connect_t *req, int status) {
            auto *self = (connect_op *)(req->data);
            self->status = status;
            self->coh.resume();
        }

    protected:
        ~connect_op() {
            if (req) delete req;
        }

        T *socket;
        uv_connect_t *req{nullptr};
        int status;
        std::coroutine_handle<> coh;
    };

    struct accept_op {
        accept_op(stream *server, T *client) : server(server), client(client) {}

        bool await_ready() const noexcept {
            if (server->pendingConns) {
                --server->pendingConns;
                return true;
            }
            return false;
        }

        void await_suspend(std::coroutine_handle<> coh) noexcept {
            this->coh = coh;
            server->waitingAcceptors.push_back(this);
        }

        int await_resume() noexcept {
            return uv_accept((uv_stream_t *)server->get(),
                             (uv_stream_t *)client->get());
        }

    private:
        friend class stream;

        void resume() { coh.resume(); }

        stream *server;
        T *client;
        std::coroutine_handle<> coh;
    };

    struct read_op {
        read_op(stream *socket) : socket(socket) {}

        bool await_ready() noexcept {
            if (!socket->pendingData.empty()) {
                data = std::move(socket->pendingData.front());
                socket->pendingData.pop_front();
                return true;
            }
            return false;
        }

        void await_suspend(std::coroutine_handle<> coh) noexcept {
            this->coh = coh;
            socket->startRead();
            socket->waitingReaders.push_back(this);
        }

        ReadData await_resume() noexcept { return std::move(data); }

    private:
        friend class stream;

        void resume(ReadData data) {
            this->data = std::move(data);
            coh.resume();
        }

        stream *socket;
        ReadData data;
        std::coroutine_handle<> coh;
    };

    struct write_op {
        write_op(stream *socket, const char *data, size_t size)
            : socket(socket), data(data), size(size) {}

        ~write_op() {
            if (req) delete req;
        }

        constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> coh) noexcept {
            this->coh = coh;
            req = new uv_write_t;
            req->data = this;
            auto buf = uv_buf_init((char *)data, size);
            uv_write(req, (uv_stream_t *)socket->get(), &buf, 1, write_cb);
        }

        int await_resume() const noexcept { return status; }

        static void write_cb(uv_write_t *req, int status) {
            auto *self = (write_op *)(req->data);
            self->status = status;
            self->coh.resume();
        }

    private:
        stream *socket;
        const char *data;
        size_t size;
        uv_write_t *req{nullptr};
        int status;
        std::coroutine_handle<> coh;
    };

    void startRead() {
        uv_read_start((uv_stream_t *)(this->get()), alloc_cb, read_cb);
    }

    void stopRead() { uv_read_stop((uv_stream_t *)(this->get())); }

protected:
    void startListen(int backlog) {
        int rc = uv_listen((uv_stream_t *)(this->get()),
                           backlog,
                           client_connection_cb);
        if (rc != 0) {
            throw exception("stream::listen", rc);
        }
    }

private:
    void client_connection_cb([[maybe_unused]] int status) {
        if (!waitingAcceptors.empty()) {
            auto *acceptor = waitingAcceptors.front();
            waitingAcceptors.pop_front();
            acceptor->resume();
        } else {
            pendingConns++;
        }
    }

    void alloc_cb(size_t suggested_size, uv_buf_t *buf) {
        *buf = uv_buf_init(new char[suggested_size], suggested_size);
    }

    void read_cb(ssize_t nread, const uv_buf_t *buf) {
        if (nread != 0) {
            auto data = std::tuple(nread, std::unique_ptr<char[]>(buf->base));
            if (!waitingReaders.empty()) {
                auto *reader = waitingReaders.front();
                waitingReaders.pop_front();
                reader->resume(std::move(data));
            } else {
                pendingData.push_back(std::move(data));
            }
        }
    }

    bool readStarted{false};
    std::deque<ReadData> pendingData;
    std::deque<read_op *> waitingReaders;

    int pendingConns{0};
    std::deque<accept_op *> waitingAcceptors;

    static void client_connection_cb(uv_stream_t *server, int status) {
        auto *self = (stream *)(server->data);
        self->client_connection_cb(status);
    }

    static void alloc_cb(uv_handle_t *handle,
                         size_t suggested_size,
                         uv_buf_t *buf) {
        auto *self = (stream *)(handle->data);
        self->alloc_cb(suggested_size, buf);
    }

    static void read_cb(uv_stream_t *s, ssize_t nread, const uv_buf_t *buf) {
        auto *self = (stream *)(s->data);
        self->read_cb(nread, buf);
    }
};

class tcp : public stream<tcp, uv_tcp_t> {
public:
    static constexpr int DEFAULT_BACKLOG = 128;

    using Base = stream<tcp, uv_tcp_t>;

    tcp() : tcp(scheduler::get_default()) {}

    tcp(const scheduler_ptr &scheduler) : Base(scheduler, new uv_tcp_t) {
        uv_tcp_init(loop(), get());
    }

    void listen(const char *host, int port, int backlog = DEFAULT_BACKLOG) {
        struct sockaddr_in addr;
        uv_ip4_addr(host, port, &addr);
        uv_tcp_bind(get(), (const struct sockaddr *)&addr, 0);
        startListen(backlog);
    }

    struct tcp_connect_op;

    tcp_connect_op connect(const char *host, int port) {
        struct sockaddr addr;
        uv_ip4_addr(host, port, (struct sockaddr_in *)&addr);
        return {this, &addr};
    }

    struct tcp_connect_op : public connect_op {
        tcp_connect_op(tcp *socket, struct sockaddr *addr)
            : connect_op(socket), addr(*addr) {}

        void await_suspend(std::coroutine_handle<> coh) noexcept {
            this->coh = coh;
            req = new uv_connect_t;
            req->data = this;
            uv_tcp_connect(req, socket->get(), &addr, connect_cb);
        }

        struct sockaddr addr;
    };
};

class pipe : public stream<pipe, uv_pipe_t> {
public:
    static constexpr int DEFAULT_BACKLOG = 2;

    using Base = stream<pipe, uv_pipe_t>;

    struct pipe_connect_op;

    pipe(int ipc = 0) : pipe(scheduler::get_default(), ipc) {}

    pipe(const scheduler_ptr &scheduler, int ipc = 0)
        : Base(scheduler, new uv_pipe_t) {
        uv_pipe_init(loop(), get(), ipc);
    }

    void listen(const char *name, int backlog = DEFAULT_BACKLOG) {
        uv_pipe_bind(get(), name);
        startListen(backlog);
    }

    pipe_connect_op connect(const char *name) { return {this, name}; }

    struct pipe_connect_op : public connect_op {
        pipe_connect_op(pipe *socket, const char *name)
            : connect_op(socket), name(name) {}

        void await_suspend(std::coroutine_handle<> coh) noexcept {
            this->coh = coh;
            req = new uv_connect_t;
            req->data = this;
            uv_pipe_connect(req, socket->get(), name, connect_cb);
        }

        const char *name;
    };
};

class udp : public handle<uv_udp_t> {
public:
    using Base = handle<uv_udp_t>;
    using RecvData = std::
            tuple<ssize_t, std::unique_ptr<char[]>, struct sockaddr, unsigned>;

    struct recv_op;
    struct send_op;

    udp() : udp(scheduler::get_default()) {}

    udp(const scheduler_ptr &scheduler) : Base(scheduler, new uv_udp_t) {
        uv_udp_init(loop(), get());
    }

    void bind(const char *ip, int port) {
        struct sockaddr_in addr;
        uv_ip4_addr(ip, port, &addr);
        int rc = uv_udp_bind(get(), (const struct sockaddr *)&addr, 0);
        if (rc != 0) {
            throw exception("udp::bind", rc);
        }
    }

    recv_op recv() { return {this}; }

    send_op send(const struct sockaddr *addr, const char *data, size_t size) {
        return {this, addr, data, size};
    }

    struct recv_op {
        recv_op(udp *socket) : socket(socket) {}

        bool await_ready() noexcept {
            if (!socket->pendingData.empty()) {
                data = std::move(socket->pendingData.front());
                socket->pendingData.pop_front();
                return true;
            }
            return false;
        }

        void await_suspend(std::coroutine_handle<> coh) noexcept {
            this->coh = coh;
            socket->startRecv();
            socket->waitingReaders.push_back(this);
        }

        RecvData await_resume() noexcept { return std::move(data); }

    private:
        friend class udp;

        void resume(RecvData data) {
            this->data = std::move(data);
            coh.resume();
        }

        udp *socket;
        RecvData data;
        std::coroutine_handle<> coh;
    };

    struct send_op {
        send_op(udp *socket,
                const struct sockaddr *addr,
                const char *data,
                size_t size)
            : socket(socket), addr(addr), data(data), size(size) {}

        ~send_op() {
            if (req) delete req;
        }

        constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> coh) noexcept {
            this->coh = coh;
            req = new uv_udp_send_t;
            req->data = this;
            auto buf = uv_buf_init((char *)data, size);
            uv_udp_send(req, socket->get(), &buf, 1, addr, send_cb);
        }

        int await_resume() const noexcept { return status; }

        static void send_cb(uv_udp_send_t *req, int status) {
            auto *self = (send_op *)(req->data);
            self->status = status;
            self->coh.resume();
        }

    private:
        udp *socket;
        const struct sockaddr *addr;
        const char *data;
        size_t size;
        uv_udp_send_t *req{nullptr};
        int status;
        std::coroutine_handle<> coh;
    };

    void startRecv() {
        if (!recvStarted) {
            recvStarted = true;
            uv_udp_recv_start(get(), alloc_cb, recv_cb);
        }
    }

    void stopRecv() {
        if (recvStarted) {
            recvStarted = false;
            uv_udp_recv_stop(get());
        }
    }

private:
    void alloc_cb(size_t suggested_size, uv_buf_t *buf) {
        *buf = uv_buf_init(new char[suggested_size], suggested_size);
    }

    void recv_cb(ssize_t nread,
                 const uv_buf_t *buf,
                 const struct sockaddr *addr,
                 unsigned flags) {
        auto data = std::tuple(
                nread,
                std::unique_ptr<char[]>(nread > 0 ? buf->base : nullptr),
                *addr,
                flags);
        if (!waitingReaders.empty()) {
            auto *reader = waitingReaders.front();
            waitingReaders.pop_front();
            reader->resume(std::move(data));
        } else {
            pendingData.push_back(std::move(data));
        }
    }

    static void alloc_cb(uv_handle_t *handle,
                         size_t suggested_size,
                         uv_buf_t *buf) {
        auto *self = (udp *)(handle->data);
        self->alloc_cb(suggested_size, buf);
    }

    static void recv_cb(uv_udp_t *handle,
                        ssize_t nread,
                        const uv_buf_t *buf,
                        const struct sockaddr *addr,
                        unsigned flags) {
        auto *self = (udp *)(handle->data);
        self->recv_cb(nread, buf, addr, flags);
    }

    bool recvStarted{false};
    std::deque<RecvData> pendingData;
    std::deque<recv_op *> waitingReaders;
};

namespace impl {
struct sleep_op : public component {
    sleep_op(int64_t delay) : delay(delay) {}
    sleep_op(const scheduler_ptr &sched, int64_t delay)
        : component(sched), delay(delay) {}

    constexpr bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> coh) noexcept {
        this->coh = coh;
        uv_timer_init(loop(), &timer);
        timer.data = this;
        uv_timer_start(&timer, timer_cb, delay, 0);
    }

    void await_resume() const noexcept {}

    static void timer_cb(uv_timer_t *handle) {
        auto *self = (sleep_op *)(handle->data);
        self->coh.resume();
    }

private:
    int64_t delay;
    uv_timer_t timer;
    std::coroutine_handle<> coh;
};
} // namespace impl

inline impl::sleep_op sleep(int64_t ms) {
    return {ms};
}

inline impl::sleep_op sleep(const scheduler_ptr &sched, int64_t ms) {
    return {sched, ms};
}

} // namespace uvco

#endif // UVCO_INCLUDE_H
