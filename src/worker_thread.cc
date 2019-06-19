
#include "worker_thread.hxx"
#include <ev.h>

WorkerThread::WorkerThread()
    : loop_(::ev_loop_new(0)),
      async_(std::make_unique<struct ev_async>()),
      running_(true) {
        ev_async_init(
            async_.get(),
            [](EV_P_ struct ev_async *w, int revents) {
                auto *wt = reinterpret_cast<WorkerThread *>(w->data);
                wt->AsyncCb();
            }
        );
        ::ev_async_start(loop_, async_.get());
        async_->data = this;
        thread_ = std::make_unique<std::thread>(&WorkerThread::MainLoop, this);
        NotifyLoop();
    }

WorkerThread::~WorkerThread() {
    Join();
    ::ev_loop_destroy(loop_);
    loop_ = nullptr;
}

void WorkerThread::MainLoop() {
    ::ev_run(loop_, 0);
}

void WorkerThread::NotifyLoop() {
    ::ev_async_send(loop_, async_.get());
}

void WorkerThread::AsyncCb() {
    std::deque<Task> burst_;
    while (true) {
        {
            std::lock_guard<std::mutex> _{lock_};
            if (!running_) {
                ::ev_async_stop(loop_, async_.get());
            }
            if (tasks_.empty()) {
                break;
            }
            burst_.swap(tasks_);
        }
        while (!burst_.empty()) {
            auto task = std::move(burst_.front());
            burst_.pop_front();
            task();
        }
    }
}

void WorkerThread::Join() {
    {
        std::lock_guard<std::mutex> _{lock_};
        running_ = false;
    }
    if (thread_) {
        NotifyLoop();
        thread_->join();
        thread_.reset();
    }
}

