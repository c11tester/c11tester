#include "actionlist.h"
#include "action.h"
#include <string.h>
#include "stl-model.h"
#include <limits.h>

actionlist::actionlist() :
	head(NULL),
	tail(NULL),
	_size(0)
{
	root.parent = NULL;
}

actionlist::~actionlist() {
}

allnode::allnode() :
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
	modelclock_t mask = ALLMASK;
	int totalshift = 0;
	index -= increment;

	while(1) {
		modelclock_t shiftclock = index >> totalshift;
		modelclock_t currindex = shiftclock & ALLMASK;

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
				mask = mask >> ALLBITS;
				totalshift -= ALLBITS;
				break;
			}
		}
		//If we get here, we already did the decrement earlier...no need to decrement again
		ptr = ptr->parent;
		increment = increment << ALLBITS;
		mask = mask << ALLBITS;
		totalshift += ALLBITS;

		if (ptr == NULL) {
			return NULL;
		}
	}

	while(1) {
		while(1) {
			modelclock_t shiftclock = index >> totalshift;
			modelclock_t currindex = shiftclock & ALLMASK;
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
		mask = mask >> ALLBITS;
		totalshift -= ALLBITS;
	}
}

void actionlist::addAction(ModelAction * act) {
	_size++;
	int shiftbits = MODELCLOCKBITS - ALLBITS;
	modelclock_t clock = act->get_seq_number();

	allnode * ptr = &root;
	do {
		int index = (clock >> shiftbits) & ALLMASK;
		allnode * tmp = ptr->children[index];
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
				ptr->children[index] = reinterpret_cast<allnode *>(((uintptr_t) llnode) | ISACT);

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
				ptr->children[index] = reinterpret_cast<allnode *>(((uintptr_t) llnode) | ISACT);
			}
			return;
		} else if (tmp == NULL) {
			tmp = new allnode();
			ptr->children[index] = tmp;
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
				}
			}
		}
		delete ptr;
	}
}

void actionlist::removeAction(ModelAction * act) {
	int shiftbits = MODELCLOCKBITS - ALLBITS;
	modelclock_t clock = act->get_seq_number();
	allnode * ptr = &root;
	do {
		int index = (clock >> shiftbits) & ALLMASK;
		allnode * tmp = ptr->children[index];
		if (shiftbits == 0) {
			if (tmp == NULL) {
				//not found
				return;
			} else {
				sllnode<ModelAction *> * llnode = reinterpret_cast<sllnode<ModelAction *> *>(((uintptr_t) tmp) & ACTMASK);
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
							if (llnodeprev->val->get_seq_number() == clock) {
								ptr->children[index] = reinterpret_cast<allnode *>(((uintptr_t)llnodeprev) | ISACT);
							} else {
								//remove ourselves and go up tree
								ptr->children[index] = NULL;
								decrementCount(ptr);
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
		} else if (tmp == NULL) {
			//not found
			return;
		}
		shiftbits -= ALLBITS;
		ptr = tmp;
	} while(1);
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
	tail=NULL;

	root.count = 0;
	_size = 0;
}

bool actionlist::isEmpty() {
	return root.count == 0;
}
