int main() {
	float a = 1;
	for (int i = 0; i < 10; ++i) {
		a++;
		for (int j = 0; j < 5; ++j) {
			a *= a;
		}
		a *= (a-1);
	}
}
