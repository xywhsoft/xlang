#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

static int x_image_bmp_read_header(const char* path, int* width, int* height)
{
	FILE* fp;
	unsigned char header[26];
	int raw_height;

	if ( width != NULL ) {
		*width = 0;
	}
	if ( height != NULL ) {
		*height = 0;
	}
	if ( path == NULL ) {
		return 0;
	}

	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		return 0;
	}
	if ( fread(header, 1, sizeof(header), fp) != sizeof(header) ) {
		fclose(fp);
		return 0;
	}
	fclose(fp);

	if ( (header[0] != 'B') || (header[1] != 'M') ) {
		return 0;
	}
	if ( width != NULL ) {
		*width = (int)((unsigned int)header[18] | ((unsigned int)header[19] << 8) | ((unsigned int)header[20] << 16) | ((unsigned int)header[21] << 24));
	}
	raw_height = (int)((unsigned int)header[22] | ((unsigned int)header[23] << 8) | ((unsigned int)header[24] << 16) | ((unsigned int)header[25] << 24));
	if ( height != NULL ) {
		*height = (raw_height < 0) ? -raw_height : raw_height;
	}
	return 1;
}

XL_EXPORT int x_image_bmp_is_bmp(const char* path)
{
	return x_image_bmp_read_header(path, NULL, NULL);
}

XL_EXPORT int x_image_bmp_width(const char* path)
{
	int width;

	if ( !x_image_bmp_read_header(path, &width, NULL) ) {
		return 0;
	}
	return width;
}

XL_EXPORT int x_image_bmp_height(const char* path)
{
	int height;

	if ( !x_image_bmp_read_header(path, NULL, &height) ) {
		return 0;
	}
	return height;
}

XL_EXPORT int x_image_bmp_write_test_bmp(const char* path, int width, int height)
{
	FILE* fp;
	unsigned char header[26];

	if ( (path == NULL) || (width <= 0) || (height <= 0) ) {
		return 0;
	}
	fp = fopen(path, "wb");
	if ( fp == NULL ) {
		return 0;
	}

	for ( int i = 0; i < (int)sizeof(header); ++i ) {
		header[i] = 0;
	}
	header[0] = 'B';
	header[1] = 'M';
	header[2] = 26;
	header[10] = 26;
	header[14] = 12;
	header[18] = (unsigned char)(width & 0xff);
	header[19] = (unsigned char)((width >> 8) & 0xff);
	header[20] = (unsigned char)((width >> 16) & 0xff);
	header[21] = (unsigned char)((width >> 24) & 0xff);
	header[22] = (unsigned char)(height & 0xff);
	header[23] = (unsigned char)((height >> 8) & 0xff);
	header[24] = (unsigned char)((height >> 16) & 0xff);
	header[25] = (unsigned char)((height >> 24) & 0xff);

	if ( fwrite(header, 1, sizeof(header), fp) != sizeof(header) ) {
		fclose(fp);
		return 0;
	}
	fclose(fp);
	return 1;
}
