#pragma once

#include <cstdint>
#include <vector>
#include <limits>

namespace ecs {

    /// @brief Entity ID type, 32 bit value should be sufficient for all use cases
    using entity_id_t = std::uint32_t;

    /// @brief generation id type, used for determining aliveness of entity after id has been recycled
    using generation_id_t = std::uint32_t;

    class entity {
        public:
            ///@brief Invalid ID number
            static constexpr entity_id_t invalid_id = std::numeric_limits<entity_id_t>::max();

            /// @brief Invalid generation number 
            static constexpr generation_id_t invalid_generation = std::numeric_limits<generation_id_t>::max();

            /// @brief default invalid entity
            static const entity invalid;

            /// @brief Default construct a handle
            constexpr entity() = default;

            explicit constexpr entity(entity_id_t id, generation_id_t generation = 0) noexcept 
                : id_(id), generation_(generation) {}

            /// @brief Test if handle is valid
            [[nodiscard]] constexpr bool valid() const noexcept {
                return *this != entity::invalid;
            }

            /// @brief Return ID number
            [[nodiscard]] constexpr auto id() const noexcept {
                return id_;
            }

            /// @brief return generation number
            [[nodiscard]] constexpr auto generation() const noexcept {
                return generation_;
            }

            /// @brief Spaceship operator
            ///
            /// @param rhs Other handle
            /// @return Automatic
            constexpr auto operator<=>(const entity& rsh) const = default;

        private:

            entity_id_t id_ { invalid_id };
            generation_id_t generation_ { invalid_generation };
    };

    class entity_pool {
        public:

            /// @brief Create new entity handle
            ///
            /// @return entity
            [[nodiscard]] entity create() {
                if(!freed_ids_.empty()) {
                    auto id = freed_ids_.back();
                    freed_ids_.pop_back();
                    return entity { id , generations_[id] };
                }
                entity e { next_id_++ };
                generations_.emplace_back();
                return e;
            }

            /// @brief Check if entity is still alive
            ///
            /// @param e Entity to check
            /// @return True if entity is alive
            [[nodiscard]] bool alive(entity e) {
                if (e.id() < generations_.size()) {
                    return generations_[e.id()] == e.generation();
                }
                return false;
            }

            /// @brief Recycle the entity, entity handle will be reused in next create()
            ///
            /// @param e Entity to recycle
            void recycle(entity e) {
                if(!alive(e)) {
                    return;
                }
                generations_[e.id()] += 1;
                freed_ids_.push_back(e.id());
            }

        private:
            entity_id_t next_id_ = 0UL;
            std::vector<generation_id_t> generations_;
            std::vector<entity_id_t> freed_ids_;
    };

    class archetype;

    class entity_location {
        public:
            // Non-owning pointer to an archetype this entity belongs to
            ecs::archetype* archetype{};

            // Mem block index
            std::size_t mem_block_index{};

            // Entry index in the mem block
            std::size_t entry_index{};
    };

}