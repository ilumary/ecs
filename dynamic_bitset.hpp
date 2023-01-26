#pragma once

#include <algorithm>
#include <climits>
#include <cstdint>
#include <ranges>
#include <vector>
#include <iostream>

namespace ecs {

    /// @brief Dynamically growing bitset
    ///
    /// @tparam A Allocator type
    template<typename T = std::uint64_t, typename A = std::allocator<T>>
    class dynamic_bitset {
        public:

            /// @brief Construct a new dynamic bitset object
            ///
            /// @param initial_bits Initial size of the bitset
            explicit dynamic_bitset(std::size_t initial_blocks = 1) {
                blocks_.resize(initial_blocks);
            }

            /// @brief Check if bit at given position is set
            ///
            /// @param pos Position of the bit
            /// @return true If bit is set
            /// @return false If bit is unset
            [[nodiscard]] inline bool test(std::size_t pos) const noexcept {
                const auto [block_index, bit_pos] = block_and_bit(pos);
                if (block_index < blocks_.size()) {
                    return blocks_[block_index] & (T{ 1 } << bit_pos);
                }
                return false;
            }

            /// @brief Set the bit value
            ///
            /// @param pos Position of the bit to set
            /// @param value Value to set
            /// @return dynamic_bitset&
            inline dynamic_bitset& set(std::size_t pos, bool value = true) {
                const auto [block_index, bit_pos] = block_and_bit(pos);
                if (block_index >= blocks_.size()) {
                    blocks_.resize(block_index + 1);
                }
                if (value) {
                    blocks_[block_index] |= ( T{ 1 } << bit_pos );
                } else {
                    blocks_[block_index] &= ~( T{ 1 } << bit_pos );

                    std::size_t i = blocks_.size() - 1;
                    while (!blocks_[i] && i != 0) {
                        i--;
                    }
                    blocks_.resize(i + 1);
                }
                return *this;
            }

            /// @brief Clear all bits
            inline void clear() noexcept {
                blocks_.clear();
            }

            /// @brief Equality operator
            ///
            /// @param rhs Right hand side bitset
            /// @return true If bitsets are equal
            /// @return false If bitsets aren't equal
            bool operator==(const dynamic_bitset& rhs) const noexcept {
                return std::equal(blocks_.begin(), blocks_.end(), rhs.blocks_.begin(), rhs.blocks_.end());
            }

        private:
            friend struct std::hash<dynamic_bitset>;

            static inline std::pair<std::size_t, std::size_t> block_and_bit(std::size_t pos) {
                const auto bit_pos = pos % (sizeof(T) * CHAR_BIT);
                const auto block_index = pos / (sizeof(T) * CHAR_BIT);
                return std::make_pair(block_index, bit_pos);
            }

            std::vector<T, A> blocks_;
    };
}

namespace std {

    /// @brief Hash implementation for dynamic_bitset
    template<typename A>
    struct hash<ecs::dynamic_bitset<A>> {
        std::size_t operator()(const ecs::dynamic_bitset<A>& bitset) const {
            std::size_t hash = 0;
            for (auto block : bitset.blocks_) {
                hash ^= block;
            }
            return hash;
        }
    };

}