#define CUTE_HUFF_IMPLEMENTATION
#include "../cute_huff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
	// allocate scratch memory for building shared keysets
	int scratch_bytes = CUTE_HUFF_SCRATCH_MEMORY_BYTES;
	void* scratch_memory = malloc(scratch_bytes);
	huff_key_t compress;
	huff_key_t decompress;

	// the data to compress
	const char* string = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Cras nec faucibus leo. Praesent risus tellus, dictum ut ipsum vitae, fringilla elementum justo. Sed placerat, mauris ac elementum rhoncus, dui ipsum tincidunt dolor, eu vehicula ipsum arcu vitae turpis. Vivamus pulvinar odio non orci sodales, at dictum ex faucibus. Donec ornare a dolor vel malesuada. Donec dapibus, mauris malesuada imperdiet hendrerit, nisl dui rhoncus nisi, ac gravida quam nulla at tellus. Praesent auctor odio vel maximus tempus. Sed luctus cursus varius. Morbi placerat ipsum quis velit gravida rhoncus. Nunc malesuada urna nisl, nec facilisis diam tincidunt at. Aliquam condimentum nulla ac urna feugiat tincidunt. Nullam semper ullamcorper scelerisque. Nunc condimentum consectetur magna, sed aliquam risus tempus vitae. Praesent ornare id massa a facilisis. Quisque mollis tristique dolor. Morbi ut velit quis augue placerat sollicitudin a eu massa.";
	int bytes = strlen(string) + 1;

	// construct compression and decompression shared keyset
	int ret = huff_build_keys(&compress, &decompress, string, bytes, scratch_memory);
	if (!ret)
	{
		printf("thBuildKeys failed: %s", huff_error_reason);
		return -1;
	}

	// compute size of data after it would be compressed
	int compressed_bits = huff_compressed_size(&compress, string, bytes);
	int compressed_bytes = (compressed_bits + 7) / 8;
	void* compressed_buffer = malloc(compressed_bytes);

	// do compression
	ret = huff_compress(&compress, string, bytes, compressed_buffer, compressed_bytes);
	if (!ret)
	{
		printf("thCompress failed: %s", huff_error_reason);
		return -1;
	}


	// do decompression
	char* buf = (char*)malloc(bytes);
	huff_decompress(&decompress, compressed_buffer, compressed_bits, buf, bytes);

	if (strcmp(buf, string))
	{
		printf("String mismatch. Something bad happened");
		return -1;
	}

	return 0;
}
