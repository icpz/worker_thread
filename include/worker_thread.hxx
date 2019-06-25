#ifndef __WORKER_THREAD_HXX__
#define __WORKER_THREAD_HXX__

#include <thread>
#include <memory>
#include <functional>
#include <deque>
#include <future>
#include <unordered_set>
#include <list>

struct ev_loop;
struct ev_async;
struct ev_timer;

class WorkerThread {
    using Task = std::packaged_task<void(void)>;
public:
    WorkerThread();
    ~WorkerThread();

    template<class F, class... Args>
    decltype(auto) PushTask(F&& f, Args&&... args) {
        using result_type = std::invoke_result_t<F, Args...>;
        std::packaged_task<result_type(void)>
            task{std::bind(std::forward<F>(f), std::forward<Args>(args)...)};

        auto result = task.get_future();
        {
            std::lock_guard<std::mutex> _{lock_};
            if (running_) {
                tasks_.emplace_back(std::move(task));
            }
        }
        NotifyLoop();

        return result;
    }

    template<class F, class... Args>
    decltype(auto) PushTimer(long delay, long repeat, F&& f, Args&&... args) {
        using result_type = std::invoke_result_t<F, Args...>;
        std::packaged_task<result_type(void)>
            task{std::bind(std::forward<F>(f), std::forward<Args>(args)...)};

        auto result = task.get_future();
        auto timer = std::make_shared<Timer>(delay, repeat, loop_);

        timer->task = std::move(task);
        timer->action = Timer::ADD;
        {
            std::lock_guard<std::mutex> _{lock_};
            if (running_) {
                pending_timers_.emplace_back(timer);
            }
        }
        NotifyLoop();

        return result;
    }

    void Join();

private:

    struct Timer : public std::enable_shared_from_this<Timer> {
        enum { ADD, DELETE, NONE };

        Timer(long delay, long repeat, struct ev_loop *loop);
        ~Timer();

        void Start();
        void Stop();

        void Invoke();
        void InvokeOnce();

        long delay;
        long repeat;
        struct ev_loop *loop;
        struct ev_timer *timer;
        int action;
        Task task;
    };
    friend struct Timer;

    void MainLoop();
    void NotifyLoop();
    void AsyncCb();
    void RegisterTimer(std::shared_ptr<Timer> timer);
    void UnregisterTimer(std::shared_ptr<Timer> timer);

    static void EvAsyncCallback(struct ev_loop *loop, struct ev_async *w, int revents);

    std::unique_ptr<std::thread> thread_;
    mutable std::mutex lock_;
    struct ev_loop *loop_;
    std::unique_ptr<struct ev_async> async_;
    std::deque<Task> tasks_;
    std::list<std::shared_ptr<Timer>> pending_timers_;
    std::unordered_set<std::shared_ptr<Timer>> timers_;
    bool running_;
};

#endif /* __WORKER_THREAD_HXX__ */
