#include <dirent.h>
#include <string.h>
void* dw_open(const char* p){return opendir(p);}
const char* dw_next(void* d){struct dirent* e=readdir((DIR*)d);return e?e->d_name:0;}
void dw_close(void* d){closedir((DIR*)d);}
