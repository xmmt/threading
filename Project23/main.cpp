#include "threading.hpp"
#include <iostream>
#include <sstream>
#include <condition_variable>

class Writer {
    std::ostringstream buffer;

public:
    ~Writer() {
        std::cout << buffer.str();
    }
    template <class T>
    Writer& operator<<(T&& input) {
        buffer << input;
        return *this;
    }
};

int main() {
    using namespace std::chrono_literals;
    std::atomic_int counter{ 0 };
    threading::threadpool tp(20);

    std::packaged_task<int()> task{ [] {
        std::this_thread::sleep_for(5s);
        return 5;
    } };
    std::future<int> result = task.get_future();
    tp.submit(task);

    for (int i = 0; i < 5; ++i) {
        tp.submit([i, &counter] {
            for (int j = 0; j < 8; ++j) {
                Writer() << i << (i < 10 ? " " : "") << ' ' << j << '\n';
                counter++;
                std::this_thread::sleep_for(200ms);
            }
        });
    }
    Writer() << result.get() << '\n';
    std::this_thread::sleep_for(10s);
    Writer() << counter << '\n';
    for (int i = 0; i < 40; ++i) {
        tp.submit([i, &counter] {
            for (int j = 0; j < 8; ++j) {
                Writer() << i << (i < 10 ? " " : "") << ' ' << j << '\n';
                counter++;
                std::this_thread::sleep_for(200ms);
            }
        });
    }
    std::this_thread::sleep_for(10s);
    Writer() << counter << '\n';
    for (int i = 0; i < 40; ++i) {
        tp.submit([i, &counter] {
            for (int j = 0; j < 8; ++j) {
                Writer() << i << (i < 10 ? " " : "") << ' ' << j << '\n';
                counter++;
                std::this_thread::sleep_for(200ms);
            }
        });
    }
    //tp.no_more_tasks();
    tp.join();
    Writer() << counter << '\n';
    return 0;
}