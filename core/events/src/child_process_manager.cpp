#include <pulp/events/child_process_manager.hpp>
#include <algorithm>
#include <filesystem>
#include <random>
#include <chrono>

namespace pulp::events {

namespace {

constexpr auto kChildIpcConnectTimeout = std::chrono::seconds(5);

}  // namespace

// ── ConnectedChildProcess ───────────────────────────────────────────────

bool ConnectedChildProcess::join_monitor_thread(bool detach_current_thread) {
    std::lock_guard lock(monitor_mutex_);
    if (!monitor_thread_.joinable())
        return true;
    if (monitor_thread_.get_id() == std::this_thread::get_id()) {
        if (!detach_current_thread)
            return false;
        monitor_thread_.detach();
        return true;
    }
    monitor_thread_.join();
    return true;
}

ConnectedChildProcess::~ConnectedChildProcess() {
    kill();
    join_monitor_thread(true);
}

bool ConnectedChildProcess::launch(std::string_view executable,
                                    const std::vector<std::string>& extra_args) {
    if (running_.load()) kill();
    if (!join_monitor_thread(false))
        return false;

    {
        std::lock_guard lock(state_mutex_);
        cancel_requested_ = false;
        exit_ready_ = false;
        exit_code_ = -1;
    }
    pid_ = -1;

    // Generate unique pipe name
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    const auto suffix = std::to_string(dist(gen));
#ifdef _WIN32
    pipe_name_ = "pulp_ipc_" + suffix;
#else
    pipe_name_ = (std::filesystem::temp_directory_path() / ("pulp_ipc_" + suffix)).string();
#endif

    // Build args: executable + pipe_name + extra_args
    std::vector<std::string> args;
    args.push_back("--ipc-pipe");
    args.push_back(pipe_name_);
    args.insert(args.end(), extra_args.begin(), extra_args.end());

    // Create server pipe (blocks until child connects).
    // We need to do this in a thread because create_server blocks.
    std::atomic<bool> server_done{false};
    std::atomic<bool> server_ok{false};
    std::thread server_thread([this, &server_done, &server_ok]() {
        connection_.on_text_message = [this](std::string_view msg) {
            if (on_message) on_message(msg);
        };
        server_ok.store(connection_.create_server(pipe_name_, IpcTransport::NamedPipe));
        server_done.store(true);
    });

#ifdef _WIN32
    // Wait briefly for CreateNamedPipe to publish the endpoint.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
#else
    const auto reply_pipe = pipe_name_ + ".reply";
    const auto listen_deadline = std::chrono::steady_clock::now() +
                                 std::chrono::seconds(2);
    while (!server_done.load() &&
           (!std::filesystem::exists(pipe_name_) ||
            !std::filesystem::exists(reply_pipe)) &&
           std::chrono::steady_clock::now() < listen_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!server_done.load() &&
        (!std::filesystem::exists(pipe_name_) ||
         !std::filesystem::exists(reply_pipe))) {
        connection_.disconnect();
        server_thread.join();
        return false;
    }
#endif
    if (server_done.load() && !server_ok.load()) {
        server_thread.join();
        return false;
    }

    // Launch the child process
    pulp::platform::ProcessOptions options;
    options.capture_stdout = false;
    options.capture_stderr = false;
    if (!process_.start(std::string(executable), args, options)) {
        connection_.disconnect();
        server_thread.join();
        return false;
    }

    pid_ = process_.process_id();
    const auto connect_started = std::chrono::steady_clock::now();
    while (!server_done.load()) {
        if (!process_.is_running()) {
            connection_.disconnect();
            break;
        }

        const auto elapsed = std::chrono::steady_clock::now() - connect_started;
        if (elapsed >= kChildIpcConnectTimeout) {
            connection_.disconnect();
            if (process_.is_running())
                process_.cancel();
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    server_thread.join();
    if (!server_ok.load()) {
        if (process_.is_running())
            process_.cancel();
        else
            (void)process_.wait();
        pid_ = -1;
        return false;
    }

    running_.store(true);

    // Monitor thread — watches for child exit
    monitor_thread_ = std::thread([this]() {
        while (true) {
            {
                std::unique_lock lock(state_mutex_);
                if (cancel_requested_) break;
                exit_cv_.wait_for(lock, std::chrono::milliseconds(50), [this] {
                    return cancel_requested_;
                });
                if (cancel_requested_) break;
            }

            if (!process_.is_running()) break;
        }

        bool should_cancel = false;
        {
            std::lock_guard lock(state_mutex_);
            should_cancel = cancel_requested_;
        }
        if (should_cancel && process_.is_running())
            process_.cancel();

        auto result = process_.wait();
        connection_.disconnect();

        std::function<void(int)> exit_callback;
        {
            std::lock_guard lock(state_mutex_);
            exit_code_ = result.exit_code;
            exit_ready_ = true;
            running_.store(false);
            exit_callback = on_exit;
        }
        exit_cv_.notify_all();

        if (exit_callback) exit_callback(result.exit_code);
    });

    return true;
}

bool ConnectedChildProcess::send_message(std::string_view message) {
    return connection_.send_message(message);
}

void ConnectedChildProcess::kill() {
    if (pid_ < 0) return;

    {
        std::lock_guard lock(state_mutex_);
        if (exit_ready_) return;
        cancel_requested_ = true;
    }

    exit_cv_.notify_all();

    join_monitor_thread(true);
}

int ConnectedChildProcess::wait_for_exit(int timeout_ms) {
    if (pid_ < 0) return -1;

    int code = -1;
    {
        std::unique_lock lock(state_mutex_);
        if (!exit_ready_) {
            if (timeout_ms > 0) {
                if (!exit_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
                    return exit_ready_;
                })) {
                    return -1;
                }
            } else {
                exit_cv_.wait(lock, [this] { return exit_ready_; });
            }
        }
        code = exit_code_;
    }

    join_monitor_thread(true);

    return code;
}

// ── ChildProcessManager ─────────────────────────────────────────────────

ChildProcessManager::~ChildProcessManager() {
    kill_all();
}

ConnectedChildProcess* ChildProcessManager::launch(
    std::string_view executable, const std::vector<std::string>& args) {
    auto child = std::make_shared<ConnectedChildProcess>();
    std::weak_ptr<ConnectedChildProcess> weak = child;
    child->on_exit = [this, weak](int code) {
        auto hold = weak.lock();
        if (hold && on_child_exit) on_child_exit(hold.get(), code);
    };

    if (!child->launch(executable, args))
        return nullptr;

    std::lock_guard<std::mutex> lock(mutex_);
    children_.push_back(std::move(child));
    return children_.back().get();
}

int ChildProcessManager::active_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (auto& c : children_)
        if (c->is_running()) ++count;
    return count;
}

void ChildProcessManager::kill_all() {
    std::vector<std::shared_ptr<ConnectedChildProcess>> children;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        children = children_;
    }
    for (auto& c : children)
        c->kill();
}

void ChildProcessManager::wait_all(int timeout_ms) {
    std::vector<std::shared_ptr<ConnectedChildProcess>> children;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        children = children_;
    }
    for (auto& c : children)
        c->wait_for_exit(timeout_ms);
}

void ChildProcessManager::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    children_.erase(
        std::remove_if(children_.begin(), children_.end(),
                      [](auto& c) { return !c->is_running(); }),
        children_.end());
}

}  // namespace pulp::events
