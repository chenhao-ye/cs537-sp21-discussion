#include <stdio.h>

int inc(int* p) {
	int x = *p;
	return x+1;
}

int main() {
	int y = 4;
	int z = inc(&y);
	printf("z: %d\n", z);
	return 0;
}
