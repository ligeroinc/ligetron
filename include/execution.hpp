#pragma once

#include <runtime.hpp>
#include <instruction.hpp>
#include <stack>
#include <exception>

namespace ligero::vm {

struct execution_context {
    execution_context() = default;
    
    const store_t *store_ = nullptr;
    module_instance *module_ = nullptr;
    std::stack<value_t> stack_;
};

struct wasm_trap : std::runtime_error {
    using std::runtime_error::runtime_error;
}

struct basic_exe_control : virtual execution_context, virtual ControlExecutor {
    basic_exe_control() = default;

    void run(const op::nop&) override {
        // Do nothing
    }
    
    void run(const op::unreachable&) override {
        throw wasm_trap("Unreachable");
    }

    void run(const op::block& block) override {
        
    }
};


}  // namespace ligero::vm
