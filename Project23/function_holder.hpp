#pragma once

#include <memory>

namespace utils {

template <typename Unused>
class function;

template <typename ReturnType, typename... Args>
class function<ReturnType(Args...)> {
private:
    class function_holder_base {
    public:
        function_holder_base() = default;
        function_holder_base(function_holder_base const&) = delete;
        function_holder_base(function_holder_base&&) = delete;
        void operator=(function_holder_base const&) = delete;
        void operator=(function_holder_base&&) = delete;
        virtual ~function_holder_base() = default;

        virtual ReturnType invoke(Args&&...) = 0;
    };
    using invoker_t = std::unique_ptr<function_holder_base>;

    template <typename F>
    class function_holder final : public function_holder_base {
    public:
        function_holder(F&& f)
            : function_holder_base()
            , function_(std::forward<F>(f)) {
        }
        void operator=(function_holder const&) = delete;
        void operator=(function_holder&&) = delete;
        ~function_holder() = default;

        ReturnType invoke(Args&&... args) final {
            return function_(std::forward<Args>(args)...);
        }

    private:
        F function_;
    };

    invoker_t invoker_;

public:
    function() = default;
    function(function const&) = delete;
    function(function&&) = default;
    function& operator=(function const&) = delete;
    function& operator=(function&&) = default;

    template <typename F>
    function(F const& f)
        : invoker_(std::make_unique<function_holder<F>>(f)) {
    }
    template <typename F>
    function(F&& f)
        : invoker_(std::make_unique<function_holder<F>>(std::forward<F>(f))) {
    }

    inline ReturnType operator()(Args... args) {
        return invoker_->invoke(std::forward<Args>(args)...);
    }
    inline operator bool() const {
        return invoker_.operator bool();
    }
};

} // namespace utils