#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

static void x_compress_gzip_write_u16(FILE* fp, unsigned int value)
{
	unsigned char bytes[2];

	bytes[0] = (unsigned char)(value & 0xffu);
	bytes[1] = (unsigned char)((value >> 8) & 0xffu);
	(void)fwrite(bytes, 1, sizeof(bytes), fp);
}

static void x_compress_gzip_write_u32(FILE* fp, unsigned long value)
{
	unsigned char bytes[4];

	bytes[0] = (unsigned char)(value & 0xffu);
	bytes[1] = (unsigned char)((value >> 8) & 0xffu);
	bytes[2] = (unsigned char)((value >> 16) & 0xffu);
	bytes[3] = (unsigned char)((value >> 24) & 0xffu);
	(void)fwrite(bytes, 1, sizeof(bytes), fp);
}

static unsigned long x_compress_gzip_read_u32(const unsigned char* bytes)
{
	return (unsigned long)bytes[0] |
		((unsigned long)bytes[1] << 8) |
		((unsigned long)bytes[2] << 16) |
		((unsigned long)bytes[3] << 24);
}

static unsigned long x_compress_gzip_crc32(const unsigned char* data, size_t size)
{
	unsigned long crc = 0xffffffffUL;
	size_t i;
	int bit;

	for ( i = 0; i < size; ++i ) {
		crc ^= (unsigned long)data[i];
		for ( bit = 0; bit < 8; ++bit ) {
			crc = (crc >> 1) ^ (0xedb88320UL & (0UL - (crc & 1UL)));
		}
	}
	return crc ^ 0xffffffffUL;
}

XL_EXPORT int x_compress_gzip_write_test_gzip(const char* path, const char* text)
{
	FILE* fp;
	unsigned char header[10];
	unsigned long text_len;
	unsigned long crc;

	if ( (path == NULL) || (text == NULL) ) {
		return 0;
	}
	text_len = (unsigned long)strlen(text);
	if ( text_len > 65535UL ) {
		return 0;
	}
	fp = fopen(path, "wb");
	if ( fp == NULL ) {
		return 0;
	}

	header[0] = 0x1f;
	header[1] = 0x8b;
	header[2] = 8;
	header[3] = 0;
	header[4] = 0;
	header[5] = 0;
	header[6] = 0;
	header[7] = 0;
	header[8] = 0;
	header[9] = 255;
	if ( fwrite(header, 1, sizeof(header), fp) != sizeof(header) ) {
		fclose(fp);
		return 0;
	}

	fputc(1, fp);
	x_compress_gzip_write_u16(fp, (unsigned int)text_len);
	x_compress_gzip_write_u16(fp, (unsigned int)(~text_len));
	if ( fwrite(text, 1, text_len, fp) != text_len ) {
		fclose(fp);
		return 0;
	}

	crc = x_compress_gzip_crc32((const unsigned char*)text, (size_t)text_len);
	x_compress_gzip_write_u32(fp, crc);
	x_compress_gzip_write_u32(fp, text_len);
	fclose(fp);
	return 1;
}

XL_EXPORT int x_compress_gzip_is_gzip(const char* path)
{
	FILE* fp;
	unsigned char header[3];
	int ok;

	if ( path == NULL ) {
		return 0;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		return 0;
	}
	ok = (fread(header, 1, sizeof(header), fp) == sizeof(header)) &&
		(header[0] == 0x1f) &&
		(header[1] == 0x8b) &&
		(header[2] == 8);
	fclose(fp);
	return ok ? 1 : 0;
}

XL_EXPORT int x_compress_gzip_uncompressed_size(const char* path)
{
	FILE* fp;
	unsigned char size_bytes[4];
	long file_size;
	int out;

	if ( path == NULL ) {
		return 0;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		return 0;
	}
	if ( fseek(fp, 0, SEEK_END) != 0 ) {
		fclose(fp);
		return 0;
	}
	file_size = ftell(fp);
	if ( file_size < 18 ) {
		fclose(fp);
		return 0;
	}
	if ( fseek(fp, file_size - 4, SEEK_SET) != 0 ) {
		fclose(fp);
		return 0;
	}
	if ( fread(size_bytes, 1, sizeof(size_bytes), fp) != sizeof(size_bytes) ) {
		fclose(fp);
		return 0;
	}
	fclose(fp);
	out = (int)x_compress_gzip_read_u32(size_bytes);
	return out;
}
