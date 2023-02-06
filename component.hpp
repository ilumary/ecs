#pragma once

#include <concepts>
#include <vector>

#include "hash_map.hpp"
#include "dynamic_bitset.hpp"

#ifndef ECS_EXPORT
#if defined _WIN32 || defined __CYGWIN__ || defined _MSC_VER
#define ECS_EXPORT __declspec(dllexport)
#define ECS_IMPORT __declspec(dllimport)
#define ECS_HIDDEN
#elif defined __GNUC__ && __GNUC__ >= 4
#define ECS_EXPORT __attribute__((visibility("default")))
#define ECS_IMPORT __attribute__((visibility("default")))
#define ECS_HIDDEN __attribute__((visibility("hidden")))
#else // Unsupported compiler
#define ECS_EXPORT
#define ECS_IMPORT
#define ECS_HIDDEN
#endif
#endif

namespace ecs {

    template<typename T>
    constexpr static std::string_view type_name() noexcept { return typeid(T).name(); }

    template<typename = void, typename _id_type = std::uint64_t>
    class type_registry {
        public:
            using id_type = _id_type;

        #ifndef ECS_CLIENT
            ECS_EXPORT static id_type id(std::string_view type_string) {
                auto [iter, inserted] = get_id_map().emplace(type_string, get_next_id());
                if (inserted) {
                    get_next_id()++;
                }
                auto type_id = iter->second;
                return type_id;
            }

            ECS_EXPORT static hash_map<std::string_view, id_type>& get_id_map() {
                static hash_map<std::string_view, id_type> id_map{};
                return id_map;
            }

            ECS_EXPORT static id_type& get_next_id() {
                static id_type next_id{};
                return next_id;
            }
        #else
            ECS_IMPORT static hash_map<std::string_view, id_type>& get_id_map();
            ECS_IMPORT static id_type& get_next_id();
            ECS_IMPORT static id_type id(std::string_view type_string);
        #endif
    };

    /// @brief Generate unique sequential IDs for types
    ///
    /// @tparam Base type
    /// @tparam _id_type Type for id
    template<typename Base = void, typename _id_type = std::uint64_t>
    struct type_id {
        using id_type = _id_type;

        template<typename T>
        inline static const id_type value = type_registry<Base, id_type>::id(type_name<T>());
    };

    /// @brief Type meta information
    struct meta_t {
        /// @brief Move constructor callback for type T
        ///
        /// @tparam T Target type
        /// @param ptr Place to construct at
        /// @param rhs Pointer to an object to construct from
        template<typename T>
        static void move_constructor(void* ptr, void* rhs) {
            std::construct_at(static_cast<T*>(ptr), std::move(*static_cast<T*>(rhs)));
        }

        /// @brief Move assignment callback for type T
        ///
        /// @tparam T Target type
        /// @param lhs Pointer to an object to assign to
        /// @param rhs Pointer to an object to assign from
        template<typename T>
        static void move_assignment(void* lhs, void* rhs) {
            *static_cast<T*>(lhs) = std::move(*static_cast<T*>(rhs));
        }

        /// @brief Destructor callback for type T
        ///
        /// @tparam T Target type
        /// @param ptr Pointer to an object to delete
        template<typename T>
        static void destructor(void* ptr) {
            static_cast<T*>(ptr)->~T();
        }

        /// @brief Constructs meta_t for type T
        ///
        /// @tparam T Target type
        /// @return const meta_t* Target type meta
        template<typename T>
        static const meta_t* of() noexcept {
            static const meta_t meta {
                sizeof(T),
                alignof(T),
                type_name<T>(),
                &move_constructor<T>,
                &move_assignment<T>,
                &destructor<T>,
            };
            return &meta;
        }

        std::size_t size;
        std::size_t align;
        std::string_view name;
        void (*move_construct)(void*, void*) = [](void*, void*) -> void {};
        void (*move_assign)(void*, void*) = [](void*, void*) -> void {};
        void (*destruct)(void*) = [](void*) -> void {};
    };

    /// @brief Type for component ID
    using component_id_t = std::uint32_t;

    /// @brief Type for family used to generated component IDs.
    using component_id = type_id<struct _component_family_t, component_id_t>;   

    /// @brief Component concept. The component must be a struct/class that can be move constructed and move
    /// assignable
    ///
    /// @tparam T Component type
    template<typename T>
    concept component = std::is_class_v<T> && std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>;

    /// @brief concept of a reference or const reference to C, where C satisfies component concept
    /// @tparam T Component reference type
    template<typename T>
    concept component_reference = std::is_reference_v<T> && component<std::remove_cvref_t<T>>;

    /// @brief Struct to determine const-ness of component reference type
    ///
    /// @tparam T component reference type
    template<component_reference T>
    struct const_component_reference {
        constexpr static bool value = std::is_const_v<std::remove_reference_t<T>>;
    };

    /// @brief Returns true for const component references
    ///
    /// @tparam T component_reference type
    template<component_reference T>
    constexpr bool const_component_reference_v = const_component_reference<T>::value;

    /// @brief Struct to determine mutability of component reference type
    ///
    /// @tparam T component reference type
    template<component_reference T>
    struct mutable_component_reference {
        constexpr static bool value = !std::is_const_v<std::remove_reference_t<T>>;
    };

    /// @brief Returns true for non-const component references
    ///
    /// @tparam T component_reference type
    template<component_reference T>
    constexpr bool mutable_component_reference_v = mutable_component_reference<T>::value;

    /// @brief Struct to determine whether all component references are const
    ///
    /// @tparam Args Component references
    template<component_reference... Args>
    struct const_component_references {
        constexpr static bool value = std::conjunction_v<const_component_reference<Args>...>;
    };

    /// @brief Returns true when all Args are const references
    ///
    /// @tparam Args Component references
    template<component_reference... Args>
    constexpr bool const_component_references_v = const_component_references<Args...>::value;

    /// @brief Component metadata. Stores an ID, size, alignment, destructor, etc.
    struct component_meta {
        /// @brief Constructs component_meta for type T
        ///
        /// @tparam T Component type
        /// @return component_meta Component metadata
        template<component T>
        static component_meta of() noexcept {
            return component_meta{
                component_id::value<T>,
                meta_t::of<T>(),
            };
        }

        /// @brief Spaceship operator
        ///
        /// @param rhs Right hand side
        /// @return auto Result of comparison
        constexpr auto operator<=>(const component_meta& rhs) const noexcept {
            return id <=> rhs.id;
        }

        /// @brief Equality operator
        ///
        /// @param rhs Right hand side
        /// @return true If equal
        /// @return false If not equal
        constexpr bool operator==(const component_meta& rhs) const noexcept {
            return id == rhs.id;
        }

        component_id_t id;
        const meta_t* type;
    };

    /// @brief Component set stores set of component ID's
    class component_set {
        public:
            /// @brief Construct component set from given component types
            ///
            /// @tparam Args Components type parameter pack
            /// @return component_set Component set
            template<component... Args>
            static component_set create() {
                component_set s;
                (..., s.insert<Args>());
                return s;
            }

            /// @brief Insert component of type T
            ///
            /// @tparam T Component type
            template<component T>
            void insert() {
                insert(component_id::value<T>);
            }

            /// @brief Erase component of type T
            ///
            /// @tparam T Component type
            template<component T>
            void erase() {
                erase(component_id::value<T>);
            }

            /// @brief Check if component of type T is present in the set
            ///
            /// @tparam T Component type
            /// @return true When component type T is present
            /// @return false When component type T is not present
            template<component T>
            [[nodiscard]] bool contains() const {
                return contains(component_id::value<T>);
            }

            /// @brief Inserts component into the set
            ///
            /// @param id Component ID
            void insert(component_id_t id) {
                bitset_.set(id);
            }

            /// @brief Erases component from the set
            ///
            /// @param id Component ID
            void erase(component_id_t id) {
                bitset_.set(id, false);
            }

            /// @brief Check if component is present in the set
            ///
            /// @param id Component ID
            /// @return true When component ID is present
            /// @return false When component ID is not present
            [[nodiscard]] bool contains(component_id_t id) const {
                return bitset_.test(id);
            }

            void clear() noexcept {
                bitset_.clear();
            }

            /// @brief Equality operator
            ///
            /// @param rhs Right hand side
            /// @return true If equal
            /// @return false If not equal
            bool operator==(const component_set& rhs) const noexcept {
                return bitset_ == rhs.bitset_;
            }

        private:

            friend class component_set_hasher;
            dynamic_bitset<> bitset_{};
    };

    /// @brief Component set hasher
    class component_set_hasher {
        public:
            /// @brief Hash component set
            ///
            /// @param set Component set
            /// @return std::size_t Hash value
            std::size_t operator()(const component_set& set) const {
                return std::hash<dynamic_bitset<>>()(set.bitset_);
            }
    };

    class component_meta_set {
        public:
            /// @brief Construct component set from given component types
            ///
            /// @tparam Args Components type parameter pack
            /// @return component_meta_set Component set
            template<component... Args>
            static component_meta_set create() {
                component_meta_set s;
                s.components_meta_data_.reserve(sizeof...(Args));
                (..., s.insert<Args>());
                return s;
            }

            /// @brief Insert component of type T
            ///
            /// @tparam T Component type
            template<component T>
            void insert() {
                insert(component_meta::of<T>());
            }

            /// @brief Erase component of type T
            ///
            /// @tparam T Component type
            template<component T>
            void erase() {
                erase(component_id::value<T>);
            }

            /// @brief Check if component of type T is present in the set
            ///
            /// @tparam T Component type
            /// @return true When component type T is present
            /// @return false When component type T is not present
            template<component T>
            [[nodiscard]] bool contains() const {
                return contains(component_id::value<T>);
            }

            /// @brief Inserts component into the set
            ///
            /// @param meta Component meta
            void insert(const std::vector<component_meta>::value_type& meta) {
                if (contains(meta.id)) {
                    return;
                }
                component_set_.insert(meta.id);
                components_meta_data_.emplace_back(meta);
            }

            /// @brief Erases component from the set
            ///
            /// @param id Component ID
            void erase(component_id_t id) {
                if (!contains(id)) {
                    return;
                }
                component_set_.erase(id);
                std::erase_if(components_meta_data_, [id](const auto& meta) { return meta.id == id; });
            }

            /// @brief Check if component is present in the set
            ///
            /// @param id Component ID
            /// @return true When component ID is present
            /// @return false When component ID is not present
            [[nodiscard]] bool contains(component_id_t id) const {
                return component_set_.contains(id);
            }

            /// @brief Returns how many components in the set
            ///
            /// @return std::size_t Number of components in the set
            [[nodiscard]] std::size_t size() const noexcept {
                return components_meta_data_.size();
            }

            /// @brief Return const iterator to beginning of the set
            ///
            /// @return const_iterator Iterator
            [[nodiscard]] std::vector<component_meta>::const_iterator begin() const noexcept {
                return components_meta_data_.begin();
            }

            /// @brief Return const iterator to the end of the set
            ///
            /// @return const_iterator Iterator
            [[nodiscard]] std::vector<component_meta>::const_iterator end() const noexcept {
                return components_meta_data_.end();
            }

            /// @brief Return const iterator to beginning of the set
            ///
            /// @return const_iterator Iterator
            [[nodiscard]] std::vector<component_meta>::const_iterator cbegin() const noexcept {
                return begin();
            }

            /// @brief Return const iterator to the end of the set
            ///
            /// @return const_iterator Iterator
            [[nodiscard]] std::vector<component_meta>::const_iterator cend() const noexcept {
                return end();
            }

            /// @brief Equality operator
            ///
            /// @param rhs Right hand side
            /// @return true If equal
            /// @return false If not equal
            bool operator==(const component_meta_set& rhs) const noexcept {
                return component_set_ == rhs.component_set_;
            }

            /// @brief Return a bitset of components
            ///
            /// @return const component_set& Component bitset
            [[nodiscard]] const component_set& ids() const noexcept {
                return component_set_;
            }

        private:
            component_set component_set_; //bitmask
            std::vector<component_meta> components_meta_data_;
    };
}