#ifndef ADS_SET_H
#define ADS_SET_H

#include <functional>
#include <algorithm>
#include <iostream>
#include <stdexcept>


template <typename Key, size_t N = 13>
class ADS_set {
public:
  class Iterator;
  using value_type = Key;
  using key_type = Key;
  using reference = key_type &;
  using const_reference = const key_type &;
  using size_type = size_t;
  using difference_type = std::ptrdiff_t;
  using iterator = Iterator;
  using const_iterator = Iterator;
  using key_compare = std::less<key_type>;   // B+-Tree
  using key_equal = std::equal_to<key_type>; // Hashing
  using hasher = std::hash<key_type>;        // Hashing

private:

	struct bucket {
		key_type *array {new key_type[N]};
		bucket *overflow_bucket {nullptr};
		size_type bucket_curr_size {0};
		
		bool bucket_isfull () const { return bucket_curr_size == N; }
		bool bucket_empty () const { return bucket_curr_size == 0; }
		key_type *find_in_bucket (const key_type &key) const;
		std::ostream& dump_bucket (std::ostream &o = std::cerr) const;
	};

	bucket **table {nullptr};
	size_type d {0};
	size_type next_to_split {0};

	size_type table_size {0};
	size_type curr_size {0};
	size_type real_table_size {0};
	
	size_type h (const key_type &key) const {
		size_type idx {hasher{}(key) % (1 << d)};
		if (idx < next_to_split) idx = hasher{}(key) % (1 << (d+1));
		return idx;
	}

	void insert_ (const key_type &key, bool is_allowed_to_call_split = true);
	std::pair<bucket*,key_type*> find_ (const key_type &key) const;	
	void split ();
	void rehash_keys (bucket *bucket_to_rehash);
		

public:

  ADS_set (size_type d = 0, size_type table_size = 0);				
		
  ADS_set (std::initializer_list<key_type> ilist) : ADS_set{} { insert(ilist); }
  template<typename InputIt> ADS_set (InputIt first, InputIt last) : ADS_set{} { insert(first, last); }	
  ADS_set (const ADS_set &other) : ADS_set(other.d, other.table_size) { next_to_split = other.next_to_split; for (const auto &k : other) insert_(k, false); }
  
  ~ADS_set ();
  ADS_set &operator=(const ADS_set &other) {
		if (this == &other) return *this;
		ADS_set temp {other};
		swap(temp);
		return *this;
	}
 
  ADS_set &operator=(std::initializer_list<key_type> ilist) {
		ADS_set temp {ilist};
		swap(temp);
		return *this;
	}
	
  size_type size () const { return curr_size; }		
  bool empty () const { return curr_size == 0; }
  size_type count (const key_type &key) const { return !!(find_(key).second); }
  iterator find (const key_type &key) const;

  void clear();
  void swap(ADS_set &other);

  void insert (std::initializer_list<key_type> ilist) { insert(std::begin(ilist), std::end(ilist)); }
  std::pair<iterator,bool> insert (const key_type &key);
  template<typename InputIt> void insert (InputIt first, InputIt last) { for (auto it {first}; it != last; ++it) if (!count(*it)) insert_(*it); }
  size_type erase (const key_type &key);

  const_iterator begin () const { if (curr_size == 0) return end(); return Iterator(this, table); }
  const_iterator end () const { return Iterator(this, table+table_size); }

  void dump (std::ostream &o = std::cerr) const;

  friend bool operator== (const ADS_set &lhs, const ADS_set &rhs) {
  	if (lhs.curr_size  != rhs.curr_size) return false;
  	for (const auto &k : rhs) if (!lhs.count(k)) return false;
  	return true;
  }
  
  friend bool operator!=(const ADS_set &lhs, const ADS_set &rhs) { return !(lhs == rhs); }
};

template <typename Key, size_t N>
ADS_set<Key,N>::ADS_set (size_type d, size_type table_size) : d {d}, table_size {table_size} {
	real_table_size = 1 << (d+1);
	table = new bucket*[real_table_size];
	for (size_type idx {0}; idx < table_size; ++idx) {
		table[idx] = new bucket;
	}	
}


template <typename Key, size_t N>
ADS_set<Key,N>::~ADS_set () {
	for (size_type idx {0}; idx < table_size; ++idx) {
		bucket *current_bucket {table[idx]};
		do { 
			delete[] current_bucket->array;
			bucket *finished_bucket {current_bucket};
			current_bucket = current_bucket->overflow_bucket;
			delete finished_bucket;
		} while (current_bucket != nullptr);
	}
	delete[] table;
}


template <typename Key, size_t N>
typename ADS_set<Key,N>::Iterator ADS_set<Key,N>::find (const key_type &key) const {
	std::pair<bucket*,key_type*> find_return {find_(key)};
	if (find_return.second == nullptr) return end();
	return Iterator(this, find_return.second);
}


template <typename Key, size_t N>
void ADS_set<Key,N>::clear () {
	ADS_set temp;
	swap(temp);
}


template <typename Key, size_t N>
void ADS_set<Key,N>::swap(ADS_set &other) {
	using std::swap;
	swap(table, other.table);
	swap(d, other.d);
	swap(next_to_split, other.next_to_split);
	swap(table_size, other.table_size);
	swap(curr_size, other.curr_size);
	swap(real_table_size, other.real_table_size);
}	


template <typename Key, size_t N>
std::pair<typename ADS_set<Key,N>::iterator,bool> ADS_set<Key,N>::insert (const key_type &key) {
	Iterator it_on_key {find(key)};
	bool ret_bool {false};
	if (it_on_key == end()) {
		insert_(key);
		it_on_key = find(key);
		ret_bool = true;
	}
	return std::pair<iterator,bool>{it_on_key, ret_bool};
}


template <typename Key, size_t N>
size_t ADS_set<Key,N>::erase (const key_type &key) {
	std::pair<bucket*,key_type*> find_return {find_(key)};
	key_type *key_ptr {find_return.second};
	if (key_ptr == nullptr) return 0;
	bucket *containing_bucket {find_return.first};
	
	if (key_ptr+1 != (containing_bucket->array + containing_bucket->bucket_curr_size)) {
		for ( ; key_ptr != (containing_bucket->array + containing_bucket->bucket_curr_size - 1); ++key_ptr) 
			*key_ptr = *(key_ptr+1);
	}
	--containing_bucket->bucket_curr_size;
	--curr_size;
	return 1;
}	


template <typename Key, size_t N>
void ADS_set<Key,N>::rehash_keys (ADS_set<Key,N>::bucket *bucket_to_rehash) {	
	for (size_type i {0}; i < bucket_to_rehash->bucket_curr_size; ++i) 
		insert_(bucket_to_rehash->array[i], false);
	
	if (bucket_to_rehash->overflow_bucket != nullptr) {
		rehash_keys(bucket_to_rehash->overflow_bucket);
		delete bucket_to_rehash->overflow_bucket;
	}
	
	delete[] bucket_to_rehash->array;
}


template <typename Key, size_t N>
void ADS_set<Key,N>::split () {
	size_type old_curr_size {curr_size};
	bucket *bucket_to_split {table[next_to_split]};
	++table_size;
	
	if (table_size > real_table_size) {
		size_type old_table_size = table_size-1;
		bucket **old_table {table};	
		real_table_size *= 2;
		table = new bucket*[real_table_size];
	
		for (size_type idx {0}; idx < old_table_size; ++idx)  
			if (idx != next_to_split) 
				table[idx] = old_table[idx];	
		delete[] old_table;
	}

	table[table_size-1] = new bucket;
	table[next_to_split] = new bucket;
	++next_to_split;
	rehash_keys(bucket_to_split);
	delete bucket_to_split;
	curr_size = old_curr_size;
	
	if (next_to_split >= size_type(1 << d)) {
		++d;
		next_to_split = 0;
	}
}
		 

template <typename Key, size_t N>
void ADS_set<Key,N>::insert_ (const key_type &key, bool is_allowed_to_call_split) {
	if (table_size == 0) {
		table[0] = new bucket;
		++table_size;
	}
	
	bool split_signal {false};
	size_type idx {h(key)};
	bucket *last_bucket {table[idx]};
	
	while (last_bucket->overflow_bucket != nullptr) {
		if (!last_bucket->bucket_isfull()) break;
		last_bucket = last_bucket->overflow_bucket;
	}
	if (last_bucket->bucket_isfull()) {
		last_bucket->overflow_bucket = new bucket;
		last_bucket = last_bucket->overflow_bucket;
		split_signal = true;
	}
	last_bucket->array[last_bucket->bucket_curr_size] = key;
	++last_bucket->bucket_curr_size;
	++curr_size;
	
	if (is_allowed_to_call_split && split_signal) 
		split();																		
}


template <typename Key, size_t N>
typename ADS_set<Key,N>::key_type *ADS_set<Key,N>::bucket::find_in_bucket (const key_type &key) const {
	for (size_type i {0}; i < bucket_curr_size; ++i) 
		if (key_equal{}(array[i], key)) return array + i;
	return nullptr;
}


template <typename Key, size_t N>
std::pair<typename ADS_set<Key,N>::bucket*, typename ADS_set<Key,N>::key_type*> ADS_set<Key,N>::find_ (const key_type &key) const {
	size_type idx {h(key)};
	if (table_size == 0) return std::pair<bucket*,key_type*>{nullptr, nullptr};
	bucket *output_bucket {table[idx]};
	key_type *output_key {nullptr};
	while (output_bucket != nullptr) {
		output_key = output_bucket->find_in_bucket(key);
		if (output_key != nullptr) return std::pair<bucket*,key_type*>{output_bucket, output_key};
		output_bucket = output_bucket->overflow_bucket;
	}
	return std::pair<bucket*,key_type*>{output_bucket, output_key};
}


template <typename Key, size_t N>
std::ostream& ADS_set<Key,N>::bucket::dump_bucket (std::ostream &o) const {		
	o << "[";	
	for (size_type i {0}; i < bucket_curr_size; ++i)
		o << "(" << array[i] << ")";
	for (size_type i {bucket_curr_size}; i < N; ++i) 
		o << "(-)";
	o << "]";
	
	if (overflow_bucket != nullptr)	{
		o << " <-- "; 
		overflow_bucket->dump_bucket(o);
	}
	return o;
}


template <typename Key, size_t N>
void ADS_set<Key,N>::dump (std::ostream &o) const {
	o << "curr_size = " << curr_size << ", table_size = " << table_size << "\n";
	o << "d = " << d << ", next_to_split = " << next_to_split << "\n";
	o << "N = " << N << "\n";
	for (size_type idx {0}; idx < table_size; ++idx) {
		o << idx << ": ";
		table[idx]->dump_bucket(o) << "\n";
	}
} 


template <typename Key, size_t N>
class ADS_set<Key,N>::Iterator {
public:
  using value_type = Key;
  using difference_type = std::ptrdiff_t;
  using reference = const value_type &;
  using pointer = const value_type *;
  using iterator_category = std::forward_iterator_tag;

private:
	bucket **table;
	size_type table_size;
	bucket **index_pos;
	bucket *current_bucket {nullptr};
	pointer key_pos {nullptr};
	
	void skip () {
		while (1) {
			do {
				if (!current_bucket->bucket_empty()) {
					key_pos = current_bucket->array;
					return;			
				}
				current_bucket = current_bucket->overflow_bucket;
			} while (current_bucket != nullptr);
			if (++index_pos != table+table_size) current_bucket = *index_pos;
			else break;
		}
		key_pos = nullptr;
	}
	
	
public:
  explicit Iterator (const ADS_set *object = nullptr, bucket **index_pos = nullptr) : index_pos {index_pos} {
  	if (object != nullptr) {
  		table = object->table;
  		table_size = object->table_size;
			if (index_pos) {
				if (index_pos == table+table_size) return; //END ITERATOR: current_bucket, key_pos == nullptr;
				current_bucket = *index_pos;
				skip();
			}
		}
  }
  
  explicit Iterator (const ADS_set *object, pointer key_pos) : table {object->table}, table_size {object->table_size}, key_pos {key_pos} {
  	current_bucket = object->find_(*key_pos).first; //key at key_pos must be valid
  	index_pos = table + object->h(*key_pos);
  }
  	
  reference operator* () const { if (key_pos == nullptr) throw std::runtime_error("Iterator cannot be dereferenced"); return *key_pos; };
  pointer operator->() const { if (key_pos == nullptr) throw std::runtime_error("Iterator cannot be dereferenced"); return key_pos; }
  
  Iterator &operator++ () { 
  	++key_pos; 
  	if (key_pos == (current_bucket->array + current_bucket->bucket_curr_size)) {
			current_bucket = current_bucket->overflow_bucket; //next bucket in chain
			if (current_bucket == nullptr) { //if next bucket in chain doesn't exist
				if (++index_pos == table+table_size) {
					key_pos = nullptr; // table end; END ITERATOR: current_bucket, key_pos == nullptr
					return *this;;
				}
				current_bucket = *index_pos; //next row first bucket
			}
			skip(); //now we have a next bucket, empty or filled doesn't matter, skip() will find the next valid element
		} 
		return *this; 
	}
	
  Iterator operator++(int) { auto temp {*this}; ++*this; return temp; }
  
  friend bool operator== (const Iterator &lhs, const Iterator &rhs) {
  	if (lhs.key_pos == rhs.key_pos && lhs.key_pos == rhs.key_pos && lhs.key_pos == rhs.key_pos) return true;
  	return false;
  }
  
  friend bool operator!= (const Iterator &lhs, const Iterator &rhs) { return !(lhs == rhs); };
};

template <typename Key, size_t N> void swap(ADS_set<Key,N> &lhs, ADS_set<Key,N> &rhs) { lhs.swap(rhs); }

#endif // ADS_SET_H
