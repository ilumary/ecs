#pragma once

#include <type_traits>

namespace ecs {

    template<typename... Rest>
    struct unique_types;

    template<typename T1, typename T2, typename... Rest>
    struct unique_types<T1, T2, Rest...>
        : unique_types<T1, T2>
        , unique_types<T1, Rest...>
        , unique_types<T2, Rest...> {};

    template<>
    struct unique_types<> {};

    template<typename T1>
    struct unique_types<T1> {};

    template<class T1, class T2>
    struct unique_types<T1, T2> {
        static_assert(!std::is_same<T1, T2>::value, "Types must be unique within parameter pack");
    };

}