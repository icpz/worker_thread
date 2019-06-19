
#include "worker_thread.hxx"

#include <iostream>
#include <chrono>

int main() {

    auto t = std::make_shared<WorkerThread>();

    using namespace std::chrono_literals;
    for (int i = 0; i < 1000; ++i) {
        t->PushTask([](int x){ std::this_thread::sleep_for(20ms); std::cout << x << std::endl; }, i);
    }
    t->Join();

    return 0;
}

