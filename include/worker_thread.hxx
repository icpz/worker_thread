#ifndef __WORKER_THREAD_HXX__
#define __WORKER_THREAD_HXX__

#include <thread>
#include <memory>
#include <functional>
#include <deque>
#include <future>

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

    void Join();

private:

    void MainLoop();
    void NotifyLoop();
    void AsyncCb();

    static void EvAsyncCallback(struct ev_loop *loop, struct ev_async *w, int revents);

    std::unique_ptr<std::thread> thread_;
    mutable std::mutex lock_;
    struct ev_loop *loop_;
    std::unique_ptr<struct ev_async> async_;
    std::deque<Task> tasks_;
    bool running_;
};

#endif /* __WORKER_THREAD_HXX__ */
