
#include "worker_thread.hxx"

#include <iostream>
#include <chrono>

int main() {

    auto t = std::make_shared<WorkerThread>();

    using namespace std::chrono_literals;
    for (int i = 0; i < 10; ++i) {
        t->PushTimer(
            10 * i,
            10,
            [](int x){
                std::cout << "delayed timer: " << x << std::endl;
            },
            i
        );
    }
    std::this_thread::sleep_for(1ms);
    for (int i = 0; i < 100; ++i) {
        t->PushTask(
            [](int x){
                std::cout << x << std::endl;
            },
            i
        );
    }
    std::this_thread::sleep_for(1s);
    t->Join();

    return 0;
}

