#define _CRT_SECURE_NO_WARNINGS

#define CUTE_SID_IMPLEMENTATION
#include <cute_sid.h>

#define CUTE_FILES_IMPLEMENTATION
#include <cute_files.h>

#include <string.h>
#include <stdlib.h>

char* strcatdup(const char* a, const char* b)
{
    int len_a = strlen(a);
    int len_b = strlen(b);
    char* c = (char*)malloc(len_a + len_b + 1);
    memcpy(c, a, len_a);
    memcpy(c + len_a, b, len_b + 1);
    return c;
}

void CB_DoPreprocess(cf_file_t* file, void* udata)
{
	(void)udata;
	char* out = strcatdup(file->path, ".preprocessed");
	sid_preprocess(file->path, out);
	free(out);
}

int main(int argc, const char** argv)
{
	if (argc != 2)
	{
		printf("Incorrect parameter usage. Should only pass the path to source directory.\n");
		return -1;
	}

	cf_traverse(argv[1], CB_DoPreprocess, 0);
	return 0;
}
