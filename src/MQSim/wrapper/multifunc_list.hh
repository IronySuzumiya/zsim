#ifndef MULTIFUNC_LIST_H
#define MULTIFUNC_LIST_H

#include <random>
#include <unordered_map>

namespace FlashGNN {

template<typename key_t, typename entry_t = key_t, typename hash_t = std::hash<key_t>>
struct MultifuncList {
  struct entry_with_key_t {
    key_t key;
    entry_t entry;
  };

  typedef std::list<entry_with_key_t*> list_t;
  typedef std::unordered_map<key_t, typename list_t::iterator, hash_t> mapping_t;

  list_t list;
  mapping_t mapping;

  size_t _capacity;
  std::default_random_engine rand_eng;

  bool empty() const {
    return size() == 0;
  }

  bool full() const {
    assert(size() <= capacity());
    return size() == capacity();
  }

  size_t size() const {
    assert(list.size() == mapping.size());
    return list.size();
  }

  size_t capacity() const {
    return _capacity;
  }

  void set_capacity(size_t capacity) {
    _capacity = capacity;
  }

  void set_rand_eng(uint32_t seed) {
    rand_eng = std::default_random_engine(seed);
  }

  void push_front(const key_t& key, const entry_t& entry = {}) {
    assert(mapping.find(key) == mapping.end());
    list.push_front(new entry_with_key_t { key, entry });
    mapping.insert(std::make_pair(key, list.begin()));
  }

  void push_back(const key_t& key, const entry_t& entry = {}) {
    assert(mapping.find(key) == mapping.end());
    list.push_back(new entry_with_key_t { key, entry });
    mapping.insert(std::make_pair(key, std::prev(list.end())));
  }

  void insert(typename list_t::iterator it, const key_t& key, const entry_t& entry = {}) {
    assert(mapping.find(key) == mapping.end());
    auto pos = list.insert(it, new entry_with_key_t { key, entry });
    mapping.insert(std::make_pair(key, pos));
  }

  bool hit(const key_t& key) const {
    return mapping.find(key) != mapping.end();
  }

  entry_t& get(const key_t& key) const {
    assert(hit(key));
    return (*mapping.find(key)->second)->entry;
  }

  entry_t& get(const key_t& key) {
    assert(hit(key));
    return (*mapping.find(key)->second)->entry;
  }

  void move_to_front(const key_t& key) {
    auto&& entry_it = mapping.find(key);
    assert(entry_it != mapping.end());
    if(entry_it->second != list.begin()) {
      auto val = *entry_it->second;
      list.erase(entry_it->second);
      list.push_front(val);
      entry_it->second = list.begin();
    }
  }

  void move_to_back(const key_t& key) {
    auto&& entry_it = mapping.find(key);
    assert(entry_it != mapping.end());
    if(entry_it->second != std::prev(list.end())) {
      auto val = *entry_it->second;
      list.erase(entry_it->second);
      list.push_back(val);
      entry_it->second = std::prev(list.end());
    }
  }

  entry_with_key_t* front() const {
    assert(!list.empty() && list.size() == mapping.size());
    return list.front();
  }

  entry_with_key_t* front() {
    assert(!list.empty() && list.size() == mapping.size());
    return list.front();
  }

  entry_with_key_t* back() const {
    assert(!list.empty() && list.size() == mapping.size());
    return list.back();
  }

  entry_with_key_t* back() {
    assert(!list.empty() && list.size() == mapping.size());
    return list.back();
  }

  void pop_back() {
    assert(!list.empty() && list.size() == mapping.size());
    const key_t key = list.back()->key;
    delete list.back();
    list.pop_back();
    auto&& entry_it = mapping.find(key);
    assert(entry_it != mapping.end());
    mapping.erase(entry_it);
  }

  void pop_front() {
    assert(!list.empty() && list.size() == mapping.size());
    const key_t key = list.front()->key;
    delete list.front();
    list.pop_front();
    auto&& entry_it = mapping.find(key);
    assert(entry_it != mapping.end());
    mapping.erase(entry_it);
  }

  entry_with_key_t* get_rand() const {
    assert(!list.empty() && list.size() == mapping.size());
    std::uniform_int_distribution<size_t> pos(0, list.size() - 1);
    size_t rand_val = pos(rand_eng);
    auto entry_it = mapping.begin();
    std::advance(entry_it, rand_val);
    assert(entry_it != mapping.end());
    return *entry_it->second;
  }

  entry_with_key_t* get_rand() {
    assert(!list.empty() && list.size() == mapping.size());
    std::uniform_int_distribution<size_t> pos(0, list.size() - 1);
    size_t rand_val = pos(rand_eng);
    auto entry_it = mapping.begin();
    std::advance(entry_it, rand_val);
    assert(entry_it != mapping.end());
    return *entry_it->second;
  }

  void kickout_rand() {
    assert(!list.empty() && list.size() == mapping.size());
    std::uniform_int_distribution<size_t> pos(0, list.size() - 1);
    size_t rand_val = pos(rand_eng);
    auto entry_it = mapping.begin();
    std::advance(entry_it, rand_val);
    assert(entry_it != mapping.end());
    delete *entry_it->second;
    list.erase(entry_it->second);
    mapping.erase(entry_it);
  }
  
  void erase(const key_t& key) {
    auto entry_it = mapping.find(key);
    assert(entry_it != mapping.end());
    delete *entry_it->second;
    list.erase(entry_it->second);
    mapping.erase(entry_it);
  }

  void clear() {
    for(typename list_t::iterator it = list.begin(); it != list.end(); ++it) delete *it;
    list.clear();
    mapping.clear();
  }

  MultifuncList(size_t capacity = 0, uint32_t seed = 0) : _capacity(capacity), rand_eng(std::default_random_engine(seed)) {}
  ~MultifuncList() {
    clear();
  }
};

template<typename key_t, typename entry_t = key_t>
struct MultifuncVec {
  struct entry_with_key_t {
    key_t key;
    entry_t entry;
  };

  typedef std::vector<entry_with_key_t*> vec_t;
  typedef std::unordered_map<key_t, size_t> mapping_t;

  vec_t vec;
  mapping_t mapping;

  std::default_random_engine rand_eng;

  size_t size() const {
    assert(vec.size() == mapping.size());
    return vec.size();
  }

  void set_rand_eng(uint32_t seed) {
    rand_eng = std::default_random_engine(seed);
  }

  void push_back(const key_t& key, const entry_t& entry = {}) {
    assert(mapping.find(key) == mapping.end());
    vec.push_back(new entry_with_key_t { key, entry });
    mapping.insert(std::make_pair(key, vec.size() - 1));
  }

  bool hit(const key_t& key) const {
    return mapping.find(key) != mapping.end();
  }

  entry_t& get(const key_t& key) const {
    assert(hit(key));
    return vec.at(mapping.find(key)->second)->entry;
  }

  entry_t& get(const key_t& key) {
    assert(hit(key));
    return vec.at(mapping.find(key)->second)->entry;
  }

  entry_with_key_t* front() const {
    assert(!vec.empty() && vec.size() == mapping.size());
    return vec.front();
  }

  entry_with_key_t* front() {
    assert(!vec.empty() && vec.size() == mapping.size());
    return vec.front();
  }

  entry_with_key_t* back() const {
    assert(!vec.empty() && vec.size() == mapping.size());
    return vec.back();
  }

  entry_with_key_t* back() {
    assert(!vec.empty() && vec.size() == mapping.size());
    return vec.back();
  }

  void clear() {
    for(typename vec_t::iterator it = vec.begin(); it != vec.end(); ++it) delete *it;
    vec.clear();
    mapping.clear();
  }

  MultifuncVec(uint32_t seed = 0) : rand_eng(std::default_random_engine(seed)) {}
  ~MultifuncVec() {
    clear();
  }
};

};

#endif
