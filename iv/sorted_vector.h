#ifndef IV_SORTED_VECTOR_H_
#define IV_SORTED_VECTOR_H_
#include <vector>
#include <functional>
namespace iv {
namespace core {

// prevent upcase, so using protected inheritance
template<
  typename T,
  typename Compare=std::less<T>,
  typename Alloc=std::allocator<T> >
class SortedVector : protected std::vector<T, Alloc> {
 public:
  typedef std::vector<T, Alloc> container_type;
  typedef const container_type const_container_type;
  typedef typename container_type::iterator iterator;
  typedef typename container_type::const_iterator const_iterator;
  typedef typename container_type::reverse_iterator reverse_iterator;
  typedef typename container_type::const_reverse_iterator const_reverse_iterator;
  typedef typename container_type::pointer pointer;
  typedef typename container_type::const_pointer const_pointer;
  typedef typename container_type::reference reference;
  typedef typename container_type::const_reference const_reference;
  typedef typename container_type::size_type size_type;
  typedef typename container_type::difference_type difference_type;
  typedef typename container_type::value_type value_type;
  typedef typename container_type::allocator_type allocator_type;
  typedef Compare key_compare;
  using const_container_type::begin;
  using const_container_type::end;
  using const_container_type::rbegin;
  using const_container_type::rend;
  using const_container_type::operator[];
  using const_container_type::at;
  using const_container_type::front;
  using const_container_type::back;
  using container_type::erase;
  using container_type::size;
  using container_type::max_size;
  using container_type::capacity;
  using container_type::empty;
  using container_type::reserve;
  using container_type::clear;
  using container_type::get_allocator;

  void insert(const T& val) {
    container_type::insert(
        std::upper_bound(container_type::begin(),
                         container_type::end(),
                         val, Compare()), val);
  }

  void push_back(const T& val) {
    insert(val);
  }
};

} }  // namespace iv::core
#endif  // IV_SORTED_VECTOR_H_