#define __LIBRARY__
#include <unistd.h>
#include <stdio.h>

#define UNAME_LEN_2     23
char user_name2[UNAME_LEN_2];

_syscall2(int,whoami,char*,name,unsigned int,size)

int main()
{
    int res = 0;
    res = whoami(user_name2, UNAME_LEN_2);
	if(res != -1)
    	printf("%s\n", user_name2);
    return res;
}

