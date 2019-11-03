
// MIT License
//
// Copyright (c) 2019 degski
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>

#include <experimental/fixed_capacity_vector>

#include <sax/uniform_int_distribution.hpp>

#ifdef small
#    define org_small small
#    undef small
#endif

namespace detail {

template<int Size, typename T = int, typename U = float>
struct VoseAliasMethodTables {
    std::array<U, Size> m_probability{ };
    std::array<T, Size> m_alias{ };

    [[nodiscard]] int size ( ) const noexcept { return static_cast<int> ( m_probability.size ( ) ); }
};

template<typename T, std::size_t Size>
[[nodiscard]] inline constexpr T pop ( std::experimental::fixed_capacity_vector<T, Size> & v_ ) noexcept {
    T const r = v_.back ( );
    v_.pop_back ( );
    return r;
}

} // namespace detail

template<int Size, typename T = int>
struct uniformly_decreasing_discrete_distribution;

template<int Size, typename T = int>
struct param_type {

    static_assert ( Size > 1, "size should be larger than 1" );

    template<int, typename>
    friend struct uniformly_decreasing_discrete_distribution;

    using result_type       = T;
    using distribution_type = uniformly_decreasing_discrete_distribution<Size, T>;

    constexpr param_type ( ) noexcept = default;

    [[nodiscard]] bool operator== ( const param_type & right ) const noexcept { return true; };
    [[nodiscard]] bool operator!= ( const param_type & right ) const noexcept { return false; };

    private:
    using VoseAliasMethodTables = detail::VoseAliasMethodTables<Size, T>;
    using FloatVector           = std::experimental::fixed_capacity_vector<float, Size>;
    using IntVector             = std::experimental::fixed_capacity_vector<int, Size>;

    static constexpr int Sum = Size % 2 == 0 ? ( ( Size / 2 ) * ( Size + 1 ) ) : ( Size * ( ( Size + 1 ) / 2 ) );

    [[nodiscard]] static constexpr VoseAliasMethodTables generate_sample_table ( ) noexcept {
        FloatVector probability;
        for ( std::int64_t i = Size * Size; i > 0; i -= Size )
            probability.emplace_back ( static_cast<float> ( i ) / static_cast<float> ( Sum ) );
        IntVector large, small;
        T i = T{ 0 };
        for ( float const p : probability )
            if ( p >= 1.0f )
                large.push_back ( i++ );
            else
                small.push_back ( i++ );
        VoseAliasMethodTables tables;
        while ( large.size ( ) and small.size ( ) ) {
            T g                       = detail::pop ( large );
            T const l                 = detail::pop ( small );
            tables.m_probability[ l ] = probability[ l ];
            tables.m_alias[ l ]       = g;
            probability[ g ]          = ( ( probability[ g ] + probability[ l ] ) - 1.0f );
            if ( probability[ g ] >= 1.0f )
                large.emplace_back ( std::move ( g ) );
            else
                small.emplace_back ( std::move ( g ) );
        }
        while ( large.size ( ) )
            tables.m_probability[ detail::pop ( large ) ] = 1.0f;
        while ( small.size ( ) )
            tables.m_probability[ detail::pop ( small ) ] = 1.0f;
        return tables;
    }

    static constexpr VoseAliasMethodTables const m_sample_table = generate_sample_table ( );
};

template<int Size, typename T>
struct uniformly_decreasing_discrete_distribution : param_type<Size, T> {

    using param_type  = param_type<Size, T>;
    using result_type = typename param_type::result_type;

    // Sample with a linearly decreasing probability.
    // Iff size was 3, the probabilities of the CDF would
    // be 3/6, 5/6, 6/6 (or 3/6, 2/6, 1/6 for the PDF).
    template<typename Generator>
    [[nodiscard]] result_type operator( ) ( Generator & gen_ ) noexcept {
        int const column = sax::uniform_int_distribution<int> ( min ( ), max ( ) ) ( gen_ );
        return std::bernoulli_distribution ( param_type::m_sample_table.m_probability[ column ] ) ( gen_ )
                   ? column
                   : param_type::m_sample_table.m_alias[ column ];
    }

    void reset ( ) const noexcept {}

    [[nodiscard]] constexpr result_type min ( ) const noexcept { return result_type{ 0 }; }
    [[nodiscard]] constexpr result_type max ( ) const noexcept { return result_type{ Size - 1 }; }
};

#ifdef org_small
#    define small org_small
#    undef org_small
#endif
