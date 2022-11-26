#include<stdio.h>
#include<stdlib.h>

void myprint(int l)
{
	if(l < 10) printf("00");
	if(l >=10 && l <100) printf("0");
	printf("%d",l);

}

int main(int argv, char** argc)
{
	int e = atoi(argc[1]);
	int ws = atoi(argc[2]);
	for(int i = 0; i < e; i++)
	{
		int l = rand() % 1000;
		int r = rand() % 1000;
		int w = rand() % 1000;
		myprint(l);
		if(ws == 1) printf(" ");
		myprint(r);
		if(ws == 1) printf(" ");
		myprint(w);
		if(ws == 1) printf("\n");
	}
	printf(" ");
	for(int i = 0; i < 1000;i++) printf("x");
	printf("\n");


	return 0;
}
