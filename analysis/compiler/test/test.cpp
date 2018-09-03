#include <cstdlib>

typedef float fptype;

float ckSub(float a, float b) {
	return a - b;
}


int main(const int argc, const char** argv) {
	fptype a = atoi(argv[1]),  b = atoi(argv[2]);
	for (int i = 0; i < 10; ++i) {
		if (b == 0) {
			a =  a * a * a;
		} else {
			if (a != 0)
				a = a / b;
		}
		a++;
	}

	b = a * (a + b * a * a);
	
	for (int i = 0; i < 10; ++i) {
		b ++;
	}
	b = b * 1.2351;
	b = b + (a + 1.0);
	b = b * b;
	b += 0.998;	

	b = ckSub(a, b);
}
