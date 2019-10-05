/*      Copyright (c) 2015 Regents of the University of California
 *
 *      Author: Brian Demsky <bdemsky@uci.edu>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation.
 */

#ifndef HASH_SET_H
#define HASH_SET_H
#include "hashtable.h"

template<typename _Key>
struct LinkNode {
	_Key key;
	LinkNode<_Key> *prev;
	LinkNode<_Key> *next;
};

template<typename _Key, typename _KeyInt, int _Shift, void * (* _malloc)(size_t), void * (* _calloc)(size_t, size_t), void (*_free)(void *), unsigned int (*hash_function)(_Key), bool (*equals)(_Key, _Key)>
class HashSet;

template<typename _Key, typename _KeyInt, int _Shift, void * (* _malloc)(size_t) = snapshot_malloc, void * (* _calloc)(size_t, size_t) = snapshot_calloc, void (*_free)(void *) = snapshot_free, unsigned int (*hash_function)(_Key) = default_hash_function<_Key, _Shift, _KeyInt>, bool (*equals)(_Key, _Key) = default_equals<_Key> >
class HSIterator {
public:
	HSIterator(LinkNode<_Key> *_curr, HashSet <_Key, _KeyInt, _Shift, _malloc, _calloc, _free, hash_function, equals> * _set) :
		curr(_curr),
		set(_set)
	{
	}

	/** Override: new operator */
	void * operator new(size_t size) {
		return _malloc(size);
	}

	/** Override: delete operator */
	void operator delete(void *p, size_t size) {
		_free(p);
	}

	/** Override: new[] operator */
	void * operator new[](size_t size) {
		return _malloc(size);
	}

	/** Override: delete[] operator */
	void operator delete[](void *p, size_t size) {
		_free(p);
	}

	bool hasNext() {
		return curr!=NULL;
	}

	_Key next() {
		_Key k=curr->key;
		last=curr;
		curr=curr->next;
		return k;
	}

	_Key currKey() {
		return last->key;
	}

	void remove() {
		_Key k=last->key;
		set->remove(k);
	}

private:
	LinkNode<_Key> *curr;
	LinkNode<_Key> *last;
	HashSet <_Key, _KeyInt, _Shift, _malloc, _calloc, _free, hash_function, equals> * set;
};

template<typename _Key, typename _KeyInt, int _Shift = 0, void * (*_malloc)(size_t) = snapshot_malloc, void * (*_calloc)(size_t, size_t) = snapshot_calloc, void (*_free)(void *) = snapshot_free, unsigned int (*hash_function)(_Key) = default_hash_function<_Key, _Shift, _KeyInt>, bool (*equals)(_Key, _Key) = default_equals<_Key> >
class HashSet {
public:
	HashSet(unsigned int initialcapacity = 16, double factor = 0.5) :
		table(new HashTable<_Key, LinkNode<_Key> *, _KeyInt, _Shift, _malloc, _calloc, _free, hash_function, equals>(initialcapacity, factor)),
		list(NULL),
		tail(NULL)
	{
	}

	/** @brief Hashset destructor */
	~HashSet() {
		LinkNode<_Key> *tmp=list;
		while(tmp!=NULL) {
			LinkNode<_Key> *tmpnext=tmp->next;
			_free(tmp);
			tmp=tmpnext;
		}
		delete table;
	}

	HashSet<_Key, _KeyInt, _Shift, _malloc, _calloc, _free, hash_function, equals> * copy() {
		HashSet<_Key, _KeyInt, _Shift, _malloc, _calloc, _free, hash_function, equals> *copy=new HashSet<_Key, _KeyInt, _Shift, _malloc, _calloc, _free, hash_function, equals>(table->getCapacity(), table->getLoadFactor());
		HSIterator<_Key, _KeyInt, _Shift, _malloc, _calloc, _free, hash_function, equals> * it=iterator();
		while(it->hasNext())
			copy->add(it->next());
		delete it;
		return copy;
	}

	void reset() {
		LinkNode<_Key> *tmp=list;
		while(tmp!=NULL) {
			LinkNode<_Key> *tmpnext=tmp->next;
			_free(tmp);
			tmp=tmpnext;
		}
		list=tail=NULL;
		table->reset();
	}

	/** @brief Adds a new key to the hashset.  Returns false if the key
	 *  is already present. */

	bool add(_Key key) {
		LinkNode<_Key> * val=table->get(key);
		if (val==NULL) {
			LinkNode<_Key> * newnode=(LinkNode<_Key> *)_malloc(sizeof(struct LinkNode<_Key>));
			newnode->prev=tail;
			newnode->next=NULL;
			newnode->key=key;
			if (tail!=NULL)
				tail->next=newnode;
			else
				list=newnode;
			tail=newnode;
			table->put(key, newnode);
			return true;
		} else
			return false;
	}

	/** @brief Gets the original key corresponding to this one from the
	 *  hashset.  Returns NULL if not present. */

	_Key get(_Key key) {
		LinkNode<_Key> * val=table->get(key);
		if (val!=NULL)
			return val->key;
		else
			return NULL;
	}

	_Key getFirstKey() {
		return list->key;
	}

	bool contains(_Key key) {
		return table->get(key)!=NULL;
	}

	bool remove(_Key key) {
		LinkNode<_Key> * oldlinknode;
		oldlinknode=table->get(key);
		if (oldlinknode==NULL) {
			return false;
		}
		table->remove(key);

		//remove link node from the list
		if (oldlinknode->prev==NULL)
			list=oldlinknode->next;
		else
			oldlinknode->prev->next=oldlinknode->next;
		if (oldlinknode->next!=NULL)
			oldlinknode->next->prev=oldlinknode->prev;
		else
			tail=oldlinknode->prev;
		_free(oldlinknode);
		return true;
	}

	unsigned int getSize() {
		return table->getSize();
	}

	bool isEmpty() {
		return getSize()==0;
	}



	HSIterator<_Key, _KeyInt, _Shift, _malloc, _calloc, _free, hash_function, equals> * iterator() {
		return new HSIterator<_Key, _KeyInt, _Shift, _malloc, _calloc, _free, hash_function, equals>(list, this);
	}

	/** Override: new operator */
	void * operator new(size_t size) {
		return _malloc(size);
	}

	/** Override: delete operator */
	void operator delete(void *p, size_t size) {
		_free(p);
	}

	/** Override: new[] operator */
	void * operator new[](size_t size) {
		return _malloc(size);
	}

	/** Override: delete[] operator */
	void operator delete[](void *p, size_t size) {
		_free(p);
	}
private:
	HashTable<_Key, LinkNode<_Key>*, _KeyInt, _Shift, _malloc, _calloc, _free, hash_function, equals> * table;
	LinkNode<_Key> *list;
	LinkNode<_Key> *tail;
};
#endif
