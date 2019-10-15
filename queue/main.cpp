
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
#include <sax/iostream.hpp>
#include <iterator>
#include <random>
#include <type_traits>
#include <vector>

// Customization point.
#define USE_MIMALLOC true

#if USE_MIMALLOC
#    if defined( _DEBUG )
#        define USE_MIMALLOC_LTO false
#    else
#        define USE_MIMALLOC_LTO true
#    endif
#    include <mimalloc.h>
#    define MALLOC mi_malloc
#    define FREE mi_free
#else
#    define MALLOC std::malloc
#    define FREE std::free
#endif

namespace detail {
template<typename Type, std::size_t Size>
struct storage {

    using value_type      = Type;
    using storage_type    = std::array<value_type, Size>;
    using storage_pointer = storage *;
    using iterator        = typename storage_type::iterator;

    storage_pointer next = nullptr;
    storage_type data;

    // Operators new/delete.
    [[nodiscard]] static void * operator new ( std::size_t ) noexcept {
        return MALLOC ( sizeof ( storage ) );
    } // OOM not handled, crash is to be expected.
    static void operator delete ( void * ptr_ ) noexcept { FREE ( ptr_ ); }

    // Factory.
    [[nodiscard]] static storage_pointer make ( ) noexcept { return reinterpret_cast<storage_pointer> ( new storage ); }

    // Iterators.
    [[nodiscard]] iterator begin ( ) noexcept { return std::begin ( data ); }
    [[nodiscard]] iterator end ( ) noexcept { return std::end ( data ); }
};
} // namespace detail

#undef MALLOC
#undef FREE

template<typename Type, std::size_t Size = 16u>
class queue {

    using storage         = detail::storage<Type, Size>;
    using storage_pointer = storage *;

    using value_type    = Type;
    using pointer       = value_type *;
    using const_pointer = value_type const *;

    using reference       = value_type &;
    using const_reference = value_type const &;
    using rv_reference    = value_type &&;

    using size_type = std::size_t;

    using iterator = typename storage::iterator;

    storage_pointer storage_head, storage_tail;
    iterator head, tail;

    public:
    queue ( ) noexcept :
        storage_head{ storage::make ( ) }, storage_tail{ storage_head }, head{ storage_head->begin ( ) }, tail{ head } {}

    ~queue ( ) noexcept {
        while ( storage_head ) {
            storage_pointer tmp = storage_head->next;
            storage::operator delete ( reinterpret_cast<void *> ( storage_head ) );
            storage_head = std::move ( tmp );
        }
    }

    template<typename... Args>
    void emplace ( Args &&... args_ ) noexcept {
        if ( storage_tail->end ( ) == tail ) {
            if ( storage_tail->next )
                storage_tail = storage_tail->next;
            else
                storage_tail = storage_tail->next = storage::make ( );
            tail = storage_tail->begin ( );
        }
        *tail++ = { std::forward<Args> ( args_ )... };
    }

    void push ( rv_reference value_ ) noexcept {
        if ( storage_tail->end ( ) == tail ) {
            if ( storage_tail->next )
                storage_tail = storage_tail->next;
            else
                storage_tail = storage_tail->next = storage::make ( );
            tail = storage_tail->begin ( );
        }
        *tail++ = std::move ( value_ );
    }
    void push ( const_reference value_ ) noexcept {
        if ( storage_tail->end ( ) == tail ) {
            if ( storage_tail->next )
                storage_tail = storage_tail->next;
            else
                storage_tail = storage_tail->next = storage::make ( );
            tail = storage_tail->begin ( );
        }
        *tail++ = value_;
    }

    void pop ( ) noexcept {
        if ( storage_head->end ( ) == ++head ) {
            storage_pointer new_storage_head = storage_head->next;
            storage_head->next               = nullptr;
            // Find an empty next storage slot.
            storage_pointer ptr_next = storage_tail->next;
            while ( ptr_next )
                ptr_next = ptr_next->next;
            ptr_next = storage_head;
            // Final assignments.
            storage_head = new_storage_head;
            head         = storage_head->begin ( );
        }
    }

    [[nodiscard]] reference front ( ) noexcept { return *head; }
    [[nodiscard]] const_reference front ( ) const noexcept { return *head; }

    [[nodiscard]] bool empty ( ) const noexcept { return &*head == &*tail; }

    void print_storage_pointers ( ) const noexcept {
        storage_pointer ptr = storage_head;
        while ( ptr ) {
            std::cout << ptr << ' ' << ptr->next << nl;
            ptr = ptr->next;
        }
        std::cout << nl;
    }

    template<typename Stream>
    [[maybe_unused]] friend Stream & operator<< ( Stream & out_, queue const & q_ ) noexcept {
        storage_pointer ptr = q_.storage_head;
        iterator curr       = q_.head;
        while ( ptr ) {
            if ( &*curr == &*q_.tail ) // Could be iterators in different containers, so compare the underlying pointer.
                break;
            if constexpr ( std::is_same<typename Stream::char_type, wchar_t>::value ) {
                out_ << *curr << L' ';
            }
            else {
                out_ << *curr << ' ';
            }
            if ( ptr->end ( ) == ++curr ) {
                if ( ptr == q_.storage_tail )
                    break;
                ptr = ptr->next;
                if ( ptr )
                    curr = ptr->begin ( );
            }
        }
        return out_;
    }
};

int main ( ) {

    queue<int, 4> queue;

    queue.push ( 6 );
    queue.push ( 5 );
    queue.push ( 4 );
    queue.push ( 3 );
    queue.push ( 2 );
    queue.push ( 1 );
    queue.push ( 0 );
    queue.push ( -1 );
    queue.emplace ( -2 );

    std::cout << queue.empty ( ) << ' ' << queue << nl;

    return EXIT_SUCCESS;
}
