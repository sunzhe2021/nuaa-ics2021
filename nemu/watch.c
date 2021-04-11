#include<stdio.h>
int main() {
	int i, j = 0;
	for(i = 0; i < 10; i++) {
		j++;
		printf("now i = %d, j = %d\n", i, j);
	}
	return 0;
}
