
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

#include <benchmark/benchmark.h>

#ifdef _WIN32
#    pragma comment( lib, "Shlwapi.lib" )
#    ifndef NDEBUG
#        pragma comment( lib, "benchmark_maind.lib" )
#        pragma comment( lib, "benchmarkd.lib" )
#    else
#        pragma comment( lib, "benchmark_main.lib" )
#        pragma comment( lib, "benchmark.lib" )
#    endif
#endif

#include <sax/splitmix.hpp>
#include <sax/uniform_int_distribution.hpp>
#include "uniformly_decreasing_discrete_distribution_vose.hpp"

// Customization point.
#define USE_MIMALLOC true

#if USE_MIMALLOC
#    if defined( NDEBUG )
#        define USE_MIMALLOC_LTO true
#    else
#        define USE_MIMALLOC_LTO false
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
    using const_iterator  = typename storage_type::const_iterator;

    using pointer       = typename storage_type::pointer;
    using const_pointer = typename storage_type::const_pointer;

    storage_pointer next = nullptr;
    storage_type m_data;

    // Operators new/delete.
    [[nodiscard]] static void * operator new ( std::size_t ) noexcept {
        return MALLOC ( sizeof ( storage ) );
    } // OOM not handled, crash is to be expected.
    static void operator delete ( void * ptr_ ) noexcept { FREE ( ptr_ ); }

    // Factory.
    [[nodiscard]] static storage_pointer make ( ) noexcept { return reinterpret_cast<storage_pointer> ( new storage ); }

    // Iterators.
    [[nodiscard]] iterator begin ( ) noexcept { return std::begin ( m_data ); }
    [[nodiscard]] iterator end ( ) noexcept { return std::end ( m_data ); }
    [[nodiscard]] const_iterator begin ( ) const noexcept { return std::begin ( m_data ); }
    [[nodiscard]] const_iterator end ( ) const noexcept { return std::end ( m_data ); }

    [[nodiscard]] pointer data ( ) noexcept { return m_data.data ( ); }
    [[nodiscard]] const_pointer data ( ) const noexcept { return m_data.data ( ); }
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
    pointer head, tail;

    public:
    queue ( ) noexcept :
        storage_head{ storage::make ( ) }, storage_tail{ storage_head }, head{ storage_head->data ( ) }, tail{ head } {}

    ~queue ( ) noexcept {
        while ( storage_head ) {
            storage_pointer tmp = storage_head->next;
            storage::operator delete ( reinterpret_cast<void *> ( storage_head ) );
            storage_head = std::move ( tmp );
        }
    }

    template<typename... Args>
    void emplace ( Args &&... args_ ) noexcept {
        if ( ( storage_tail->data ( ) + Size ) == tail ) {
            if ( storage_tail->next )
                storage_tail = storage_tail->next;
            else
                storage_tail = ( storage_tail->next = storage::make ( ) );
            tail = storage_tail->data ( );
        }
        *tail++ = { std::forward<Args> ( args_ )... };
    }

    void pop ( ) noexcept {
        if ( ( storage_head->data ( ) + ( Size - 1 ) ) == head ) {
            storage_pointer new_storage_head = storage_head->next;
            storage_head->next               = nullptr;
            // Find an empty next storage slot.
            storage_pointer ptr = storage_tail;
            while ( ptr->next )
                ptr = ptr->next;
            ptr->next = storage_head;
            // Final assignments.
            storage_head = new_storage_head;
            head         = storage_head->data ( );
        }
        else
            ++head;
    }

    [[nodiscard]] reference front ( ) noexcept { return *head; }
    [[nodiscard]] const_reference front ( ) const noexcept { return *head; }

    [[nodiscard]] bool empty ( ) const noexcept { return head == tail; }

    void print_head_tail ( ) const noexcept { std::cout << "head " << head << " tail " << tail << nl; }

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
        pointer curr        = q_.head;
        while ( ptr ) {
            if ( curr == q_.tail ) // Compare the pointer.
                break;
            if constexpr ( std::is_same<typename Stream::char_type, wchar_t>::value ) {
                out_ << *curr << L' ';
            }
            else {
                out_ << *curr << ' ';
            }
            if ( ( ptr->data ( ) + ( Size - 1 ) ) == curr++ ) {
                if ( ptr == q_.storage_tail )
                    break;
                ptr = ptr->next;
                if ( ptr )
                    curr = ptr->data ( );
            }
        }
        return out_;
    }
};

#include <queue>
#include <plf/plf_list.h>
#include <plf/plf_nanotimer.h>

#include <boost/container/deque.hpp>

template<typename T>
using plf_queue = std::queue<T, plf::list<T>>;

template<typename T>
using bst_queue = std::queue<T, boost::container::deque<T>>;

template<typename T>
using std_queue = std::queue<T, std::deque<T>>;

template<typename Stream, typename T>
[[maybe_unused]] Stream & operator<< ( Stream & out_, plf_queue<T> const & q_ ) noexcept {
    plf_queue<T> q{ q_ };
    while ( q.size ( ) ) {
        out_ << q.front ( ) << ' ';
        q.pop ( );
    }
    out_ << nl;
    return out_;
}

sax::splitmix64 rng_1{ 123u }, rng_2{ 123u };

[[nodiscard]] int get_no_ops ( sax::splitmix64 & rng_ ) {
    static uniformly_decreasing_discrete_distribution<32, int> dis;
    return dis ( rng_ ) + 1;
}

enum op { enqueue, dequeue };

op op_1{ op::enqueue }, op_2{ op::enqueue };

std_queue<int> q_3;

bst_queue<int> q_1;
queue<int, 8> q_2;

int s_1 = 0, s_2 = 0;

sax::uniform_int_distribution<int> dis{ 1, 1'000 };

int main899yu ( ) {

    plf::nanotimer t_1;

    t_1.start ( );

    for ( int i = 0; i < 1'000'000; ++i ) {
        int const no_ops = get_no_ops ( rng_1 );
        for ( int o = 0; o < no_ops; ++o ) {
            if ( op::dequeue == op_1 ) { // dequeue.
                if ( not q_1.empty ( ) ) {
                    s_1 += q_1.front ( );
                    q_1.pop ( );
                }
                else {
                    break;
                }
            }
            else { // enqueue.
                q_1.emplace ( dis ( rng_1 ) );
            }
        }
        op_1 = ( op ) ( not op_1 );
    }

    std::int64_t time_1 = ( std::int64_t ) t_1.get_elapsed_ms ( );

    plf::nanotimer t_2;

    t_2.start ( );

    for ( int i = 0; i < 1'000'000; ++i ) {
        int const no_ops = get_no_ops ( rng_2 );
        for ( int o = 0; o < no_ops; ++o ) {
            if ( op::dequeue == op_2 ) { // dequeue.
                if ( not q_2.empty ( ) ) {
                    s_2 += q_2.front ( );
                    q_2.pop ( );
                }
                else {
                    break;
                }
            }
            else { // enqueue.
                q_2.emplace ( dis ( rng_2 ) );
            }
        }
        op_2 = ( op ) ( not op_2 );
    }

    std::int64_t time_2 = ( std::int64_t ) t_2.get_elapsed_ms ( );

    std::cout << time_1 << " ms           " << s_1 << nl;
    std::cout << time_2 << " ms           " << s_2 << nl;

    return EXIT_SUCCESS;
}

using std::string_view_literals::operator""sv ;

class widget {
    const std::string_view name = "I'm a widget"sv;
    friend std::ostream & operator<< ( std::ostream & os, const widget & w );
};

class doodad {
    const std::string_view name = "I'm a doodad"sv;
    friend std::ostream & operator<< ( std::ostream & os, const doodad & d );
};

std::ostream & operator<< ( std::ostream & os, const widget & w ) { return os << w.name; }
std::ostream & operator<< ( std::ostream & os, const doodad & d ) { return os << d.name; }

class experiment {
    public:
    auto get_entity ( ) {
        struct result {
            operator widget ( ) { return experiment->get_entity_as_widget ( ); }
            operator doodad ( ) { return experiment->get_entity_as_doodad ( ); }
            experiment * experiment;
        };
        return result{ this };
    }

    private:
    static widget get_entity_as_widget ( ) { return widget{ }; }
    static doodad get_entity_as_doodad ( ) { return doodad{ }; }
};

int main ( ) {

    queue<int, 4> q;

    q.print_head_tail ( );

    for ( int i = 0; i < 19; ++i ) {
        q.emplace ( i );
        q.print_head_tail ( );
    }

    q.print_storage_pointers ( );
    std::cout << nl << q << nl;

    q.print_head_tail ( );

    for ( int i = 0; i < 25; ++i ) {
        if ( not q.empty ( ) ) {
            q.pop ( );
        }
        else {
            q.emplace ( 123 );
        }
        q.print_head_tail ( );
    }

    q.print_storage_pointers ( );
    std::cout << nl << q << nl;

    return EXIT_SUCCESS;
}

/*
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
*/
