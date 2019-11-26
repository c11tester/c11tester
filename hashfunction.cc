#include "hashfunction.h"

/**
 * Hash function for 64-bit integers
 * https://gist.github.com/badboy/6267743#64-bit-to-32-bit-hash-functions
 */
unsigned int int64_hash(uint64_t key) {
	key = (~key) + (key << 18);	// key = (key << 18) - key - 1;
	key = key ^ (key >> 31);
	key = key * 21;	// key = (key + (key << 2)) + (key << 4);
	key = key ^ (key >> 11);
	key = key + (key << 6);
	key = key ^ (key >> 22);
	return (unsigned int) key;
}
