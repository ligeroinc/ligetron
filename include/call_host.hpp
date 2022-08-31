#pragma once

#include <runtime.hpp>

namespace ligero::vm {

template <typename Derive>
struct extend_call_host {
    extend_call_host() : derive_(static_cast<Derive&>(*this)) { }
    virtual ~extend_call_host() = default;

    result_t call_host(const import_name_t& name, const std::vector<value_t>& args) {
        if (name.second == "get_witness_size") {
            return derive_.builtin_get_witness_size();
        }
        else if (name.second == "get_witness") {
            derive_.show_stack();
            auto a = derive_.builtin_get_witness(args);
            derive_.show_stack();
            return a;
        }
        else {
            undefined("Undefined host function");
            return {};
        }
    }

protected:
    result_t builtin_get_witness_size() {
        u32 size = derive_.args().size() / sizeof(u32);
        derive_.stack_push(size);
        return {};
    }

    result_t builtin_get_witness(const std::vector<value_t>& args) {
        assert(args.size() == 1);
        u32 index = std::get<u32>(args[0]);
        u32 val = reinterpret_cast<const u32*>(derive_.args().data())[index];
        derive_.stack_push(val);
        return {};
    }

protected:
    Derive& derive_;
};

}  // namespace ligero::vm

