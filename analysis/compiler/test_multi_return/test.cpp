float calc(float a) {
  if (a* a > 1.2) {
	return 1;
  } else if (a > 1 && a < 1.2) {
	return 0;
  } else {
	return 2;
  }
}



int main() {
  float a = 1.0;
  float  b = calc(a);
  return 0;
}
