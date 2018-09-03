#include <cstdlib>
int main() {
	float *a, *b, *c, *d;
	int size = 100;
	a = (float*) malloc (size);
	b = (float*) malloc (size);
	c = (float*) malloc (size);
	d = (float*) malloc (size);

	for (int i = 0; i < size - 2; ++i) { 
		c[i] = a[i + 1] * b[i + 2];
	}

	for (int i = 0; i < size - 2; ++i) { 
		a[i] = a[i + 1] * c[i + 2];
	}
	a[1] *= a[1];
	*(d + 3) = *a ;
	d[3] *= d[3];
}
