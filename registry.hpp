#pragma once

#include "entity.hpp"
#include "component.hpp"
#include "type_traits.hpp"
#include "archetype.hpp"

#include <bitset>

namespace ecs {

    class registry {

        public:

            template<component... Args>
            entity create(Args&&... args) {
                [[maybe_unused]] unique_types<Args...> uniqueness;

                auto entity = entity_pool_.create();
                auto archetype = archetype_registry_.ensure_archetype<Args...>();
                auto location = archetype->template emplace_back<Args...>(entity, std::forward<Args>(args)...);
                save_location(entity.id(), location);

                return entity;
            }

            void destroy(entity e) {
                ensure_alive(e);
                auto location = get_location(e.id());

                auto moved = location.archetype->erase_and_fill(location);
                remove_location(e.id());

                if(moved) { save_location(moved->id(), location); }
                auto tmp = get_location(moved->id());
                entity_pool_.recycle(e);
            }

            [[nodiscard]] bool alive(entity e) noexcept {
                return entity_pool_.alive(e);
            }

            /// @brief Get reference to component C
            /// @tparam C Component C
            /// @param ent Entity to read component from
            /// @return C& Reference to component C
            template<component C>
            [[nodiscard]] C& get(entity ent) {
                return std::get<0>(get_impl<C&>(*this, ent));
            }

            /// @brief Get const reference to component C
            /// @tparam C Component C
            /// @param ent Entity to read component from
            /// @return const C& Const reference to component C
            template<component C>
            [[nodiscard]] const C& get(entity ent) const {
                return std::get<0>(get_impl<const C&>(*this, ent));
            }

            /// @brief Get components for a single entity
            /// @tparam Args Component reference
            /// @param ent Entity to query
            /// @return value_type Components tuple
            template<component_reference... Args>
            [[nodiscard]] std::tuple<Args...> get(entity ent)
                requires(const_component_references_v<Args...>) {
                return get_impl<Args...>(*this, ent);
            }

            /// @brief Get components for a single entity
            /// @tparam Args Component references
            /// @param ent Entity to query
            /// @return value_type Components tuple
            template<component_reference... Args>
            [[nodiscard]] std::tuple<Args...> get(entity ent) const
                requires(!const_component_references_v<Args...>) {
                return get_impl<Args...>(*this, ent);
            }

        private:

            template<component_reference... Args>
            static std::tuple<Args...> get_impl(auto&& self, entity e) {
                self.ensure_alive(e);
                auto& loc = self.get_location(e.id());
                auto* archetype = loc.archetype;
                return std::tuple<Args...>(std::ref(archetype->template get<Args>(loc))...);
            }

            inline void ensure_alive(const entity& e) const {
                if(!const_cast<registry*>(this)->alive(e)) {
                    throw std::logic_error{"Entity not found"};
                }
            }

            void save_location(entity_id_t id, const entity_location& el) {
                entity_map_[id] = el;
            }

            void remove_location(entity_id_t id) {
                entity_map_.erase(id);
            };

            [[nodiscard]] const entity_location& get_location(entity_id_t id) const {
                return entity_map_.at(id);
            }

            [[nodiscard]] entity_location& get_location(entity_id_t id) {
                return entity_map_.at(id);
            }

            entity_pool entity_pool_;
            archetype_registry archetype_registry_;
            sparse_map<entity_id_t, entity_location> entity_map_;
    };

};