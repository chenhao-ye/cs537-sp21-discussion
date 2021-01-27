#include <stdio.h>
#include <stdlib.h>

void buf_overflow() {
	int array[] = {0, 1, 2, 3};
	for (int i = 0; i < 5; ++i)
		printf("%d\n", array[i]);
	return;
}

int main() {
	buf_overflow();
	return 0;
}
