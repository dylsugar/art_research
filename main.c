#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include "art.h"



int main()
{
	art_tree t;
	int res = art_tree_init(&t);
	int len;
	char buf[10];
	FILE* f = fopen("words.txt","r");
	uintptr_t line = 1;
	while(fgets(buf, sizeof buf, f)){
		len = strlen(buf);
		buf[len-1] = '\0';
		art_insert(&t, (unsigned char*)buf, len, (void*)line);
		line++;
	}
	
	printf("tree size: %d\n",t.size);
	printf("\n\n");
	recurse(&t,(void*)line);
}
