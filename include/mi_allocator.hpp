
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

#include <cassert>
#include <cstdint>
#include <cstdlib>

#include <memory>
#include <new>
#include <type_traits>

#include <mimalloc.h>

namespace sax {

template<class T>
class mi_allocator {
    public:
    using value_type                             = T;
    using difference_type                        = typename std::pointer_traits<value_type *>::difference_type;
    using size_type                              = std::make_unsigned_t<difference_type>;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal                        = std::true_type;

    mi_allocator ( ) noexcept                      = default;
    mi_allocator ( mi_allocator const & ) noexcept = default;
    template<class U>
    mi_allocator ( mi_allocator<U> const & ) noexcept {};

    [[nodiscard]] value_type * allocate ( std::size_t n_ ) {
        if constexpr ( alignof ( value_type ) <= sizeof ( void * ) ) {
            return static_cast<value_type *> ( operator new ( n_ * sizeof ( value_type ) ) );
        }
        else {
            return static_cast<value_type *> ( operator new ( n_ * sizeof ( value_type ),
                                                              static_cast<std::align_val_t> ( alignof ( value_type ) ) ) );
        }
    }

    void deallocate ( value_type * ptr_, std::size_t ) noexcept {
        if constexpr ( alignof ( value_type ) <= sizeof ( void * ) ) {
            operator delete ( ptr_ );
        }
        else {
            operator delete ( ptr_, static_cast<std::align_val_t> ( alignof ( value_type ) ) );
        }
    }

    private:
    // Pointer alignment, assumes C++20 two's-complement.
    [[nodiscard]] static constexpr std::size_t pointer_alignment ( void const * ptr_ ) noexcept {
        return static_cast<std::size_t> ( reinterpret_cast<std::uintptr_t> ( ptr_ ) &
                                          static_cast<std::uintptr_t> ( -reinterpret_cast<std::intptr_t> ( ptr_ ) ) );
    }

    // Unaligned.

    [[nodiscard]] void * operator new ( std::size_t n_size_ ) {
        auto ptr = mi_malloc ( n_size_ );
        assert ( pointer_alignment ( ptr ) >= alignof ( value_type ) );
        if ( ptr )
            return ptr;
        else
            throw std::bad_alloc{};
    }

    void operator delete ( void * ptr_ ) noexcept { mi_free ( ptr_ ); }

    // Aligned.

    [[nodiscard]] void * operator new ( std::size_t n_size_, std::align_val_t align_ ) {
        auto ptr = mi_malloc_aligned ( n_size_, static_cast<std::size_t> ( align_ ) );
        if ( ptr )
            return ptr;
        else
            throw std::bad_alloc{};
    }

    void operator delete ( void * ptr_, std::align_val_t align_ ) noexcept {
        mi_free_aligned ( ptr_, static_cast<std::size_t> ( align_ ) );
    }
};

} // namespace sax

template<class T, class U>
[[nodiscard]] bool operator== ( sax::mi_allocator<T> const &, sax::mi_allocator<U> const & ) noexcept {
    return true;
}

template<class T, class U>
[[nodiscard]] bool operator!= ( sax::mi_allocator<T> const &, sax::mi_allocator<U> const & ) noexcept {
    return false;
}
