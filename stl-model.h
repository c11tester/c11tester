#ifndef __STL_MODEL_H__
#define __STL_MODEL_H__

#include <list>
#include "mymemory.h"
typedef unsigned int uint;

template<typename _Tp>
class mllnode {
public:
	_Tp getVal() {return val;}
	mllnode<_Tp> * getNext() {return next;}
	mllnode<_Tp> * getPrev() {return prev;}

	MEMALLOC;

private:
	mllnode<_Tp> * next;
	mllnode<_Tp> * prev;
	_Tp val;
	template<typename T>
	friend class ModelList;
};

template<typename _Tp>
class ModelList
{
public:
	ModelList() : head(NULL),
		tail(NULL), _size(0) {
	}

	void push_front(_Tp val) {
		mllnode<_Tp> * tmp = new mllnode<_Tp>();
		tmp->prev = NULL;
		tmp->next = head;
		tmp->val = val;
		if (head == NULL)
			tail = tmp;
		else
			head->prev = tmp;
		head = tmp;
		_size++;
	}

	void push_back(_Tp val) {
		mllnode<_Tp> * tmp = new mllnode<_Tp>();
		tmp->prev = tail;
		tmp->next = NULL;
		tmp->val = val;
		if (tail == NULL)
			head = tmp;
		else tail->next = tmp;
		tail = tmp;
		_size++;
	}

	void pop_front() {
		mllnode<_Tp> *tmp = head;
		head = head->next;
		if (head == NULL)
			tail = NULL;
		else
			head->prev = NULL;
		delete tmp;
		_size--;
	}

	void pop_back() {
		mllnode<_Tp> *tmp = tail;
		tail = tail->prev;
		if (tail == NULL)
			head = NULL;
		else
			tail->next = NULL;
		delete tmp;
		_size--;
	}

	void clear() {
		while(head != NULL) {
			mllnode<_Tp> *tmp=head->next;
			delete head;
			head = tmp;
		}
		tail=NULL;
		_size=0;
	}

	void insertAfter(mllnode<_Tp> * node, _Tp val) {
		mllnode<_Tp> *tmp = new mllnode<_Tp>();
		tmp->val = val;
		tmp->prev = node;
		tmp->next = node->next;
		node->next = tmp;
		if (tmp->next == NULL) {
			tail = tmp;
		} else {
			tmp->next->prev = tmp;
		}
		_size++;
	}

	void insertBefore(mllnode<_Tp> * node, _Tp val) {
		mllnode<_Tp> *tmp = new mllnode<_Tp>();
		tmp->val = val;
		tmp->next = node;
		tmp->prev = node->prev;
		node->prev = tmp;
		if (tmp->prev == NULL) {
			head = tmp;
		} else {
			tmp->prev->next = tmp;
		}
		_size++;
	}

	mllnode<_Tp> * erase(mllnode<_Tp> * node) {
		if (head == node) {
			head = node->next;
		} else {
			node->prev->next = node->next;
		}

		if (tail == node) {
			tail = node->prev;
		} else {
			node->next->prev = node->prev;
		}
		mllnode<_Tp> *next = node->next;
		delete node;
		_size--;
		return next;
	}

	mllnode<_Tp> * begin() {
		return head;
	}

	mllnode<_Tp> * end() {
		return tail;
	}

	_Tp front() {
		return head->val;
	}

	_Tp back() {
		return tail->val;
	}

	uint size() {
		return _size;
	}

	bool empty() {
		return _size == 0;
	}

	MEMALLOC;
private:
	mllnode<_Tp> *head;
	mllnode<_Tp> *tail;
	uint _size;
};

class actionlist;

template<typename _Tp>
class sllnode {
public:
	_Tp getVal() {return val;}
	sllnode<_Tp> * getNext() {return next;}
	sllnode<_Tp> * getPrev() {return prev;}
	SNAPSHOTALLOC;

private:
	sllnode<_Tp> * next;
	sllnode<_Tp> * prev;
	_Tp val;
	template<typename T>
	friend class SnapList;
	friend class actionlist;
};

template<typename _Tp>
class SnapList
{
public:
	SnapList() : head(NULL),
		tail(NULL), _size(0) {
	}

	void push_front(_Tp val) {
		sllnode<_Tp> * tmp = new sllnode<_Tp>();
		tmp->prev = NULL;
		tmp->next = head;
		tmp->val = val;
		if (head == NULL)
			tail = tmp;
		else
			head->prev = tmp;
		head = tmp;
		_size++;
	}

	void push_back(_Tp val) {
		sllnode<_Tp> * tmp = new sllnode<_Tp>();
		tmp->prev = tail;
		tmp->next = NULL;
		tmp->val = val;
		if (tail == NULL)
			head = tmp;
		else tail->next = tmp;
		tail = tmp;
		_size++;
	}

	sllnode<_Tp>* add_front(_Tp val) {
		sllnode<_Tp> * tmp = new sllnode<_Tp>();
		tmp->prev = NULL;
		tmp->next = head;
		tmp->val = val;
		if (head == NULL)
			tail = tmp;
		else
			head->prev = tmp;
		head = tmp;
		_size++;
		return tmp;
	}

	sllnode<_Tp> * add_back(_Tp val) {
		sllnode<_Tp> * tmp = new sllnode<_Tp>();
		tmp->prev = tail;
		tmp->next = NULL;
		tmp->val = val;
		if (tail == NULL)
			head = tmp;
		else tail->next = tmp;
		tail = tmp;
		_size++;
		return tmp;
	}

	void pop_front() {
		sllnode<_Tp> *tmp = head;
		head = head->next;
		if (head == NULL)
			tail = NULL;
		else
			head->prev = NULL;
		delete tmp;
		_size--;
	}

	void pop_back() {
		sllnode<_Tp> *tmp = tail;
		tail = tail->prev;
		if (tail == NULL)
			head = NULL;
		else
			tail->next = NULL;
		delete tmp;
		_size--;
	}

	void clear() {
		while(head != NULL) {
			sllnode<_Tp> *tmp=head->next;
			delete head;
			head = tmp;
		}
		tail=NULL;
		_size=0;
	}

	sllnode<_Tp> * insertAfter(sllnode<_Tp> * node, _Tp val) {
		sllnode<_Tp> *tmp = new sllnode<_Tp>();
		tmp->val = val;
		tmp->prev = node;
		tmp->next = node->next;
		node->next = tmp;
		if (tmp->next == NULL) {
			tail = tmp;
		} else {
			tmp->next->prev = tmp;
		}
		_size++;
		return tmp;
	}

	void insertBefore(sllnode<_Tp> * node, _Tp val) {
		sllnode<_Tp> *tmp = new sllnode<_Tp>();
		tmp->val = val;
		tmp->next = node;
		tmp->prev = node->prev;
		node->prev = tmp;
		if (tmp->prev == NULL) {
			head = tmp;
		} else {
			tmp->prev->next = tmp;
		}
		_size++;
	}

	sllnode<_Tp> * erase(sllnode<_Tp> * node) {
		if (head == node) {
			head = node->next;
		} else {
			node->prev->next = node->next;
		}

		if (tail == node) {
			tail = node->prev;
		} else {
			node->next->prev = node->prev;
		}

		sllnode<_Tp> *next = node->next;
		delete node;
		_size--;
		return next;
	}

	sllnode<_Tp> * begin() {
		return head;
	}

	sllnode<_Tp> * end() {
		return tail;
	}

	_Tp front() {
		return head->val;
	}

	_Tp back() {
		return tail->val;
	}
	uint size() {
		return _size;
	}
	bool empty() {
		return _size == 0;
	}

	SNAPSHOTALLOC;
private:
	sllnode<_Tp> *head;
	sllnode<_Tp> *tail;
	uint _size;
};


#define VECTOR_DEFCAP 8


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
		return array[_size - 1];
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

	type operator[](int index) const {
		return array[index];
	}

	type & operator[](int index) {
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

	type operator[](int index) const {
		return array[index];
	}

	type & operator[](int index) {
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

	void remove(type item) {
		for(uint i = 0;i < _size;i++) {
			if (at(i) == item) {
				removeAt(i);
				return;
			}
		}
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
