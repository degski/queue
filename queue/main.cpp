
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

// https://codereview.stackexchange.com/a/197739/194172

template<typename Type, std::size_t N>
struct block {

    using value_type             = Type;
    using pointer                = value_type *;
    using const_pointer          = value_type const *;
    using reference              = value_type &;
    using const_reference        = value_type const &;
    using block_type             = std::array<value_type, N>;
    using block_pointer          = block *;
    using iterator               = typename block_type::iterator;
    using const_iterator         = typename block_type::const_iterator;
    using reverse_iterator       = typename block_type::reverse_iterator;
    using const_reverse_iterator = typename block_type::const_reverse_iterator;

    block *prev, *next;

    private:
    block_type m_data;

    public:
    // Constructor.
    explicit block ( block_pointer ptr_ ) : prev{ nullptr }, next{ ptr_ } {}
    // Operator new/delete.
    [[nodiscard]] static void * operator new ( std::size_t ) noexcept {
        return pdr::malloc ( sizeof ( block ) );
    } // O-o-m not handled, crash is to be expected.
    static void operator delete ( void * ptr_ ) noexcept { pdr::free ( ptr_ ); }
    // Factory.
    [[nodiscard]] static block_pointer make ( block_pointer ptr_ = nullptr ) noexcept {
        return reinterpret_cast<block_pointer> ( new block ( ptr_ ) );
    }
    // Front.
    [[nodiscard]] reference front ( ) noexcept { return m_data[ 0 ]; }
    [[nodiscard]] const_reference front ( ) const noexcept { return m_data[ 0 ]; }
    // Back.
    [[nodiscard]] reference back ( ) noexcept { return m_data[ N - 1 ]; }
    [[nodiscard]] const_reference back ( ) const noexcept { return m_data[ N - 1 ]; }
    // Data.
    [[nodiscard]] pointer data ( ) noexcept { return m_data.m_data ( ); }
    [[nodiscard]] const_pointer data ( ) const noexcept { return m_data.m_data ( ); }
    // Iterators.
    [[nodiscard]] iterator begin ( ) noexcept { return std::begin ( m_data ); }
    [[nodiscard]] const_iterator begin ( ) const noexcept { return std::begin ( m_data ); }
    [[nodiscard]] const_iterator cbegin ( ) const noexcept { return std::cbegin ( m_data ); }

    [[nodiscard]] iterator end ( ) noexcept { return std::end ( m_data ); }
    [[nodiscard]] const_iterator end ( ) const noexcept { return std::end ( m_data ); }
    [[nodiscard]] const_iterator cend ( ) const noexcept { return std::cend ( m_data ); }

    [[nodiscard]] reverse_iterator rbegin ( ) noexcept { return std::rbegin ( m_data ); }
    [[nodiscard]] const_reverse_iterator rbegin ( ) const noexcept { return std::rbegin ( m_data ); }
    [[nodiscard]] const_reverse_iterator crbegin ( ) const noexcept { return std::crbegin ( m_data ); }

    [[nodiscard]] reverse_iterator rend ( ) noexcept { return std::rend ( m_data ); }
    [[nodiscard]] const_reverse_iterator rend ( ) const noexcept { return std::rend ( m_data ); }
    [[nodiscard]] const_reverse_iterator crend ( ) const noexcept { return std::crend ( m_data ); }
};

/*
template<typename Type, std::size_t N = 5>
class queue {

    using block         = block<Type, N>;
    using block_pointer = block *;
    using iterator      = typename block::iterator;

    public:
    static_assert ( std::is_trivially_copyable<Type>::value, "Type must be trivially copyable!" );

    using value_type          = Type;
    using optional_value_type = std::optional<value_type>;
    using pointer             = value_type *;
    using const_pointer       = value_type const *;

    using reference       = value_type &;
    using const_reference = value_type const &;
    using rv_reference    = value_type &&;

    using size_type        = std::size_t;
    using signed_size_type = typename std::make_signed<size_type>::type;
    using difference_type  = signed_size_type;

    using iterator               = typename block::iterator;
    using const_iterator         = typename block::const_iterator;
    using reverse_iterator       = typename block::reverse_iterator;
    using const_reverse_iterator = typename block::const_reverse_iterator;


    [[nodiscard]] void * operator new ( std::size_t n_size_ ) {
        auto ptr = pdr::malloc ( n_size_ );
        if ( ptr )
            return ptr;
        else
            throw std::bad_alloc{};
    }

    void operator delete ( void * ptr_ ) noexcept { pdr::free ( ptr_ ); }


    block_pointer new_block ( ) {
        auto b = static_cast<block_pointer> ( block::operator new );
        m_data   = b;
        return b;
    }

    queue ( ) {
        m_data.emplace_back ( );
        tail = head = std::begin ( *std::begin ( m_data ) );
    }

    void push ( rv_reference value ) {
        if ( std::end ( *std::rbegin ( m_data ) ) == tail ) {
            std::cout << "requiring new storage (emplace)" << '\n';
            m_data.emplace_back ( );
            tail = std::begin ( *std::rbegin ( m_data ) );
        }
        *tail++ = std::move ( value );
    }

    void push ( const_reference value ) {
        if ( std::end ( *std::rbegin ( m_data ) ) == tail ) {
            std::cout << "requiring new storage (push)" << '\n';
            m_data.emplace_back ( );
            tail = std::begin ( *std::rbegin ( m_data ) );
        }
        *tail++ = value;
    }

    void pop ( ) noexcept {
        if ( std::end ( *std::begin ( m_data ) ) == ++head ) {
            std::cout << "freeing unused memory" << '\n';
            m_data.pop_front ( );
            head = std::begin ( *std::begin ( m_data ) );
        }
    }

    value_type front ( ) const noexcept { return *head; }

    private:
    block_pointer m_data = nullptr, free = nullptr;
    iterator head, tail;
};
*/

template<typename Type, std::size_t Size = 16u>
struct dll {

    using block         = block<Type, Size>;
    using block_pointer = block *;
    using iterator      = typename block::iterator;

    block_pointer head, tail;

    dll ( ) noexcept : head{ block::make ( ) }, tail{ head } {}

    ~dll ( ) noexcept {
        while ( head ) {
            block_pointer tmp = head->next;
            block::operator delete ( reinterpret_cast<void *> ( head ) );
            head = tmp;
        }
    }

    [[nodiscard]] block_pointer push ( block_pointer head_ ) noexcept { return head_->prev = block::make ( head_ ); }

    void push ( ) noexcept { head = push ( head ); }

    void print_block ( block_pointer ptr_ ) noexcept { std::cout << ptr_ << ' ' << ptr_->prev << ' ' << ptr_->next << nl; }

    void print ( ) noexcept {
        block_pointer ptr = head;
        while ( ptr ) {
            print_block ( ptr );
            ptr = ptr->next;
        }
        std::cout << nl;
    }

    void move_tail_to_head ( ) noexcept {
        block_pointer new_tail = tail->prev;
        // New sentinels.
        new_tail->next = nullptr;
        tail->prev     = nullptr;
        // Move to front.
        tail->next = head;
        head->prev = tail;
        // Final assignments.
        head = tail;
        tail = new_tail;
    }

    void move_head_to_tail ( ) noexcept {
        block_pointer new_head = head->next;
        // New sentinels.
        new_head->prev = nullptr;
        head->next     = nullptr;
        // Move to back.
        head->prev = tail;
        tail->next = head;
        // Final assignments.
        tail = head;
        head = new_head;
    }
};

int main ( ) {

    dll<int> l1;

    l1.push ( );
    l1.push ( );
    l1.push ( );
    l1.push ( );

    l1.print ( );

    l1.move_tail_to_head ( );

    l1.print ( );

    l1.move_head_to_tail ( );

    l1.print ( );

    /*
    queue<int> queue;
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
    */

    return EXIT_SUCCESS;
}

template<std::size_t N>
struct MyAllocator {

    char m_data[ N ];
    void * p;
    std::size_t sz;

    MyAllocator ( ) : p ( m_data ), sz ( N ) {}

    template<typename T>
    T * aligned_alloc ( std::size_t a = alignof ( T ) ) {
        if ( std::align ( a, sizeof ( T ), p, sz ) ) {
            T * result = reinterpret_cast<T *> ( p );
            p          = ( char * ) p + sizeof ( T );
            sz -= sizeof ( T );
            return result;
        }
        return nullptr;
    }
};

int main0809 ( ) {

    MyAllocator<64> a;

    // allocate a char
    char * p1 = a.aligned_alloc<char> ( );
    if ( p1 )
        *p1 = 'a';
    std::cout << "allocated a char at " << ( void * ) p1 << '\n';

    // allocate an int
    int * p2 = a.aligned_alloc<int> ( );
    if ( p2 )
        *p2 = 1;
    std::cout << "allocated an int at " << ( void * ) p2 << '\n';

    // allocate an int, aligned at 32-byte boundary
    int * p3 = a.aligned_alloc<int> ( 32 );
    if ( p3 )
        *p3 = 2;
    std::cout << "allocated an int at " << ( void * ) p3 << " (32 byte alignment)\n";

    return EXIT_SUCCESS;
}
