#pragma once

#include <numeric>
#include <cstring>

#include "hash_map.hpp"
#include "sparse_map.hpp"
#include "component.hpp"
#include "mem_block.hpp"

namespace ecs {

    /// @brief Archetype groups entities that share the same types of components. 
    /// Archetype has a list of fixed size chunks where entities and their
    /// components are stored in a packed arrays, in a so called SoA fashion
    class archetype {

        public: 

            archetype() = default;

            explicit archetype(component_meta_set components) : components_(components) {
                max_size_ = get_max_size(components_);
                //std::cout << "Max size: " << max_size_ << std::endl;
                init_component_sections(components_);
                mem_blocks_.emplace_back(mem_blocks_info_, max_size_);
            }

            template<component... Components>
            entity_location emplace_back(entity ent, Components&&... components) {
                auto& free_mem_block = ensure_free_mem_block();
                auto entry_index = free_mem_block.size();
                auto mem_block_index = mem_blocks_.size() - 1;

                free_mem_block.emplace_back(ent, std::forward<Components>(components)...);

                return entity_location {
                    this, mem_block_index, entry_index
                };
            }

            /// @brief erase an entity at given location, fill possible gap with last
            /// entity
            /// @param location enity location
            /// @return std::optional<entity>
            std::optional<entity> erase_and_fill(const entity_location& loc) noexcept {
                auto& mem_block = get_mem_block(loc); //get mem_block of location
                auto& crnt_mem_block = mem_blocks_.back(); //get current mem_block thats being used
                auto opt_ent = mem_block.erase_and_fill(loc.entry_index, crnt_mem_block);

                if(crnt_mem_block.empty() && mem_blocks_.size() > 1) {
                    mem_blocks_.pop_back();
                }

                return opt_ent;
            }

            /// @brief Get component data
            ///
            /// @tparam ComponentRef Component reference type
            /// @param id Component ID
            /// @param loc Entity location
            /// @return Component& Component reference
            template<component_reference ComponentRef>
            ComponentRef get(entity_location loc) {
                return get_component_reference<ComponentRef>(*this, loc);
            }

            /// @brief Get component data
            ///
            /// @tparam ComponentRef Component reference type
            /// @param id Component ID
            /// @param loc Entity location
            /// @return ComponentRef Component reference
            template<component_reference ComponentRef>
            ComponentRef get(entity_location loc) const {
                static_assert( std::is_const_v<std::remove_reference_t<ComponentRef>>,
                    "Can only get a non-const reference on const archetype");
                return get_component_reference<ComponentRef>(*this, loc);
            }

            template<component C>
            [[nodiscard]] bool contains() const noexcept {
                if constexpr (std::is_same_v<C, entity>) {
                    return true;
                } else {
                    return components_.contains<C>();
                }
            };

            [[nodiscard]] bool contains(component_id_t component_id) const noexcept {
                if (component_id == component_id::value<entity>) {
                    return true;
                } else {
                    return components_.contains(component_id);
                }
            }

            [[nodiscard]] std::vector<mem_block>& mem_blocks() noexcept {
                return mem_blocks_;
            }

            [[nodiscard]] const std::vector<mem_block>& mem_blocks() const noexcept {
                return mem_blocks_;
            }

        private:

            void init_component_sections(const component_meta_set& components_meta) {
                // make space for entity
                auto offset = add_component_section(0, component_meta::of<entity>());
                // space for all components
                for (const auto& meta : components_meta) {
                    offset = add_component_section(offset, meta);
                }
            }

            std::size_t add_component_section(std::size_t offset, const component_meta& meta) {
                const std::size_t size_in_bytes = max_size_ * meta.type->size;
                const std::size_t align = meta.type->align;
                mem_blocks_info_.emplace(meta.id, offset, meta);
                offset += mod_2n(offset, align) + size_in_bytes;
                return offset;
            }

            static std::size_t get_max_size(auto&& components_meta) {
                auto aligned_size = aligned_components_size(components_meta);
                //std::cout << "Alinged components size: " << aligned_size << std::endl;

                // handle subtraction overflow - memory block size is insufficient to hold at least one such entity
                if (aligned_size > mem_block::mem_block_size) [[unlikely]] {
                    throw std::overflow_error("Mem block too small for component size");
                }

                // Remaining size for packed components
                auto remaining_space = mem_block::mem_block_size - aligned_size;
                //std::cout << "Remaining space: " << remaining_space << std::endl;

                // Calculate how much components we can pack into remaining space
                auto remaining_elements_count = remaining_space / packed_components_size(components_meta);
                //std::cout << "Packed component size: " << packed_components_size(components_meta) << std::endl;
                //std::cout << "Remaining elements count: " << remaining_elements_count << std::endl;

                // The maximum amount of entities we can hold is grater by 1 for which we calculated aligned_size
                return remaining_elements_count + 1;
            }

            static std::size_t packed_components_size(auto&& components_meta) noexcept {
                return std::accumulate(components_meta.begin(),
                    components_meta.end(),
                    component_meta::of<entity>().type->size,
                    [](const auto& res, const auto& meta) { return res + meta.type->size; });
            }

            static std::size_t aligned_components_size(auto&& components_meta) noexcept {
                auto begin = mem_block::alloc_alignment;
                auto end = begin;

                // Add single component element size accounting for its alignment
                auto add_elements = [&end](const component_meta& meta) {
                    auto t_end = end;
                    std::memcpy(&t_end, &end, sizeof(t_end));
                    end += mod_2n(t_end, meta.type->align);
                    //std::cout << "Name: " << meta.type->name << ", Align: " << meta.type->align << ", t_end: " << t_end << std::endl;
                    //std::cout << "End + mod " << mod_2n(t_end, meta.type->align) << std::endl;
                    end += meta.type->size;
                    //std::cout << "End + size " << meta.type->size << std::endl;
                };

                add_elements(component_meta::of<entity>());
                for (const auto& meta : components_meta) {
                    add_elements(meta);
                }

                return end - begin;
            }

            mem_block& ensure_free_mem_block() {
                auto& mb = mem_blocks_.back();
                if(!mb.full()) {
                    return mb;
                }
                mem_blocks_.emplace_back(mem_blocks_info_, max_size_);
                return mem_blocks_.back();
            }

            inline mem_block& get_mem_block(entity_location loc) noexcept {
                assert((loc.archetype == this) && "Location archetype pointer points at unknown archetype");
                assert((loc.mem_block_index < mem_blocks_.size()) && "Memory block index points at inaccessible location");
                auto& mb = mem_blocks_.at(loc.mem_block_index);
                return mb;
            }

            template<component_reference ComponentRef>
            inline static ComponentRef get_component_reference(auto&& self, entity_location loc) {
                auto& mem_block = self.get_mem_block(loc);
                assert((loc.entry_index < mem_block.size()) && "Entity location index exeeds memory block size");
                return *component_fetch::fetch_pointer<ComponentRef>(mem_block, loc.entry_index);
            }

            component_meta_set components_{};
            std::size_t max_size_{};
            std::vector<mem_block> mem_blocks_{};
            sparse_map<component_id_t, block_metadata> mem_blocks_info_;
    };

    /// @brief Container for archetypes, stores map [component_set => archetype]
    class archetype_registry {

        public:

            using storage_type_t = hash_map<component_set, std::unique_ptr<archetype>, component_set_hasher>;

            /// @brief Get or create an archetype matching the passed Components types
            ///
            /// @tparam Components Component types
            /// @return archetype*
            template<component... Components>
            archetype* ensure_archetype() {
                tmp_component_set_.clear();
                (..., tmp_component_set_.insert<Components>());

                auto& archetype = archetypes_[tmp_component_set_];
                if (!archetype) {
                    archetype = create_archetype(component_meta_set::create<Components...>());
                }
                return archetype.get();
            }

            /// @brief Returns iterator to the beginning of archetypes container
            ///
            /// @return decltype(auto)
            storage_type_t::iterator begin() noexcept {
                return archetypes_.begin();
            }

            /// @brief Returns iterator to the end of archetypes container
            ///
            /// @return decltype(auto)
            storage_type_t::iterator end() noexcept {
                return archetypes_.end();
            }

            /// @brief Returns const iterator to the beginning of archetypes container
            ///
            /// @return decltype(auto)
            [[nodiscard]] storage_type_t::const_iterator begin() const noexcept {
                return archetypes_.begin();
            }

            /// @brief Returns const iterator to the end of archetypes container
            ///
            /// @return decltype(auto)
            [[nodiscard]] storage_type_t::const_iterator end() const noexcept {
                return archetypes_.end();
            }

            /// @brief Returns the number of archetypes
            ///
            /// @return std::size_t
            [[nodiscard]] std::size_t size() const noexcept {
                return archetypes_.size();
            }


        private:

            static decltype(auto) create_archetype(auto&& components_meta) {
                return std::make_unique<ecs::archetype>(std::forward<decltype(components_meta)>(components_meta));
            }

            component_set tmp_component_set_{};
            storage_type_t archetypes_{};
    };

}