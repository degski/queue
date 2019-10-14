
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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <array>
#include <filesystem>
#include <fstream>
#include <sax/iostream.hpp>
#include <iterator>
#include <list>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

namespace fs = std::filesystem;

#define PRIVATE
#define PUBLIC public:

namespace detail { // keep the macros out of the code (as it's ugly).
#ifdef _DEBUG
using is_debug   = std::true_type;
using is_release = std::false_type;
#else
using is_debug   = std::false_type;
using is_release = std::true_type;
#endif
} // namespace detail

template<int I>
void div ( char ( * )[ I % 2 == 0 ] = 0 ) {
    // this is taken when I is even.
}

template<int I>
void div ( char ( * )[ I % 2 == 1 ] = 0 ) {
    // this is taken when I is odd.
}

// costumization point.
#define USE_MIMALLOC true

#if USE_MIMALLOC
#    if defined( _DEBUG )
#        define USE_MIMALLOC_LTO false
#    else
#        define USE_MIMALLOC_LTO true
#    endif
#    include <mimalloc.h>
#endif

namespace pdr {
#if USE_MIMALLOC
[[nodiscard]] inline void * malloc ( std::size_t size ) noexcept { return mi_malloc ( size ); }
[[nodiscard]] inline void * zalloc ( std::size_t size ) noexcept { return mi_zalloc ( size ); }
[[nodiscard]] inline void * calloc ( std::size_t num, std::size_t size ) noexcept { return mi_calloc ( num, size ); }
[[nodiscard]] inline void * realloc ( void * ptr, std::size_t new_size ) noexcept { return mi_realloc ( ptr, new_size ); }
inline void free ( void * ptr ) noexcept { mi_free ( ptr ); }
#else
[[nodiscard]] inline void * malloc ( std::size_t size ) noexcept { return std::malloc ( size ); }
[[nodiscard]] inline void * zalloc ( std::size_t size ) noexcept { return std::calloc ( 1u, size ); }
[[nodiscard]] inline void * calloc ( std::size_t num, std::size_t size ) noexcept { return std::calloc ( num, size ); }
[[nodiscard]] inline void * realloc ( void * ptr, std::size_t new_size ) noexcept { return std::realloc ( ptr, new_size ); }
inline void free ( void * ptr ) noexcept { std::free ( ptr ); }
#endif
} // namespace pdr

template<typename Type, std::size_t N>
struct block {

    using value_type    = Type;
    using block_type    = std::array<value_type, N>;
    using block_pointer = block *;
    using iterator      = typename block_type::iterator;

    block_pointer next = nullptr;

    private:
    block_type m_data;

    public:
    // Constructor.
    explicit block ( ) noexcept {}
    // Operator new/delete.
    [[nodiscard]] static void * operator new ( std::size_t ) noexcept {
        return pdr::malloc ( sizeof ( block ) );
    } // O-o-m not handled, crash is to be expected.
    static void operator delete ( void * ptr_ ) noexcept { pdr::free ( ptr_ ); }
    // Factory.
    [[nodiscard]] static block_pointer make ( ) noexcept { return reinterpret_cast<block_pointer> ( new block ); }
    // Iterators.
    [[nodiscard]] iterator begin ( ) noexcept { return std::begin ( m_data ); }
    [[nodiscard]] iterator end ( ) noexcept { return std::end ( m_data ); }
};

template<typename Type, std::size_t Size = 16u>
struct queue {

    using block         = block<Type, Size>;
    using block_pointer = block *;

    using value_type    = Type;
    using pointer       = value_type *;
    using const_pointer = value_type const *;

    using reference       = value_type &;
    using const_reference = value_type const &;
    using rv_reference    = value_type &&;

    using size_type = std::size_t;

    using iterator = typename block::iterator;

    block_pointer sto_head, sto_tail;
    iterator head, tail;

    queue ( ) noexcept : sto_head{ block::make ( ) }, sto_tail{ sto_head }, head{ std::begin ( *sto_head ) }, tail{ head } {}

    ~queue ( ) noexcept {
        while ( sto_head ) {
            block_pointer tmp = sto_head->next;
            block::operator delete ( reinterpret_cast<void *> ( sto_head ) );
            sto_head = std::move ( tmp );
        }
    }

    void push ( rv_reference value_ ) noexcept {
        if ( std::end ( *sto_tail ) == tail ) {
            add_storage_to_tail ( );
            tail = std::begin ( *sto_tail );
        }
        *tail++ = std::move ( value_ );
    }

    void push ( const_reference value_ ) noexcept {
        if ( std::end ( *sto_tail ) == tail ) {
            add_storage_to_tail ( );
            tail = std::begin ( *sto_tail );
        }
        *tail++ = value_;
    }

    void pop ( ) noexcept {
        if ( std::end ( *sto_head ) == ++head ) {
            move_storage_head_to_tail ( );
            head = std::begin ( *sto_head );
        }
    }

    [[nodiscard]] value_type front ( ) const noexcept { return *head; }

    [[nodiscard]] bool empty ( ) const noexcept { return head == tail; }

    void print ( ) noexcept {
        block_pointer ptr = sto_head;
        while ( ptr ) {
            std::cout << ptr << ' ' << ptr->next << nl;
            ptr = ptr->next;
        }
        std::cout << nl;
    }

    private:
    [[nodiscard]] block_pointer add_storage_to_tail ( block_pointer tail_ ) noexcept { return tail_->next = block::make ( ); }

    void add_storage_to_tail ( ) noexcept { sto_tail = add_storage_to_tail ( sto_tail ); }

    void move_storage_head_to_tail ( ) noexcept {
        block_pointer new_head = sto_head->next;
        sto_head->next         = nullptr;
        sto_tail->next         = sto_head;
        sto_tail               = sto_head;
        sto_head               = new_head;
    }
};

int main ( ) {

    queue<int, 4> queue;

    queue.push ( 5 );
    queue.push ( 4 );
    queue.push ( 3 );
    queue.push ( 2 );
    queue.push ( 1 );
    queue.push ( 0 );
    queue.push ( -1 );
    queue.push ( -2 );

    for ( auto x = 0; x < 8; ++x ) {
        std::cout << queue.front ( ) << '\n';
        queue.pop ( );
    }

    return EXIT_SUCCESS;
}
