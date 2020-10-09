#include <stdint.h>
#ifndef ART_H
#define ART_H

#ifdef __cplusplus
extern "C"{
#endif

#define NODE4 1
#define MAX_LEN 10


#if defined(__GNUC__) && !defined(__clang__)
# if __STDC_VERSION__ >= 199901L && 402 == (__GNUC__ * 100 + __GNUC_MINOR__)
/*
 *  * GCC 4.2.2's C99 inline keyword support is pretty broken; avoid. Introduced in
 *   * GCC 4.2.something, fixed in 4.3.0. So checking for specific major.minor of
 *    * 4.2 is fine.
 *     */
#  define BROKEN_GCC_C99_INLINE
#endif
#endif

typedef int(*art_callback)(void *data, const unsigned char *key, uint32_t key_len, void *value);

typedef struct {
	uint8_t num_children;
	unsigned char partial[MAX_LEN];	
	uint32_t partial_len;
	uint8_t type;
}art_node;


typedef struct {
	art_node n;
	unsigned char keys[4];
	art_node* children[4];
}art_node4;


typedef struct{
	void *value;
	uint32_t key_len;
	unsigned char key[];
}art_leaf;


typedef struct{
	art_node *root;
	uint64_t size;
}art_tree;


int art_tree_init(art_tree *t);
#define init_art_tree(...) art_tree_init(__VA_ARGS__)


#ifdef BROKEN_GCC_C99_INLINE
# define art_size(t) ((t)->size)
#else
inline uint64_t art_size(art_tree *t){
	return t->size;	
}
#endif

art_leaf* art_minimum(art_tree *t);


void* art_insert(art_tree *t,const unsigned char * key, int key_len, void* value);


int recurse(art_tree *t,void*data);

int recurse_iter(art_node* n, int depth);
#ifdef __cplusplus
}
#endif

#endif
