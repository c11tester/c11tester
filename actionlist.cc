#include "actionlist.h"
#include "action.h"
#include <string.h>
#include "stl-model.h"
#include <limits.h>

actionlist::actionlist() :
	root(),
	head(NULL),
	tail(NULL),
	_size(0)
{
}

actionlist::~actionlist() {
	clear();
}

allnode::allnode() :
	parent(NULL),
	count(0) {
	bzero(children, sizeof(children));
}

allnode::~allnode() {
	if (count != 0)
		for(int i=0;i<ALLNODESIZE;i++) {
			if (children[i] != NULL && !(((uintptr_t) children[i]) & ISACT))
				delete children[i];
		}
}

sllnode<ModelAction *> * allnode::findPrev(modelclock_t index) {
	allnode * ptr = this;
	modelclock_t increment = 1;
	int totalshift = 0;
	index -= increment;

	while(1) {
		modelclock_t currindex = (index >> totalshift) & ALLMASK;

		//See if we went negative...
		if (currindex != ALLMASK) {
			if (ptr->children[currindex] == NULL) {
				//need to keep searching at this level
				index -= increment;
				continue;
			} else {
				//found non-null...
				if (totalshift == 0)
					return reinterpret_cast<sllnode<ModelAction *> *>(((uintptr_t)ptr->children[currindex])& ACTMASK);
				//need to increment here...
				ptr = ptr->children[currindex];
				increment = increment >> ALLBITS;
				totalshift -= ALLBITS;
				break;
			}
		}
		//If we get here, we already did the decrement earlier...no need to decrement again
		ptr = ptr->parent;
		increment = increment << ALLBITS;
		totalshift += ALLBITS;

		if (ptr == NULL) {
			return NULL;
		}
	}

	while(1) {
		while(1) {
			modelclock_t currindex = (index >> totalshift) & ALLMASK;
			if (ptr->children[currindex] != NULL) {
				if (totalshift != 0) {
					ptr = ptr->children[currindex];
					break;
				} else {
					allnode * act = ptr->children[currindex];
					sllnode<ModelAction *> * node = reinterpret_cast<sllnode<ModelAction *>*>(((uintptr_t)act) & ACTMASK);
					return node;
				}
			}
			index -= increment;
		}

		increment = increment >> ALLBITS;
		totalshift -= ALLBITS;
	}
}

void actionlist::addAction(ModelAction * act) {
	_size++;
	int shiftbits = MODELCLOCKBITS - ALLBITS;
	modelclock_t clock = act->get_seq_number();

	allnode * ptr = &root;
	do {
		modelclock_t currindex = (clock >> shiftbits) & ALLMASK;
		allnode * tmp = ptr->children[currindex];
		if (shiftbits == 0) {
			sllnode<ModelAction *> * llnode = new sllnode<ModelAction *>();
			llnode->val = act;
			if (tmp == NULL) {
				sllnode<ModelAction *> * llnodeprev = ptr->findPrev(clock);
				if (llnodeprev != NULL) {
					llnode->next = llnodeprev->next;
					llnode->prev = llnodeprev;

					//see if we are the new tail
					if (llnode->next != NULL)
						llnode->next->prev = llnode;
					else
						tail = llnode;
					llnodeprev->next = llnode;
				} else {
					//We are the begining
					llnode->next = head;
					llnode->prev = NULL;
					if (head != NULL) {
						head->prev = llnode;
					} else {
						//we are the first node
						tail = llnode;
					}

					head = llnode;
				}
				ptr->children[currindex] = reinterpret_cast<allnode *>(((uintptr_t) llnode) | ISACT);

				//need to find next link
				ptr->count++;
			} else {
				//handle case where something else is here

				sllnode<ModelAction *> * llnodeprev = reinterpret_cast<sllnode<ModelAction *>*>(((uintptr_t) tmp) & ACTMASK);
				llnode->next = llnodeprev->next;
				llnode->prev = llnodeprev;
				if (llnode->next != NULL)
					llnode->next->prev = llnode;
				else
					tail = llnode;
				llnodeprev->next = llnode;
				ptr->children[currindex] = reinterpret_cast<allnode *>(((uintptr_t) llnode) | ISACT);
			}
			return;
		} else if (tmp == NULL) {
			tmp = new allnode();
			ptr->children[currindex] = tmp;
			tmp->parent=ptr;
			ptr->count++;
		}
		shiftbits -= ALLBITS;
		ptr = tmp;
	} while(1);

}

void decrementCount(allnode * ptr) {
	ptr->count--;
	if (ptr->count == 0) {
		if (ptr->parent != NULL) {
			for(uint i=0;i<ALLNODESIZE;i++) {
				if (ptr->parent->children[i]==ptr) {
					ptr->parent->children[i] = NULL;
					decrementCount(ptr->parent);
					break;
				}
			}
			delete ptr;
		}
	}
}

void actionlist::removeAction(ModelAction * act) {
	int shiftbits = MODELCLOCKBITS;
	modelclock_t clock = act->get_seq_number();
	allnode * ptr = &root;
	allnode * oldptr;
	modelclock_t currindex;

	while(shiftbits != 0) {
		shiftbits -= ALLBITS;
		currindex = (clock >> shiftbits) & ALLMASK;
		oldptr = ptr;
		ptr = ptr->children[currindex];
		if (ptr == NULL)
			return;
	}

	sllnode<ModelAction *> * llnode = reinterpret_cast<sllnode<ModelAction *> *>(((uintptr_t) ptr) & ACTMASK);
	bool first = true;
	do {
		if (llnode->val == act) {
			//found node to remove
			sllnode<ModelAction *> * llnodeprev = llnode->prev;
			sllnode<ModelAction *> * llnodenext = llnode->next;
			if (llnodeprev != NULL) {
				llnodeprev->next = llnodenext;
			} else {
				head = llnodenext;
			}
			if (llnodenext != NULL) {
				llnodenext->prev = llnodeprev;
			} else {
				tail = llnodeprev;
			}
			if (first) {
				//see if previous node has same clock as us...
				if (llnodeprev != NULL && llnodeprev->val->get_seq_number() == clock) {
					oldptr->children[currindex] = reinterpret_cast<allnode *>(((uintptr_t)llnodeprev) | ISACT);
				} else {
					//remove ourselves and go up tree
					oldptr->children[currindex] = NULL;
					decrementCount(oldptr);
				}
			}
			delete llnode;
			_size--;
			return;
		}
		llnode = llnode->prev;
		first = false;
	} while(llnode != NULL && llnode->val->get_seq_number() == clock);
	//node not found in list... no deletion
	return;
}

void actionlist::clear() {
	for(uint i = 0;i < ALLNODESIZE;i++) {
		if (root.children[i] != NULL) {
			delete root.children[i];
			root.children[i] = NULL;
		}
	}

	while(head != NULL) {
		sllnode<ModelAction *> *tmp=head->next;
		delete head;
		head = tmp;
	}
	tail = NULL;

	root.count = 0;
	_size = 0;
}

bool actionlist::isEmpty() {
	return root.count == 0;
}

/**
 * Fix the parent pointer of root when root address changes (possible
 * due to vector<action_list_t> resize)
 */
void actionlist::fixupParent()
{
	for (int i = 0;i < ALLNODESIZE;i++) {
		allnode * child = root.children[i];
		if (child != NULL && child->parent != &root)
			child->parent = &root;
	}
}
