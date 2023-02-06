#pragma once

#include <cstdint>
#include <type_traits>
#include <sstream>

#include "entity.hpp"
#include "component.hpp"
#include "sparse_map.hpp"

namespace ecs {

    /// @brief Block metadata holds pointers where it begins, ends and a component metadata it holds
    struct block_metadata {
        std::size_t offset{};
        component_meta meta{};

        block_metadata(std::size_t offset, const component_meta& meta) noexcept
            : offset(offset), meta(meta) {}
    };

    /// @brief Chunk holds a 16 Kb block of memory that holds components in blocks:
    /// |A1|A2|A3|...padding|B1|B2|B3|...padding|C1|C2|C3...padding where A, B, C
    /// are component types and A1, B1, C1 and others are components instances.
    class mem_block {

        public:

            /// @brief Chunk size in bytes
            static constexpr std::size_t mem_block_size = static_cast<const std::size_t>(16U * 1024);

            /// @brief Block allocation alignment
            static constexpr std::size_t alloc_alignment = alignof(entity);

            mem_block(const sparse_map<component_id_t, block_metadata>& mem_blocks_info, std::size_t max_size) 
                : mem_blocks_info_(&mem_blocks_info), max_size_(max_size),
                  buffer_(reinterpret_cast<std::byte*>(std::malloc(mem_block_size))) {}

            // delete copy constructor and copy assignment operator
            mem_block(const mem_block& rhs) = delete;
            mem_block& operator=(const mem_block& rhs) = delete;

            /// @brief move constructor 
            mem_block(mem_block&& rhs) noexcept
                : buffer_(rhs.buffer_), number_of_elements_(rhs.number_of_elements_), max_size_(rhs.max_size_), mem_blocks_info_(rhs.mem_blocks_info_) {
                rhs.buffer_ = nullptr;
            }

            /// @brief move assignment operator
            mem_block& operator=(mem_block&& rhs) noexcept {
                buffer_ = rhs.buffer_;
                number_of_elements_ = rhs.number_of_elements_;
                max_size_ = rhs.max_size_;
                mem_blocks_info_ = rhs.mem_blocks_info_;
                rhs.buffer_ = nullptr;
                return *this;
            }

            ~mem_block() {
                if(buffer_ == nullptr) {
                    return;
                }

                for(const auto& [id, block_md] : *mem_blocks_info_) {
                    for(std::size_t i = 0; i < number_of_elements_; ++i) {
                        //TODO investigate why this causes crash
                        //block_md.meta.type->destruct(buffer_ + block_md.offset + i * block_md.meta.type->size);
                    }
                }

                std::free(buffer_);
            }

            template<component... Args>
            void emplace_back(entity ent, Args&&... args) {
                assert((!full()) && "Memory block is full, cannot add another entity");
                std::construct_at(buffer_ptr<entity>(size()), ent);
                (..., std::construct_at(mut_ptr<Args>(size()), std::move(args)));
                number_of_elements_++;
            }

            /// @brief erase and entity at given index, fill gap with last entity if possible
            /// @param index index of entity
            /// @param other mem_block of last entity
            /// @return std::optional<entity>
            std::optional<entity> erase_and_fill(std::size_t index, mem_block& other) noexcept {
                assert((index < number_of_elements_) && "Entity index exeeds known size");
                if(number_of_elements_ == 1 || index == number_of_elements_ - 1) { // index points to last element => nothing has to be filled
                    delete_last_entity();
                    return std::nullopt;
                }
                assert((!other.empty()) && "Other memory block is empty, cannot move entity");
                const std::size_t other_mem_block_index = other.size() - 1;
                entity ent = *other.buffer_ptr<entity>(other_mem_block_index);
                //iterate over all component_blocks inside mem_block and move them to freed spot
                for (const auto& [id, block] : *mem_blocks_info_) {
                    auto other_block = other.mem_blocks_info_->find(id)->second;
                    const auto* type = block.meta.type;
                    auto* ptr = other.buffer_ + other_block.offset + other_mem_block_index * type->size;
                    type->move_assign(buffer_ + block.offset + index * type->size, ptr);
                }
                other.delete_last_entity();
                return ent;
            }

            void delete_last_entity() noexcept {
                assert((!empty()) && "Memory block is empty, cannot destroy last entity");
                number_of_elements_--;
                destroy_at(number_of_elements_);
            }

            template<component T>
            inline T* mut_ptr(std::size_t index) {
                static_assert(!std::is_same_v<T, entity>, "Cannot give a mutable pointer/reference to the entity");
                return buffer_ptr_impl<T*>(*this, index);
            }

            template<component T>
            inline const T* const_ptr(std::size_t index) const {
                return buffer_ptr_impl<const T*>(*this, index);
            }

            [[nodiscard]] constexpr std::size_t max_size() const noexcept { return max_size_; }
            [[nodiscard]] constexpr std::size_t size() const noexcept { return number_of_elements_; }
            [[nodiscard]] constexpr bool full() const noexcept { return size() == max_size(); }
            [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

        private:

            template<component T>
            inline T* buffer_ptr(std::size_t index) noexcept {
                return buffer_ptr_impl<T*>(*this, index);
            }

            template<component T>
            [[nodiscard]] inline const T* buffer_ptr(std::size_t index) const noexcept {
                return buffer_ptr_impl<const T*>(*this, index);
            }

            template<typename P>
            static inline P buffer_ptr_impl(auto&& self, std::size_t index) {
                using component_type = std::remove_const_t<std::remove_pointer_t<P>>;
                const auto& block = self.get_block(component_id::value<component_type>);
                return (reinterpret_cast<P>(self.buffer_ + block.offset) + index);
            }

            [[nodiscard]] const block_metadata& get_block(component_id_t id) const {
                return mem_blocks_info_->at(id);
            }

            inline void destroy_at(std::size_t index) noexcept {
                for(const auto& [id, block] : *mem_blocks_info_) {
                    block.meta.type->destruct(buffer_ + block.offset + index * block.meta.type->size);
                }
            }

            std::byte* buffer_{};
            std::size_t max_size_{}, number_of_elements_{};
            const sparse_map<component_id_t, block_metadata>* mem_blocks_info_;
    };

    /// @brief namespace for fetching single component from memory block
    struct component_fetch {
        
        /// @brief Fetches const pointer for component reference
        /// @tparam C component type
        /// @param mb memory block
        /// @param index index
        /// @return const pointer to component
        template<component_reference C>
        static const std::decay_t<C>* fetch_pointer(auto&& mb, std::size_t index) 
            requires(std::is_const_v<std::remove_reference_t<C>>) {
            try {
                return mb.template const_ptr<std::decay_t<C>>(index);
            } catch (const std::out_of_range&) {
                std::stringstream ss;
                ss << "Component \"" << meta_t::of<std::decay_t<C>>()->name << "\" not found";
                throw std::logic_error{ss.str()};
            }
        }

        /// @brief Fetches mutable pointer for component reference
        /// @tparam C component type
        /// @param mb memory block
        /// @param index index
        /// @return mutable pointer to component
        template<component_reference C>
        static std::decay_t<C>* fetch_pointer(auto&& mb, std::size_t index)
            requires(!std::is_const_v<std::remove_reference_t<C>>) {
            try {
                return mb.template mut_ptr<std::decay_t<C>>(index);
            } catch (const std::out_of_range&) {
                std::stringstream ss;
                ss << "Component \"" << meta_t::of<std::decay_t<C>>()->name << "\" not found";
                throw std::logic_error{ss.str()};
            }
        }
    };

    template<component_reference... Args>
    class mem_block_view {
        public:

            static constexpr bool is_const = const_component_references_v<Args...>;

            using mem_block_type = std::conditional_t<is_const, const mem_block&, mem_block&>;

            /// @brief implements an iterator over memory blocks
            class mem_block_iterator {
                public:
                    using iterator_concept = std::forward_iterator_tag;
                    using iterator_category = std::forward_iterator_tag;
                    using difference_type = int;
                    using value_type = std::tuple<Args...>;
                    using reference = std::tuple<Args...>;
                    using element_type = reference;

                    constexpr mem_block_iterator() = default;
                    constexpr ~mem_block_iterator() = default;

                    explicit constexpr mem_block_iterator(mem_block_type mb, std::size_t index = 0)
                        : pointers_(std::make_tuple(component_fetch::fetch_pointer<Args>(mb, index)...)) {}

                    constexpr mem_block_iterator(const mem_block_iterator& rhs) noexcept = default;
                    constexpr mem_block_iterator& operator=(const mem_block_iterator& rhs) noexcept = default;
                    constexpr mem_block_iterator(mem_block_iterator&& rhs) noexcept = default;
                    constexpr mem_block_iterator& operator=(mem_block_iterator&& rhs) noexcept = default;

                    constexpr mem_block_iterator& operator++() noexcept {
                        std::apply([](auto&&... args) { (args++, ...); }, pointers_);
                        return *this;
                    }

                    constexpr mem_block_iterator operator++(int) noexcept {
                        mem_block_iterator tmp(*this);
                        std::apply([](auto&&... args) { (args++, ...); }, pointers_);
                        return tmp;
                    }

                    constexpr reference operator*() const noexcept {
                        return std::apply([](auto&&... args) { return std::make_tuple(std::ref(*args)...); }, pointers_);
                    }

                    constexpr auto operator<=>(const mem_block_iterator& rhs) const noexcept = default;

                private:
                    std::tuple<std::add_pointer_t<std::remove_reference_t<Args>>...> pointers_;
            };

            explicit mem_block_view(mem_block_type mb) : mem_block_(mb) {}

            constexpr mem_block_iterator begin() noexcept {
                return mem_block_iterator(mem_block_, 0);
            }

            constexpr mem_block_iterator end() noexcept {
                return mem_block_iterator(mem_block_, mem_block_.size());
            }

            const std::size_t size() const noexcept {
                return mem_block_.size();
            }

        private:
            mem_block_type mem_block_;
    };
}