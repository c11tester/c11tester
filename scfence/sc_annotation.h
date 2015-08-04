#ifndef _SC_ANNOTATION_H
#define _SC_ANNOTATION_H

#include "cdsannotate.h"
#include "action.h"

#define SC_ANNOTATION 2

#define BEGIN 1
#define END 2
#define KEEP 3


inline bool IS_SC_ANNO(ModelAction *act) {
	return act != NULL && act->is_annotation() &&
		act->get_value() == SC_ANNOTATION;
}

inline bool IS_ANNO_BEGIN(ModelAction *act) {
	return (void*) BEGIN == act->get_location();
}

inline bool IS_ANNO_END(ModelAction *act) {
	return (void*) END == act->get_location();
}

inline bool IS_ANNO_KEEP(ModelAction *act) {
	return (void*) KEEP == act->get_location();
}

inline void SC_BEGIN() {
	void *loc = (void*) BEGIN;
	cdsannotate(SC_ANNOTATION, loc);
}

inline void SC_END() {
	void *loc = (void*) END;
	cdsannotate(SC_ANNOTATION, loc);
}

inline void SC_KEEP() {
	void *loc = (void*) KEEP;
	cdsannotate(SC_ANNOTATION, loc);
}


#endif
