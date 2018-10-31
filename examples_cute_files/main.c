#ifdef _WIN32
#include <Windows.h>
#endif

#define CUTE_FILES_IMPLEMENTATION
#include "cute_files.h"

#include <stdio.h>

void print_dir(cf_file_t* file, void* udata)
{
	printf("name: %-10s\text: %-10s\tpath: %s\n", file->name, file->ext, file->path);
	*(int*)udata += 1;
}

void test_dir()
{
	int n = 0;
	cf_traverse(".", print_dir, &n);
	printf("Found %d files with cf_traverse\n\n", n);

	cf_dir_t dir;
	cf_dir_open(&dir, "a");

	while (dir.has_next)
	{
		cf_file_t file;
		cf_read_file(&dir, &file);
		printf("%s\n", file.name);
		cf_dir_next(&dir);
	}

	cf_dir_close(&dir);
}


int main()
{
	test_dir();
	return 0;
}
