// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, 4Paradigm Inc. */

#ifndef PERSISTENT_SKIPLIST
#define PERSISTENT_SKIPLIST

#include <libpmemobj++/detail/common.hpp>
#include <libpmemobj++/detail/life.hpp>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

#include <numeric>
#include <type_traits>
#include <vector>
#include <chrono>
#include <random>

#include "smartpptr.h"

#define DEBUG_LOG4P

#include <iostream>

namespace pmem
{
namespace kv
{
namespace internal
{

using namespace pmem::obj;

template <typename Key, typename T>
class slnode_t {
public:
	using self_type = slnode_t<Key, T>;
	using key_type = Key;
	using mapped_type = T;
	using size_type = std::size_t;
	using level_type = uint8_t;

	using value_type = std::pair<key_type, mapped_type>;
	using reference = value_type &;
	using const_reference = const value_type &;
	using pointer = value_type *;
	using const_pointer = const value_type *;

	// using slnode_ptr = typename std::conditional<is_const, const slnode_t *, slnode_t *>::type;
	using slnode_ptr = self_type *;
	
	using slnode_pptr = ::fourpd::SmartPPtr<self_type>;
	using atomic_slnode_pptr = std::atomic<slnode_pptr>;

	template <typename K, typename M>
	slnode_t(K &&key, M &&obj, uint8_t height): _ref(1) {
		LOG4P_DEBUG("[YJ]:slnode_t()1:Enter");
		assert(pmemobj_tx_stage() == TX_STAGE_WORK);
		assert(height > 0);
		try {
			_height = height;
			std::cout << "[YJ]:slnode_t()1:_height = " << unsigned(height) << std::endl;
			pointer x = new (&_entry) value_type(std::forward<K>(key), std::forward<M>(obj));
			assert(x == &_entry);
			std::cout << "[YJ]:slnode_t()1:_entry = " << (void*)(&_entry) << " : first = " << _entry.first.data() << ",second = " << _entry.second.data() << std::endl;
			_nexts = make_persistent<atomic_slnode_pptr[]>(height);
			std::cout << "[YJ]:slnode_t()1:_nexts = " << _nexts.get() << std::endl;
		} catch (transaction_error &e) {
			std::terminate();
		}
	}

	slnode_t(uint8_t height): _ref(1) { // for head & tail
		std::cout << "[YJ]:slnode_t()2:Enter" << std::endl;
		assert(pmemobj_tx_stage() == TX_STAGE_WORK);
		try {
			_height = height;
			std::cout << "[YJ]:slnode_t()2:_height = " << unsigned(height) << std::endl;
			if (height > 0) {
				_nexts = make_persistent<atomic_slnode_pptr[]>(height);
				std::cout << "[YJ]:slnode_t()2:_nexts = " << _nexts.get() << std::endl;
			}
		} catch (transaction_error &e) {
			std::cout << "[YJ]:slnode_t()2:transaction_error";
			std::terminate();
		}
	}

	~slnode_t()
	{
		assert(pmemobj_tx_stage() == TX_STAGE_WORK);
		try {
			_entry.first.~key_type();
			_entry.second.~mapped_type();
			delete_persistent<atomic_slnode_pptr[]>(_nexts, _height.get_ro());
		} catch (transaction_error &e) {
			std::terminate();
		}
	}

	uint8_t height() {
		return _height.get_ro();
	}

	const key_type& getKey() {
        return _entry.first;
    }

	reference getValue() {
		return _entry;
	}

	const_reference getValue() const {
		return _entry;
	}

	const bool isTail() {
		return (_height == 0);
	}

	slnode_ptr get_next_ptr(level_type lv) {
		return get_next_pptr(lv).getVptr(pmemobj_pool_by_oid(pmemobj_oid(this)));
	}
	slnode_pptr get_next_pptr(level_type lv) {
		auto expected = _nexts[lv].load(std::memory_order_relaxed);
		auto target = slnode_pptr(expected.getOffset(), false, false);
		while (true) {
			if (expected.isDirty()) {
				if (!(_nexts[lv].compare_exchange_strong(expected, target))) {
					expected = _nexts[lv].load(std::memory_order_relaxed);
					target = slnode_pptr(expected.getOffset(), false, false);
					continue;
				}
				pmemobj_persist(pmemobj_pool_by_oid(pmemobj_oid(this)), &(expected), sizeof(expected));
			}
			break;
		}
		return target;
	}

	void set_next_pptr(level_type lv, const slnode_pptr &node) {
		assert(lv < _height && lv >= 0);
		std::cout << "[YJ]:set_next_pptr:lv = " << unsigned(lv) << ", pptr = " << (void*)(&node) << std::endl;
		_nexts[lv].store(node, std::memory_order_relaxed);
	}

	void pin() {
		_ref++;
	}

	void unpin() {
		_ref--;
	}

private:
	union {
		value_type _entry;
	};
	persistent_ptr<atomic_slnode_pptr[]> _nexts;
	p<uint8_t> _height;
	std::atomic<uint16_t> _ref;
};

template <typename NodeType, bool is_const>
class persistent_skiplist_iterator {
private:
	using slnode_type = NodeType;
	using slnode_ptr = slnode_type *;
	friend class persistent_skiplist_iterator<slnode_type, true>;

	slnode_ptr _current_node;
public:
	using iterator_category = std::forward_iterator_tag;
	using difference_type = ptrdiff_t;
	using value_type = typename slnode_type::value_type;
	using reference = typename slnode_type::reference;
	using pointer = typename slnode_type::pointer;

	persistent_skiplist_iterator(std::nullptr_t)
		: _current_node(nullptr) {}
	persistent_skiplist_iterator(slnode_ptr node)
		: _current_node(node) {}
	persistent_skiplist_iterator(const persistent_skiplist_iterator &other)
		: _current_node(other._current_node) {}
	template <typename T = void, typename = typename std::enable_if<is_const, T>::type>
	persistent_skiplist_iterator(const persistent_skiplist_iterator<slnode_type, false> &other)
		: _current_node(other._current_node) {}

	persistent_skiplist_iterator &operator=(const persistent_skiplist_iterator &other)
	{
		_current_node = other._current_node;
		return *this;
	}

	persistent_skiplist_iterator &operator++()
	{
		_current_node = _current_node->get_next_ptr(0);
		return *this;
	}
	persistent_skiplist_iterator operator++(int)
	{
		persistent_skiplist_iterator tmp = *this;
		++*this;
		return tmp;
	}

	bool operator==(const persistent_skiplist_iterator &other) const
	{
		return _current_node == other._current_node;
	}

	bool operator!=(const persistent_skiplist_iterator &other) const
	{
		return !(*this == other);
	}

	reference operator*() const
	{
		return _current_node->getValue();
	}

	pointer operator->() const
	{
		return &(_current_node->getValue());
	}
};

template <typename Key, typename T, typename Compare, uint8_t Height, uint8_t Branch>
class persistent_skiplist_base {
private:
	using self_type = persistent_skiplist_base<Key, T, Compare, Height, Branch>;
	using slnode_type = slnode_t<Key, T>;
	using key_pptr = persistent_ptr<Key>;
	// using node_pptr = persistent_ptr<slnode_t>;
	using node_pptr = ::fourpd::SmartPPtr<slnode_type>;
	using node_ptr = slnode_type*;
	using atomic_node_pptr = std::atomic<node_pptr>;
public:
	using value_type = typename slnode_type::value_type;
	using key_type = typename slnode_type::key_type;
	using mapped_type = typename slnode_type::mapped_type;
	using key_compare = Compare;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	using reference = typename slnode_type::reference;
	using const_reference = typename slnode_type::const_reference;
	using pointer = typename slnode_type::pointer;
	using const_pointer = typename slnode_type::const_pointer;

	using iterator = persistent_skiplist_iterator<slnode_type, false>;
	using const_iterator = persistent_skiplist_iterator<slnode_type, true>;

	persistent_skiplist_base() : 
		_random((unsigned long)std::chrono::system_clock::now().time_since_epoch().count()) {
		std::cout << "[YJ]:persistent_skiplist_base():Enter" << std::endl;
		assert(pmemobj_tx_stage() == TX_STAGE_WORK);
		_head.store(allocate_node(Height), std::memory_order_relaxed);
		std::cout << "[YJ]:persistent_skiplist_base():_head = " << _head.load().getOffset() << std::endl;
		_tail = allocate_node(0);
		std::cout << "[YJ]:persistent_skiplist_base():_tail = " << _tail.getOffset() << std::endl;
		for (uint8_t i = 0;i < Height;i++) {
			std::cout << "[YJ]:persistent_skiplist_base():_head.load().getVptr() = " << (void*)(_head.load().getVptr(get_objpool())) << std::endl;
			_head.load().getVptr(get_objpool())->set_next_pptr(i, _tail);
		}
		_size = 0;
	}

	~persistent_skiplist_base() {

	}

	template <typename K, typename M>
	std::pair<iterator, bool> try_emplace(K &&key, M &&obj) {
		std::cout << "[YJ]:try_emplace():Enter" << std::endl;
		std::vector<node_ptr> pre(Height);
		std::pair<node_ptr, bool> res = find_less_or_equal(key, pre);
		if (res.second) { //key found
			return std::pair<iterator, bool>(iterator(res.first), false);
		} else {
			return internal_insert(pre, std::forward<K>(key), std::forward<M>(obj));
		}
	}

	template <typename K>
	iterator find(const K &key) {
		std::cout << "[YJ]:find():Enter" << std::endl;
		std::vector<node_ptr> pre(Height);
		std::pair<node_ptr, bool> res = find_less_or_equal(key, pre);
		std::cout << "[YJ]:find():result {node=" << (void*)res.first << ",found=" << res.second << "}" << std::endl;
		return res.second ? 
				iterator(res.first) :
				end();
	}

	template <typename K>
	const_iterator find(const K &key) const {
		std::cout << "[YJ]:const_find():Enter" << std::endl;
		std::vector<node_ptr> pre(Height);
		std::pair<node_ptr, bool> res = find_less_or_equal(key, pre);
		std::cout << "[YJ]:const_find():result {node=" << (void*)res.first << ",found=" << res.second << "}" << std::endl;
		return res.second ? 
				const_iterator(res.first) :
				cend();
	}
	template <typename K>
	iterator lower_bound(const K &key) {
		std::vector<node_ptr> pre(Height);
		std::pair<node_ptr, bool> res = find_less_or_equal(key, pre);
		if (res.second)
			return iterator(res.first);
		else {
			node_ptr next = res.first->get_next_ptr(0);
			if (next->isTail())
				return end();
			else
				return iterator(next);
		}
	}

	template <typename K>
	const_iterator lower_bound(const K &key) const {
		std::vector<node_ptr> pre(Height);
		std::pair<node_ptr, bool> res = find_less_or_equal(key, pre);
		if (res.second)
			return const_iterator(res.first);
		else {
			node_ptr next = res.first->get_next_ptr(0);
			if (next->isTail())
				return cend();
			else
				return const_iterator(next);
		}
	}

	template <typename K>
	iterator upper_bound(const K &key) {
		std::vector<node_ptr> pre(Height);
		std::pair<node_ptr, bool> res = find_less_or_equal(key, pre);
		node_ptr next = res.first->get_next_ptr(0);
		while (!next->isTail()) {
			if (_compare(key, next->getKey()))
				return iterator(next);
			next = next->get_next_ptr(0);
		}
		return end();
	}

	template <typename K>
	const_iterator upper_bound(const K &key) const {
		std::vector<node_ptr> pre(Height);
		std::pair<node_ptr, bool> res = find_less_or_equal(key, pre);
		node_ptr next = res.first->get_next_ptr(0);
		while (!next->isTail()) {
			if (_compare(key, next->getKey()))
				return const_iterator(next);
			next = next->get_next_ptr(0);
		}
		return cend();
	}

	template <typename K>
	size_type erase(const K &key) {
		std::cout << "[YJ]:erase():Enter" << std::endl;
		std::vector<node_ptr> pre(Height);
		std::pair<node_ptr, bool> res = find_less_or_equal(key, pre);
		if (res.second) { //key found
			return internal_erase(pre, res.first);
		} else {
			return size_type(0);
		}
	}
	
	iterator begin() {
		return iterator(_head.load(std::memory_order_relaxed).getVptr(get_objpool())->get_next_ptr(0));
	}
	iterator end() {
		return iterator(_tail.getVptr(get_objpool()));
	}
	const_iterator begin() const {
		return const_iterator(_head.load(std::memory_order_relaxed).getVptr(get_objpool())->get_next_ptr(0));
	}
	const_iterator end() const {
		return const_iterator(_tail.getVptr(get_objpool()));
	}
	const_iterator cbegin() const {
		return begin();
	}
	const_iterator cend() const {
		return end();
	}
	
	/* method */

	size_type size() const noexcept {
		return _size.get_ro();
	}

	reference operator[](size_type pos) {
		node_ptr temp = _head.load(std::memory_order_relaxed).getVptr(get_objpool())->get_next_ptr(0);
		while (!temp->isTail()) {
			if (pos == 0)
				return temp->getValue();
			temp = temp->get_next_ptr(0);
			pos--;
		}
		assert(false);
	}

	const_reference operator[](size_type pos) const {
		node_ptr temp = _head.load(std::memory_order_relaxed).getVptr(get_objpool())->get_next_ptr(0);
		while (!temp->isTail()) {
			if (pos == 0)
				return temp->getValue();
			temp = temp->get_next_ptr(0);
			pos--;
		}
		assert(false);
	}

	key_compare &key_comp() {
		return _compare;
	}
	const key_compare &key_comp() const {
		return _compare;
	}

private:
	atomic_node_pptr _head;
	node_pptr _tail;
	key_compare _compare;
	std::mt19937_64 _random;
	pmem::obj::p<size_type> _size;

	/* helper func */
	template <typename... Args>
	inline node_pptr allocate_node(Args &&... args) {
		auto pptr = make_persistent<slnode_type>(std::forward<Args>(args)...);
		std::cout << "[YJ]:allocate_node(): vptr=" << (void*)pptr.get() << ", " \
				<< "pmemobj_oid={" << pmemobj_oid(pptr.get()).pool_uuid_lo << "," << pmemobj_oid(pptr.get()).off << "} " << std::endl;
		return node_pptr(pptr.raw().off);
	}

	inline void deallocate(node_pptr node) {
		std::cout << "[YJ]:deallocate():Enter" << std::endl;
		assert(node.getVptr(get_objpool()) != nullptr);
		pool_base pop = get_pool_base();
		pmem::obj::transaction::run(pop, [&] {
			delete_persistent<slnode_type>(node.getPptr(get_pool_uuid()));
			// node = nullptr;
		});
	}

	inline uint8_t random_height() {
		uint8_t height = 1;
		std::uniform_int_distribution<uint8_t> dist(0, Branch-1);
        while (height < Height && dist(_random) == 0) {
            height ++;
        }
        return height;
	}

	template <typename K>
	std::pair<node_ptr, bool> find_less_or_equal(const K &key, std::vector<node_ptr> &pre) 
	{
		std::cout << "[YJ]:find_less_or_equal():Enter" << std::endl;

		node_ptr head = _head.load().getVptr(get_objpool());
		node_ptr node = head;
		std::cout << "[YJ]:find_less_or_equal():head = " << (void*)head << std::endl;
		uint8_t level = Height - 1;
		while (true) {
			node_ptr next = node->get_next_ptr(level);
			std::cout << "[YJ]:find_less_or_equal():next[" << unsigned(level) << "]=" << (void*)next << std::endl;
			if (is_after_node(key, next)) {
				std::cout << "[YJ]:find_less_or_equal():is_after_node = TRUE" << std::endl;
				node = next;
			} else {
				std::cout << "[YJ]:find_less_or_equal():is_after_node = FALSE" << std::endl;
				pre[level] = node;
				if (level == 0) {
					if (next->isTail()) {
						std::cout << "[YJ]:find_less_or_equal():x key=" << key.data() << ", next= tail" << std::endl;
						return ((node != head) && (!_compare(key, node->getKey()) && !_compare(node->getKey(), key))) ?
							std::pair<node_ptr, bool>(node, true) :
							std::pair<node_ptr, bool>(node, false);
					}
					else if (!_compare(key, next->getKey()) && !_compare(next->getKey(), key)) {
						std::cout << "[YJ]:find_less_or_equal():y key=" << key.data() << ", next=" << (void*)next << "->key=" << next->getKey().data() << std::endl;
						return std::pair<node_ptr, bool>(next, true);
					} else {
						std::cout << "[YJ]:find_less_or_equal():z key=" << key.data() << ", node=" << (void*)node << std::endl;
						return ((node != head) && (!_compare(key, node->getKey()) && !_compare(node->getKey(), key))) ?
							std::pair<node_ptr, bool>(node, true) :
							std::pair<node_ptr, bool>(node, false);
					}
				}
				level--;
			}
		}
	}

	template <typename K, typename M>
	std::pair<iterator, bool> internal_insert(std::vector<node_ptr> &pre, K &&key, M &&obj) {
		std::cout << "[YJ]:internal_insert():Enter" << std::endl;
		auto pop = get_pool_base();

		node_pptr newNode;
		uint8_t height = random_height();
		std::cout << "[YJ]:internal_insert():random_height=" << unsigned(height) << std::endl;
		pmem::obj::transaction::run(pop, [&] {
			newNode = allocate_node(std::forward<K>(key), std::forward<M>(obj), height);
		});
		for (auto i = 0; i < height; i++) {
			std::cout << "[YJ]:internal_insert():pre[" << unsigned(i) << "]=" << (void*)(pre[i]) << ", ->next_pptr=" << pre[i]->get_next_pptr(i).getOffset() << std::endl;
			newNode.getVptr(get_objpool())->set_next_pptr(i, pre[i]->get_next_pptr(i));
			std::cout << "[YJ]:internal_insert():set newNode->next_pptr[" << unsigned(i) << "]=" << newNode.getVptr(get_objpool())->get_next_pptr(i).getOffset() << std::endl;
			pre[i]->set_next_pptr(i, newNode);
			std::cout << "[YJ]:internal_insert():pre[" << unsigned(i) << "]=" << (void*)(pre[i]) << ", ->next_pptr=" << pre[i]->get_next_pptr(i).getOffset() << std::endl;
		}
		_size++;
		return std::pair<iterator, bool>(iterator(newNode.getVptr(get_objpool())), true);
	}

	size_type internal_erase(std::vector<node_ptr> &pre, node_ptr node) {
		std::cout << "[YJ]:internal_erase():Enter" << std::endl;
		auto pop = get_pool_base();
		auto target = pre[0]->get_next_pptr(0);
		std::cout << "[YJ]:internal_erase():pre[0]=" << (void*)pre[0] << ", ->next_pptr=" << pre[0]->get_next_pptr(0).getOffset() << std::endl;
		for (auto i = node->height()-1; i >= 0; i--) {
			std::cout << "[YJ]:internal_erase():pre[" << unsigned(i) << "]=" << (void*)(pre[i]) << ", ->next_pptr=" << pre[0]->get_next_pptr(i).getOffset() << std::endl;
			pre[i]->set_next_pptr(i, node->get_next_pptr(i));
			std::cout << "[YJ]:internal_erase():set pre[" << unsigned(i) << "]=" << (void*)(pre[i]) << ", ->next_pptr=" << node->get_next_pptr(i).getOffset() << std::endl;
		}
		deallocate(target);
		_size--;
		return 1;
	}

	PMEMobjpool *get_objpool() {
		PMEMoid oid = pmemobj_oid(this);
		return pmemobj_pool_by_oid(oid);
	}

	uint64_t get_pool_uuid() {
		PMEMoid oid = pmemobj_oid(this);
		return oid.pool_uuid_lo;
	}

	pool_base get_pool_base() {
		return pool_base(get_objpool());
	}
	template <typename K>
	bool is_after_node(const K& key, node_ptr node) const {
        return (!node->isTail()) && (_compare(node->getKey(), key));
    }
};

} /* namespace internal */

template <typename Key, typename Value, typename Compare = std::less<Key>,
	  std::size_t height = 8, std::size_t branch = 4>
class persistent_skiplist : public internal::persistent_skiplist_base<Key, Value, Compare, height, branch> {
private:
	using base_type = internal::persistent_skiplist_base<Key, Value, Compare, height, branch>;

public:
	using base_type::begin;
	using base_type::end;
	using base_type::erase;
	using base_type::find;
	using base_type::try_emplace;

	/* type definitions */
	using key_type = typename base_type::key_type;
	using mapped_type = typename base_type::mapped_type;
	using value_type = typename base_type::value_type;
	using iterator = typename base_type::iterator;
	using const_iterator = typename base_type::const_iterator;
	// using reverse_iterator = typename base_type::reverse_iterator;

	explicit persistent_skiplist() : base_type()
	{
		std::cout << "[YJ]:persistent_skiplist():Enter" << std::endl;
	}

	~persistent_skiplist()
	{
	}

	persistent_skiplist(const persistent_skiplist &) = delete;
	persistent_skiplist &operator=(const persistent_skiplist &) = delete;
};

} // namespace kv
} // namespace persistent
#endif // PERSISTENT_SKIPLIST
