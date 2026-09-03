#include "/project/prove-programmi/array/math/mate.h"
#include <stdlib.h>

int add(int a, int b) {
	return a+b;
}

/*
	Calcolo matrice quadrata x vettore
*/

int *moltmatQ_vet(int **a, int *b, int SIZE_M, int SIZE_V) {
	
	int *c = malloc(sizeof(int) * SIZE_V);
	if (c == NULL) {
		return NULL;
	}
	
	for (int i = 0; i < SIZE_M; i++){
		c[i] = 0;
		for (int j = 0; j < SIZE_M; j++) {
			c[i] = c[i] + a[i][j] * b[j];
		}
	}
	
	return c;
}
