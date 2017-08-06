#include <stdio.h>

int main(int argc, char *argv[])
{
	int flg = 0, c;
	while((c = getchar()) != EOF){
		if(flg == 0 && c == '/') flg = 1;
		else if(flg == 1){
			if(c == '/') flg = 2;
			else {
				putchar('/');
				putchar(c);
				flg = 0;
			}
		}
		else if(flg == 2){
			if(c == '\n'){
				putchar(c);
				flg = 0;
			}
		} else{
			putchar(c);
		}
	}
	return 0;
}
