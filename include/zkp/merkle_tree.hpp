#pragma once

#include <bit>
// #include <relation.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <zkp/hash.hpp>

namespace ligero::vm::zkp {

template <typename Hash> requires IsHashScheme<Hash>
struct merkle_tree {
    using hasher_type = Hash;
    using digest_type = typename Hash::digest;

    struct builder {
        explicit builder() : hashers_() { }
        explicit builder(size_t n) : hashers_(n) { }

        bool empty() const { return hashers_.empty(); }
        size_t size() const { return hashers_.size(); }
        hasher_type& operator[](size_t idx) { return hashers_[idx]; }

        builder& add_empty_node() { hashers_.emplace_back(); }

        template <typename T>
        builder& append(const T& val) {
            hasher_type hasher;
            hasher << val;
            hashers_.emplace_back(std::move(hasher));
            return *this;
        }

        template <typename T> requires requires(T t, size_t i) {
            { t.size() } -> std::convertible_to<size_t>;
            { t[i] };
        }
        builder& operator<<(const T& val) {
            assert(val.size() == hashers_.size());
            #pragma omp parallel for
            for (size_t i = 0; i < hashers_.size(); i++) {
                hashers_[i] << val[i];
            }
            return *this;
        }

        // template <typename T>
        // builder& operator<<(const relation<T> *r) {
        //     if (auto *p = dynamic_cast<const binary_relation<T>*>(r); p != nullptr) {
        //         return *this << p->a() << p->b() << p->c();
        //     }
        //     else {
        //         throw std::runtime_error("Unexpected relation type");
        //     }
        // }

        auto begin() { return hashers_.begin(); }
        auto end()   { return hashers_.end();   }
        const auto& data() const { return hashers_; }
    protected:
        std::vector<hasher_type> hashers_;
    };

    struct decommitment {
        decommitment() = default;
        decommitment(size_t count, const std::vector<size_t>& index)
            : total_count_(count), known_index_(index) { }

        void insert(size_t pos, digest_type node) {
            commitment_.emplace(pos, node);
        }

        size_t size() const { return total_count_; }
        size_t leaf_size() const { return total_count_ / 2 + 1; }

        const auto& known_index() const { return known_index_; }
        const auto& commitment() const { return commitment_; }
        
    protected:
        size_t total_count_;
        std::vector<size_t> known_index_;
        std::unordered_map<size_t, digest_type> commitment_;
    };

    explicit merkle_tree() : nodes_() { }
    explicit merkle_tree(size_t n) : nodes_(std::bit_ceil(n) * 2 - 1) { }
    merkle_tree(builder& b) : nodes_() { initialize_from_builder(std::move(b)); }
    merkle_tree(builder& b, const decommitment& d) : nodes_() { from_decommitment(b, d); }
    merkle_tree(const merkle_tree& o) : nodes_(o.nodes_) { }
    merkle_tree(merkle_tree&& o) : nodes_(std::move(o.nodes_)) { }

    merkle_tree& operator=(const merkle_tree& o) {
        nodes_ = o.nodes_;
        return *this;
    }

    merkle_tree& operator=(merkle_tree&& o) {
        nodes_ = std::move(o.nodes_);
        return *this;
    }

    merkle_tree& operator=(builder& b) {
        initialize_from_builder(std::move(b));
        return *this;
    }

    size_t parent_index(size_t curr) const { return curr ? (curr - 1) / 2 : curr; }
    
    size_t sibling_index(size_t curr) const {
        if (size_t parent = parent_index(curr); parent == curr) {
            return curr;
        }
        else {
            const size_t left = 2 * parent + 1, right = 2 * (parent + 1);
            return curr == left ? right : left;
        }
    }

    size_t size() const { return nodes_.size(); }
    size_t leaf_size() const { return size() / 2 + 1; }

    auto leaf_begin() { return nodes_.begin() + (nodes_.size() / 2); }
    auto leaf_end() { return nodes_.end(); }
    
    const digest_type& operator[](size_t n) const { return nodes_[n]; }
    const digest_type& root() const { return nodes_[0]; }
    const digest_type& node(size_t n) const { return nodes_[nodes_.size() / 2 + n]; }

    decommitment decommit(const std::vector<size_t>& known_index) {
        decommitment d{ nodes_.size(), known_index };
        const size_t child_begin = nodes_.size() / 2;
        const size_t child_end = nodes_.size();

        std::unordered_set<size_t> known(known_index.begin(), known_index.end());

        for (size_t i = child_begin; i < child_end; i += 2) {
            const size_t parent = parent_index(i);
            bool left_known = known.contains(i);
            bool right_known = known.contains(i + 1);

            if (!left_known) {
                d.insert(i, nodes_[i]);
            }
            
            if (!right_known) {
                d.insert(i + 1, nodes_[i + 1]);
            }
        }

        return d;
    }

private:
    void initialize_from_builder(builder&& b) {
        if (b.empty()) return;
        /* Make sure the input size is power of 2 */
        const size_t power_of_two = std::bit_ceil(b.size());
        /* Data node has length `power_of_two`, parent nodes have length `power_of_two - 1`,
         * total nodes are `power_of_two * 2 - 1`   */
        const size_t leaf_size = power_of_two;
        const size_t parent_size = power_of_two - 1;
        const size_t node_size = parent_size + leaf_size;
        nodes_.resize(node_size);

        std::transform(b.begin(), b.end(),
                       nodes_.begin() + parent_size,
                       [](auto& h) {
                           return h.flush_digest();
                       });

        if (size_t parent = parent_index(parent_size); parent != parent_size)
            build_tree(parent, parent_size);
    }

    void build_tree(size_t curr_start, size_t curr_end) {
        /* Recursively build each layer in the tree */
        for (size_t i = curr_start; i < curr_end; i++) {
            const size_t left_child = 2 * i + 1;
            const size_t right_child = left_child + 1;

            hasher_type hasher;
            hasher << nodes_[left_child] << nodes_[right_child];
            nodes_[i] = hasher.flush_digest();
        }

        if (curr_start > 0)
            build_tree(parent_index(curr_start), curr_start);
    }

    merkle_tree& from_decommitment(builder& b, const decommitment& d) {
        nodes_.clear();
        nodes_.resize(d.size());
        
        const auto& cm = d.commitment();
        size_t ki = 0;
        for (size_t i = d.size() / 2; i < d.size(); i++) {
            if (cm.contains(i)) {
                nodes_[i] = cm.at(i);
            }
            else {
                auto& hasher = b[ki++];
                nodes_[i] = hasher.flush_digest();
            }
        }

        build_tree(d.size() / 2, d.size());
        return *this;
    }

protected:
    std::vector<digest_type> nodes_;
};

}  // namespace ligero::zkp
