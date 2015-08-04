#ifndef _INFERSET_H
#define _INFERSET_H

#include "fence_common.h"
#include "patch.h"
#include "inference.h"
#include "inferlist.h"

typedef struct inference_stat {
	int notAddedAtFirstPlace;

	inference_stat() :
		notAddedAtFirstPlace(0) {}

	void print() {
		model_print("Inference process statistics output:...\n");
		model_print("The number of inference not added at the first place: %d\n",
			notAddedAtFirstPlace);
	}
} inference_stat_t;


/** Essentially, we have to explore a big lattice of inferences, the bottom of
 *  which is the inference that has all relaxed orderings, and the top of which
 *  is the one that has all SC orderings. We define the partial ordering between
 *  inferences as in compareTo() function in the Infereence class. In another
 *  word, we compare ordering parameters one by one as a vector. Theoretically,
 *  we need to explore up to the number of 5^N inferences, where N denotes the
 *  number of wildcards (since we have 5 possible options for each memory order).

 *  We try to reduce the searching space by recording whether an inference has
 *  been discovered or not, and if so, we only need to explore from that
 *  inference for just once. We can use a set to record the inferences to be be
 *  explored, and insert new undiscovered fixes to that set iteratively until it
 *  gets empty.

 *  In detail, we use the InferenceList to represent that set, and use a
 *  LIFO-like actions in pushing and popping inferences. When we add an
 *  inference to the stack, we will set it to leaf or non-leaf node. For the
 *  current inference to be added, it is non-leaf node because it generates
 *  stronger inferences. On the other hands, for those generated inferences, we
 *  set them to be leaf node. So when we pop a leaf node, we know that it is
 *  not just discovered but thoroughly explored. Therefore, when we dicide
 *  whether an inference is discovered, we can first try to look up the
 *  discovered set and also derive those inferences that are stronger the
 *  explored ones to be discovered.

 ********** The main algorithm **********
 Initial:
 	InferenceSet set; // Store the candiates to explore in a set
	Set discoveredSet; // Store the discovered candidates. For each discovered
		// candidate, if it's explored (feasible or infeasible, meaning that
		// they are already known to work or not work), we set it to be
		// explored. With that, we can reduce the searching space by ignoring
		// those candidates that are stronger than the explored ones.
	Inference curInfer = RELAX; // Initialize the current inference to be all relaxed (RELAX)

 API Methods:
 	bool addInference(infer, bool isLeaf) {
		// Push back infer to the discovered when it's not discvoerd, and return
		// whether it's added or not
	}
	
	void commitInference(infer, isFeasible) {
		// Set the infer to be explored and add it to the result set if feasible 
	}

	Inference* getNextInference() {
		// Get the next unexplored inference so that we can contine searching
	}
*/

class InferenceSet {
	private:

	/** The set of already discovered nodes in the tree */
	InferenceList *discoveredSet;

	/** The list of feasible inferences */
	InferenceList *results;

	/** The set of candidates */
	InferenceList *candidates;

	/** The staticstics of inference process */
	inference_stat_t stat;
	
	public:
	InferenceSet();

	/** Print the result of inferences  */
	void printResults();

	/** Print candidates of inferences */
	void printCandidates();

	/** When we finish model checking or cannot further strenghen with the
	 * inference, we commit the inference to be explored; if it is feasible, we
	 * put it in the result list */
	void commitInference(Inference *infer, bool feasible);

	int getCandidatesSize();

	int getResultsSize();

	/** Be careful that if the candidate is not added, it will be deleted in this
	 *  function. Therefore, caller of this function should just delete the list when
	 *  finishing calling this function. */
	bool addCandidates(Inference *curInfer, InferenceList *inferList);


	/** Check if we have stronger or equal inferences in the current result
	 * list; if we do, we remove them and add the passed-in parameter infer */
	 void addResult(Inference *infer);

	/** Get the next available unexplored node; @Return NULL 
	 * if we don't have next, meaning that we are done with exploring */
	Inference* getNextInference();

	/** Add the current inference to the set before adding fixes to it; in
	 * this case, fixes will be added afterwards, and infer should've been
	 * discovered */
	void addCurInference(Inference *infer);
	
	/** Add one weaker node (the stronger one has been explored and known to be SC,
	 *  we just want to know if a weaker one might also be SC).
	 *  @Return true if the node to add has not been explored yet
	 */
	void addWeakerInference(Inference *curInfer);

	/** Add one possible node that represents a fix for the current inference;
	 * @Return true if the node to add has not been explored yet
	 */
	bool addInference(Inference *infer);

	/** Return false if we haven't discovered that inference yet. Basically we
	 * search the candidates list */
	bool hasBeenDiscovered(Inference *infer);

	/** Return true if we have explored this inference yet. Basically we
	 * search the candidates list */
	bool hasBeenExplored(Inference *infer);

	MEMALLOC
};

#endif
