#pragma once

#include "entity.hpp"
#include "component.hpp"
#include "type_traits.hpp"
#include "archetype.hpp"

#include <bitset>
#include <type_traits>
#include <ranges>

namespace ecs {

    template<component_reference... Args>
    class view;

    template<component_reference... Args>
    struct view_arguments {
        static constexpr bool is_const = const_component_references_v<Args...>;
    };

    template<typename T>
    struct view_converter {};

    template<typename... Args>
    struct view_converter<std::tuple<Args...>> {
        using view_t = view<Args...>;
        using view_arguments_t = view_arguments<Args...>;
    };

    template<typename F>
    struct func_decomposer {
        using view_converter_t = view_converter<typename function_traits<F>::arguments_tuple_type>;
        using view_t = view_converter_t::view_t;
        static constexpr bool is_const = view_converter_t::view_arguments_t::is_const;
    };

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

            /// @brief Check if entity has component
            /// @tparam C component type
            /// @param e entity
            /// @return boolean
            template<component C>
            [[nodiscard]] bool has(entity e) const {
                ensure_alive(e);
                auto e_id = e.id();
                const auto& loc = get_location(e_id);
                return loc.archetype->template contains<C>();
            }

            template<component_reference... Args>
            ecs::view<Args...> view() requires(!const_component_references_v<Args...>);

            template<component_reference... Args>
            ecs::view<Args...> view() const requires const_component_references_v<Args...>;

            template<typename F>
            void each(F&& func) requires(!func_decomposer<F>::is_const);

            template<typename F>
            void each(F&& func) const requires(func_decomposer<F>::is_const);

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

            [[nodiscard]] archetype_registry& get_archetype_registry() noexcept {
                return archetype_registry_;
            }

            [[nodiscard]] const archetype_registry& get_archetype_registry() const noexcept {
                return archetype_registry_;
            }

            entity_pool entity_pool_;
            archetype_registry archetype_registry_;
            sparse_map<entity_id_t, entity_location> entity_map_;

            template<component_reference... Args>
            friend class view;
    };

    template<component_reference... Args>
    class view {
        public:
            
            static constexpr bool is_const = const_component_references_v<Args...>;

            using registry_type = std::conditional_t<is_const, const registry&, registry&>;

            explicit view(registry_type registry) noexcept : registry_(registry) {}

            decltype(auto) each() requires (!is_const) {
                return mem_blocks(registry_.get_archetype_registry()) | std::views::join;
            }

            decltype(auto) each() const requires (is_const) {
                return mem_blocks(registry_.get_archetype_registry()) | std::views::join;
            }

            void each(auto&& func) requires(!is_const) {
                for (auto mem_block : mem_blocks(registry_.get_archetype_registry())) {
                    for (auto entry : mem_block) {
                        std::apply(func, entry);
                    }
                }
            }

            void each(auto&& func) const requires(is_const) {
                for (auto mem_block : mem_blocks(registry_.get_archetype_registry())) {
                    for (auto entry : mem_block) {
                        std::apply(func, entry);
                    }
                }
            }
            
            const std::size_t size() const noexcept {
                std::size_t c = 0;
                for(const auto& [cs, arch] : registry_.get_archetype_registry()) {
                    if((... && arch->template contains<std::decay_t<Args>>())) {
                        for(const auto& mb : arch->mem_blocks()) {
                            c += mb.size();
                        }
                    }
                }
                return c;
            }

        private:

            static decltype(auto) mem_blocks(auto&& archetype_registry) {
                auto filter_archetypes = [](auto& archetype) {
                    return (... && archetype->template contains<std::decay_t<Args>>());
                };
                auto into_mem_blocks = [](auto& archetype) -> decltype(auto) { return archetype->mem_blocks(); };
                auto as_typed_mem_block = [](auto& mem_block) -> decltype(auto) { return mem_block_view<Args...>(mem_block); };

                return archetype_registry                               // for each archetype entry in archetype map
                    | std::views::values                     // for each value, a pointer to archetype
                    | std::views::filter(filter_archetypes)  // filter archetype by requested components
                    | std::views::transform(into_mem_blocks)     // fetch chunks vector
                    | std::views::join                       // join chunks together
                    | std::views::transform(as_typed_mem_block); // each chunk casted to a typed chunk view range-like type
            }

            registry_type registry_;
    };

    template<component_reference... Args>
    ecs::view<Args...> registry::view()
        requires(!const_component_references_v<Args...>) {
        return ecs::view<Args...>{ *this };
    }

    template<component_reference... Args>
    ecs::view<Args...> registry::view() const
        requires const_component_references_v<Args...> {
        return ecs::view<Args...>{ *this };
    }

    template<typename F>
    void registry::each(F&& func) requires(!func_decomposer<F>::is_const) {
        using view_t = typename func_decomposer<F>::view_t;
        view_t{ *this }.each(std::forward<F>(func));
    }

    template<typename F>
    void registry::each(F&& func) const requires(func_decomposer<F>::is_const) {
        using view_t = typename func_decomposer<F>::view_t;
        view_t{ *this }.each(std::forward<F>(func));
    }

};