
#include "worker_thread.hxx"
#include <ev.h>

WorkerThread::WorkerThread()
    : loop_(::ev_loop_new(0)),
      async_(std::make_unique<struct ev_async>()),
      running_(true) {
        ::ev_set_userdata(loop_, this);
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
    std::deque<Task> burst_task;
    std::list<std::shared_ptr<Timer>> pending_timer;
    while (true) {
        {
            std::lock_guard<std::mutex> _{lock_};
            if (!running_) {
                ::ev_async_stop(loop_, async_.get());
            }
            if (tasks_.empty() && pending_timers_.empty()) {
                break;
            }
            burst_task.swap(tasks_);
            pending_timer.swap(pending_timers_);
        }
        while (!burst_task.empty()) {
            auto task = std::move(burst_task.front());
            burst_task.pop_front();
            task();
        }
        while (!pending_timer.empty()) {
            auto timer = pending_timer.front();
            pending_timer.pop_front();
            if (timer->action == Timer::ADD) {
                RegisterTimer(timer);
            } else {
                UnregisterTimer(timer);
            }
        }
    }
}

void WorkerThread::Join() {
    {
        std::lock_guard<std::mutex> _{lock_};
        running_ = false;
        pending_timers_.remove_if(
            [](auto timer) -> bool {
                return timer->repeat;
            }
        );
        for (auto timer : timers_) {
            if (timer->repeat) {
                timer->action = Timer::DELETE;
                pending_timers_.emplace_back(timer);
            }
        }
    }
    if (thread_) {
        NotifyLoop();
        thread_->join();
        thread_.reset();
    }
}

void WorkerThread::RegisterTimer(std::shared_ptr<Timer> timer) {
    timers_.emplace(timer);

    timer->action = Timer::NONE;
    timer->Start();
}

void WorkerThread::UnregisterTimer(std::shared_ptr<Timer> timer) {
    auto itr = timers_.find(timer);
    if (itr != timers_.end()) {
        timers_.erase(itr);
    }
    timer->action = Timer::NONE;
    timer->Stop();
}

WorkerThread::Timer::Timer(long delay, long repeat, struct ev_loop *loop)
    : delay(delay), repeat(repeat), loop(loop), action(NONE) {
        timer = std::make_unique<struct ev_timer>().release();
        timer->data = this;
    }

WorkerThread::Timer::~Timer() {
    std::unique_ptr<struct ev_timer> p{timer};
    Stop();
    p.reset();
}

void WorkerThread::Timer::Start() {
    if (repeat) {
        ev_timer_init(
            timer,
            [](EV_P_ struct ev_timer *w, int revents) {
                auto *p = reinterpret_cast<Timer *>(w->data);
                p->Invoke();
            },
            static_cast<double>(delay) / 1000.,
            static_cast<double>(repeat) / 1000.
        );
    } else {
        ev_timer_init(
            timer,
            [](EV_P_ struct ev_timer *w, int revents) {
                auto *p = reinterpret_cast<Timer *>(w->data);
                p->InvokeOnce();
            },
            static_cast<double>(delay) / 1000.,
            0.
        );
    }
    ::ev_timer_start(loop, timer);
}

void WorkerThread::Timer::Stop() {
    ::ev_timer_stop(loop, timer);
}

void WorkerThread::Timer::Invoke() {
    auto *th = reinterpret_cast<WorkerThread *>(::ev_userdata(loop));
    task();
    task.reset();
}

void WorkerThread::Timer::InvokeOnce() {
    auto self{shared_from_this()};
    auto *th = reinterpret_cast<WorkerThread *>(::ev_userdata(loop));
    self->task();
    th->UnregisterTimer(self);
}
