#include <stdio.h>
#include <stdlib.h>

void leak() {
	int* p = malloc(sizeof(int));
	return;
}

int main() {
	leak();
	return 0;
}
