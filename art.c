#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <assert.h>
#include "art.h"
#include <ctype.h>

#ifdef __i386__
    #include <emmintrin.h>
#else
#ifdef __amd64__
    #include <emmintrin.h>
#endif
#endif

#define IS_LEAF(x) (((uintptr_t)x & 1))
#define SET_LEAF(x) ((void*)((uintptr_t)x | 1))
#define LEAF_RAW(x) ((art_leaf*)((void*)((uintptr_t)x & ~1))) 

static art_node* alloc_node(uint8_t type ){
	art_node* n;
	if(type == NODE4){
		n = (art_node*)calloc(1,sizeof(art_node4));
	}
	n->type = type;
	return n;
}


int art_tree_init(art_tree *t){
	t->root = NULL;
	t->size = 0;
	return 0;
}

#ifndef BROKEN_GCC_C99_INLINE
extern inline uint64_t art_size(art_tree *t);
#endif

static art_node** find_child(art_node *n, unsigned char c){
	union{
		art_node4 *p1;
	} p;
	if(n->type == NODE4){
		 p.p1= (art_node4*)n;
		int i;
		for(i=0;i<n->num_children;i++){
			if (((unsigned char*)p.p1->keys)[i] == c)
				return &p.p1->children[i];
		}
	}
	return NULL;
}


static inline int min(int a, int b){
	return (a < b) ? a:b;
}


static int longest_common_prefix(art_leaf *l1, art_leaf *l2, int depth) {
	int max_cmp = min(l1->key_len, l2->key_len) - depth;
	int idx;
	for (idx=0; idx < max_cmp; idx++) {
		if (l1->key[depth+idx] != l2->key[depth+idx])
			return idx;
	}
}


static int check_prefix(const art_node *n, const unsigned char *key, int key_len, int depth){
	int maxcmp = min(min(n->partial_len, MAX_LEN), key_len - depth);	
	int idx;
	for(idx = 0; idx < maxcmp; idx++){
		if(n->partial[idx] != key[depth+idx])
			return idx;
	}
	return idx;
}

static int leaf_matches(const art_leaf *n, const unsigned char *key, int key_len, int depth) {
	(void)depth;
	if (n->key_len != (uint32_t)key_len) return 1;
	return memcmp(n->key, key, key_len);
}

static art_leaf* minimum(const art_node *n){
	if(!n) return NULL;
	if(IS_LEAF(n)) return LEAF_RAW(n);
	int idx;
	if(n->type == NODE4){
		return minimum(((const art_node4*)n)->children[0]);
	}
}

static art_leaf* maximum(const art_node *n){
	if(!n) return NULL;
	if(IS_LEAF(n)) return LEAF_RAW(n);
	if(n->type == NODE4){
		return maximum(((const art_node4*)n)->children[n->num_children-1]);
	}
}

art_leaf* art_minimum(art_tree *t){
	return minimum((art_node*)t->root);
}

art_leaf* art_maximum(art_tree *t){
	return maximum((art_node*)t->root);
}

static art_leaf* make_leaf(const unsigned char *key, int key_len, void *value){
	art_leaf *l = (art_leaf*)calloc(1, sizeof(art_leaf)+key_len);
	l->value = value;
	l->key_len = key_len;
	memcpy(l->key, key, key_len);
	return l;
}

static void addchild4(art_node4 *n, art_node **ref, unsigned char c, void *child){
	
	if(n->n.num_children < 4){
		int idx;
		for(idx = 0; idx < n->n.num_children;idx++){
			if(c < n->keys[idx]) break;
		}
		memmove(n->keys+idx+1,n->keys+idx,n->n.num_children-idx);
		memmove(n->children+idx+1,n->children+idx,(n->n.num_children - idx)*sizeof(void*));
		n->keys[idx] = c;
		n->children[idx] = (art_node*)child;
		n->n.num_children++;
	}
}

static int prefix_mismatch(const art_node *n, const unsigned char *key, int key_len, int depth) {
	int max_cmp = min(min(MAX_LEN, n->partial_len), key_len - depth);
	int idx;
	for(idx = 0; idx < max_cmp;idx++){
		if(n->partial[idx]!= key[depth+idx]){
			return idx;
		}
	}

	if(n->partial_len > MAX_LEN){
		art_leaf *l = minimum(n);
		max_cmp = min(l->key_len,key_len)- depth;
		for(;idx < max_cmp; idx++){
			if(l->key[idx+depth] != key[depth+idx]){
				return idx;
			}
		}
	}
	return idx;
}


static void* recursive_insert(art_node *n, art_node **ref, const unsigned char *key, int key_len, void *value, int depth, int *old) {
	
	if(!n){	
		*ref = (art_node*)SET_LEAF(make_leaf(key, key_len, value));
		return NULL;
	}	

	if(IS_LEAF(n)){
		art_leaf *l = LEAF_RAW(n);
		if (!leaf_matches(l,key, key_len, depth)) {
			*old = 1;
			void *old_val = l->value;
			l->value = value;
			return old_val;
		}
		art_node4 *new_node = (art_node4*)alloc_node(NODE4);
		art_leaf *l2 = make_leaf(key,key_len,value);
		int longest_prefix = longest_common_prefix(l,l2,depth);
		new_node->n.partial_len = longest_prefix;
		memcpy(new_node->n.partial, key+depth, min(MAX_LEN, longest_prefix));
		*ref = (art_node*)new_node;
		addchild4(new_node, ref, l->key[depth+longest_prefix], SET_LEAF(l));
		addchild4(new_node, ref, l2->key[depth+longest_prefix], SET_LEAF(l2));
	}

	if(n->partial_len){
		int prefix_diff = prefix_mismatch(n,key,key_len,depth);
		if ((uint32_t)prefix_diff >= n->partial_len) {
			depth += n->partial_len;
			goto RECURSE_SEARCH;
		}
		art_node4 *new_node = (art_node4*)alloc_node(NODE4);
		*ref = (art_node*)new_node;
		new_node->n.partial_len = prefix_diff;
		memcpy(new_node->n.partial, n->partial, min(MAX_LEN, prefix_diff));
		if (n->partial_len <= MAX_LEN) {
			addchild4(new_node, ref, n->partial[prefix_diff], n);
			n->partial_len -= (prefix_diff+1);
			memmove(n->partial, n->partial+prefix_diff+1,min(MAX_LEN, n->partial_len));		
		}else{
			n->partial_len -= (prefix_diff+1);
			art_leaf *l = minimum(n);
			addchild4(new_node, ref, l->key[depth+prefix_diff], n);
			memcpy(n->partial, l->key+depth+prefix_diff+1,min(MAX_LEN, n->partial_len));
		}
		art_leaf *l = make_leaf(key, key_len, value);
		addchild4(new_node, ref, key[depth+prefix_diff], SET_LEAF(l));
		return NULL;
	}	

RECURSE_SEARCH:;
	art_node **child = find_child(n, key[depth]);
	if(child){
		return recursive_insert(*child, child, key, key_len, value, depth+1, old);
	}
	art_leaf *l = make_leaf(key, key_len, value);
	addchild4((art_node4*)n, ref, key[depth], SET_LEAF(l));
	return NULL;

}


void* art_insert(art_tree *t, const unsigned char *key, int key_len, void *value){
	int old_val = 0;
	void *old = recursive_insert(t->root, &t->root, key, key_len, value, 0, &old_val);
	if (!old_val) t->size++;
	return old;
}

static char prefix[4096];
int recurse(art_tree *t,void* data){
	memset(prefix, ' ',4096);
	printf("		");
	for(int i = 0; i < t->root->partial_len;i++){
		printf("%c",t->root->partial[i]);
	}
	printf("\n");
	//printf("		     /");
	return recurse_iter(t->root,4);

}

int recurse_iter(art_node *n, int depth){
	if(!n)return 0;
	if(IS_LEAF(n)){
		art_leaf *l = LEAF_RAW(n);
		//printf("%.*s", depth, prefix);
		printf("  |   \n");
		printf("[%.*s]",l->key_len, l->key);
		printf("	");
		//printf("%d",depth);
		return 0;
	}
	int idx,res;
	        uint8_t num_ch = n->num_children;
		//printf("%.*s", depth, prefix);
		//printf("type%hhu partial len=%u %.*s\n", num_ch, n->partial_len, n->partial_len, n->partial);
	if(n->type == NODE4){
		for(int i = 0; i < num_ch;i++){
			//print("%.*s",depth,prefix);
			printf("  	       /\n");
			printf("	   [%c]\n",((art_node4*)n)->keys[i]);
			res = recurse_iter(((art_node4*)n)->children[i],depth+4);
			if(res){
				return res;
			}	
		}
	}
	return 0;
}		

