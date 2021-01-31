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
    // A worker thread
    // It will wait until it is requested to stop.
    threading::jthread worker([](threading::stop_token stoken) {
        Writer() << "Worker thread's id: " << std::this_thread::get_id() << '\n';
        std::mutex mutex;
        std::unique_lock lock(mutex);
        std::condition_variable_any cv;
        condition_variable_any_wait_stop(cv, lock, stoken,
          [&stoken] { return stoken.stop_requested(); });
    });

    // Register a stop callback on the worker thread.
    threading::stop_callback callback(worker.get_stop_token(), [] {
        Writer() << "Stop callback executed by thread: "
                 << std::this_thread::get_id() << '\n';
    });

    // stop_callback objects can be destroyed prematurely to prevent execution
    {
        threading::stop_callback scoped_callback(worker.get_stop_token(), [] {
            // This will not be executed.
            Writer() << "Scoped stop callback executed by thread: "
                     << std::this_thread::get_id() << '\n';
        });
    }

    // Demonstrate which thread executes the stop_callback and when.
    // Define a stopper function
    auto stopper_func = [&worker] {
        if (worker.request_stop())
            Writer() << "Stop request executed by thread: "
                     << std::this_thread::get_id() << '\n';
        else
            Writer() << "Stop request not executed by thread: "
                     << std::this_thread::get_id() << '\n';
    };

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(5ms);
    // Let multiple threads compete for stopping the worker thread
    threading::jthread stopper1(stopper_func);
    threading::jthread stopper2(stopper_func);
    stopper1.join();
    stopper2.join();

    // After a stop has already been requested,
    // a new stop_callback executes immediately.
    Writer() << "Main thread: " << std::this_thread::get_id() << '\n';
    threading::stop_callback callback_after_stop(worker.get_stop_token(), [] {
        Writer() << "Stop callback executed by thread: "
                 << std::this_thread::get_id() << '\n';
    });

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