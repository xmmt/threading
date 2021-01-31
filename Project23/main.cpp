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
    std::atomic_int counter{ 0 };
    threading::threadpool tp(80);
    for (int i = 0; i < 120; ++i) {
        tp.submit([i, &counter] {
            for (int j = 0; j < 1000; ++j) {
                Writer() << i << ' ' << j << '\n';
                counter++;
            }
        });
    }
    //tp.no_more_tasks();
    tp.join();
    Writer() << counter << '\n';
    return 0;
}