#include <cstdlib>
#include <cmath>
void TestFuncPtr(float* a) {
	for (int i = 0; i < 10; ++i) 
		*a *= *a;

	float tmp = *a;
	*a = sqrt(tmp*tmp);
}

float TestFuncVal(float b) {
	float rtn;
	//for (int i = 0; i < 10; ++i) 
	//	b *= b;
  
	rtn = exp(-b * b);
	rtn = (rtn + 1.1) * (rtn + b);
	return rtn;
}

void TestFuncRef(float &a) {
	for (int i = 0; i < 10; ++i) 
		a *= a;
}

int main () {
	float* x = (float*) malloc(10 * sizeof(float));
	//float* y = (float*) malloc(10 * sizeof(float));
	float y = 0.01;
	//x[2] = log(x[2] / y);
	
	//y[1] *= y[1];
	//float x2 = TestFuncVal(x[2]);
	//float x1 = TestFuncVal(x[1]);
	//x[2] = x2;
	y *= (y + x[2]);
	TestFuncPtr(&y);
	y *= y;
}
