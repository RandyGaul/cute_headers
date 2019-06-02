#define ASSETSYS_IMPLEMENTATION
#include <assetsys.h>

#define CUTE_FILEWATCH_IMPLEMENTATION
#include <cute_filewatch.h>

void watch_callback(filewatch_update_t change, const char* virtual_path, void* udata)
{
	const char* change_string = 0;
	switch (change)
	{
	case FILEWATCH_DIR_ADDED: change_string = "FILEWATCH_DIR_ADDED"; break;
	case FILEWATCH_DIR_REMOVED: change_string = "FILEWATCH_DIR_REMOVED"; break;
	case FILEWATCH_FILE_ADDED: change_string = "FILEWATCH_FILE_ADDED"; break;
	case FILEWATCH_FILE_REMOVED: change_string = "FILEWATCH_FILE_REMOVED"; break;
	case FILEWATCH_FILE_MODIFIED: change_string = "FILEWATCH_FILE_MODIFIED"; break;
	}

	printf("%s at %s.\n", change_string, virtual_path);
}

int main()
{
	assetsys_t* assetsys = assetsys_create(0);
	filewatch_t* filewatch = filewatch_create(assetsys, 0);

	filewatch_mount(filewatch, "./watch_me", "/data");
	filewatch_mount(filewatch, "./also_watch_me", "/data");
	filewatch_start_watching(filewatch, "/data", watch_callback, 0);

	while (!GetAsyncKeyState(VK_ESCAPE))
	{
		filewatch_update(filewatch);
		filewatch_notify(filewatch);
		Sleep(100);
	}

	filewatch_stop_watching(filewatch, "/data");

	filewatch_free(filewatch);
	assetsys_destroy(assetsys);

	return 0;
}
