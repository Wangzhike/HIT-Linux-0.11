#define __LIBRARY__
#include <unistd.h>

#define UNAME_LEN   23
char user_name[UNAME_LEN];

_syscall1(int,iam,const char*,name)

int main(int argc, char** argv){
    int len =0;
    int res = 0;
    while( (user_name[len] = *((*(argv+1))+len)) != '\0')
		len++;
    res = iam(user_name);
    return res;
}

