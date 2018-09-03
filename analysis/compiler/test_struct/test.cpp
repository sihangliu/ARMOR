#include <cstdlib>
#include "st.h"

int main() {
  st s; // = (st*) malloc (10 * sizeof(st));
	s.a++;
	int a = 0;
	func(a, &s);
}
