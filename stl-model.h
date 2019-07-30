#ifndef __STL_MODEL_H__
#define __STL_MODEL_H__

#include <list>
#include "mymemory.h"

template<typename _Tp>
class ModelList : public std::list<_Tp, ModelAlloc<_Tp> >
{
public:
	typedef std::list< _Tp, ModelAlloc<_Tp> > list;

	ModelList() :
		list()
	{ }

	ModelList(size_t n, const _Tp& val = _Tp()) :
		list(n, val)
	{ }

	MEMALLOC
};

template<typename _Tp>
class SnapList : public std::list<_Tp, SnapshotAlloc<_Tp> >
{
public:
	typedef std::list<_Tp, SnapshotAlloc<_Tp> > list;

	SnapList() :
		list()
	{ }

	SnapList(size_t n, const _Tp& val = _Tp()) :
		list(n, val)
	{ }

	SNAPSHOTALLOC
};

#define VECTOR_DEFCAP 8

typedef unsigned int uint;

template<typename type>
class ModelVector {
public:
	ModelVector(uint _capacity = VECTOR_DEFCAP) :
		_size(0),
		capacity(_capacity),
		array((type *) model_malloc(sizeof(type) * _capacity)) {
	}

	ModelVector(uint _capacity, type *_array)  :
		_size(_capacity),
		capacity(_capacity),
		array((type *) model_malloc(sizeof(type) * _capacity)) {
		memcpy(array, _array, capacity * sizeof(type));
	}
	void pop_back() {
		_size--;
	}

	type back() const {
		return array[size - 1];
	}

	void resize(uint psize) {
		if (psize <= _size) {
			_size = psize;
			return;
		} else if (psize > capacity) {
			array = (type *)model_realloc(array, (psize << 1) * sizeof(type));
			capacity = psize << 1;
		}
		bzero(&array[_size], (psize - _size) * sizeof(type));
		_size = psize;
	}

	void push_back(type item) {
		if (_size >= capacity) {
			uint newcap = capacity << 1;
			array = (type *)model_realloc(array, newcap * sizeof(type));
			capacity = newcap;
		}
		array[_size++] = item;
	}

	type operator[](uint index) const {
		return array[index];
	}

	type & operator[](uint index) {
		return array[index];
	}

	bool empty() const {
		return _size == 0;
	}

	type & at(uint index) const {
		return array[index];
	}

	void setExpand(uint index, type item) {
		if (index >= _size)
			resize(index + 1);
		set(index, item);
	}

	void set(uint index, type item) {
		array[index] = item;
	}

	void insertAt(uint index, type item) {
		resize(_size + 1);
		for (uint i = _size - 1;i > index;i--) {
			set(i, at(i - 1));
		}
		array[index] = item;
	}

	void removeAt(uint index) {
		for (uint i = index;(i + 1) < _size;i++) {
			set(i, at(i + 1));
		}
		resize(_size - 1);
	}

	inline uint size() const {
		return _size;
	}

	~ModelVector() {
		model_free(array);
	}

	void clear() {
		_size = 0;
	}

	MEMALLOC;
private:
	uint _size;
	uint capacity;
	type *array;
};


template<typename type>
class SnapVector {
public:
	SnapVector(uint _capacity = VECTOR_DEFCAP) :
		_size(0),
		capacity(_capacity),
		array((type *) snapshot_malloc(sizeof(type) * _capacity)) {
	}

	SnapVector(uint _capacity, type *_array)  :
		_size(_capacity),
		capacity(_capacity),
		array((type *) snapshot_malloc(sizeof(type) * _capacity)) {
		memcpy(array, _array, capacity * sizeof(type));
	}
	void pop_back() {
		_size--;
	}

	type back() const {
		return array[_size - 1];
	}

	void resize(uint psize) {
		if (psize <= _size) {
			_size = psize;
			return;
		} else if (psize > capacity) {
			array = (type *)snapshot_realloc(array, (psize <<1 )* sizeof(type));
			capacity = psize << 1;
		}
		bzero(&array[_size], (psize - _size) * sizeof(type));
		_size = psize;
	}

	void push_back(type item) {
		if (_size >= capacity) {
			uint newcap = capacity << 1;
			array = (type *)snapshot_realloc(array, newcap * sizeof(type));
			capacity = newcap;
		}
		array[_size++] = item;
	}

	type & operator[](uint index) {
		return array[index];
	}

	type operator[](uint index) const {
		return array[index];
	}

	bool empty() const {
		return _size == 0;
	}

	type & at(uint index) const {
		return array[index];
	}

	void setExpand(uint index, type item) {
		if (index >= _size)
			resize(index + 1);
		set(index, item);
	}

	void set(uint index, type item) {
		array[index] = item;
	}

	void insertAt(uint index, type item) {
		resize(_size + 1);
		for (uint i = _size - 1;i > index;i--) {
			set(i, at(i - 1));
		}
		array[index] = item;
	}

	void removeAt(uint index) {
		for (uint i = index;(i + 1) < _size;i++) {
			set(i, at(i + 1));
		}
		resize(_size - 1);
	}

	inline uint size() const {
		return _size;
	}

	~SnapVector() {
		snapshot_free(array);
	}

	void clear() {
		_size = 0;
	}

	SNAPSHOTALLOC;
private:
	uint _size;
	uint capacity;
	type *array;
};

#endif	/* __STL_MODEL_H__ */
