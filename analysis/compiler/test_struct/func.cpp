#include "st.h"

void func(int in_a, st* approx_in) {
		float a, b, c;
		a = approx_in->a;
		b = approx_in->b;
		c = approx_in->c;
		a *= a;
		b += a;
		c = (a + b) * a * (b + c);
		approx_in->c = c;
}

