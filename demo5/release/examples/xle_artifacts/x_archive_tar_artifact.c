#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

static void x_archive_tar_write_octal(char* out, int len, unsigned long value)
{
	snprintf(out, (size_t)len, "%0*lo", len - 1, value);
	out[len - 1] = '\0';
}

static unsigned long x_archive_tar_read_octal(const char* text, int len)
{
	unsigned long value = 0;
	int i;

	for ( i = 0; i < len; ++i ) {
		char ch = text[i];
		if ( (ch < '0') || (ch > '7') ) {
			continue;
		}
		value = (value << 3) + (unsigned long)(ch - '0');
	}
	return value;
}

static int x_archive_tar_is_zero_block(const unsigned char* block)
{
	int i;

	for ( i = 0; i < 512; ++i ) {
		if ( block[i] != 0 ) {
			return 0;
		}
	}
	return 1;
}

XL_EXPORT int x_archive_tar_write_test_tar(const char* path, const char* name, const char* text)
{
	FILE* fp;
	unsigned char header[512];
	unsigned char zero[512];
	unsigned int checksum;
	size_t text_len;
	size_t pad_len;
	size_t i;

	if ( (path == NULL) || (name == NULL) || (text == NULL) || (name[0] == '\0') ) {
		return 0;
	}
	fp = fopen(path, "wb");
	if ( fp == NULL ) {
		return 0;
	}

	memset(header, 0, sizeof(header));
	memset(zero, 0, sizeof(zero));
	snprintf((char*)header, 100, "%s", name);
	x_archive_tar_write_octal((char*)header + 100, 8, 0644);
	x_archive_tar_write_octal((char*)header + 108, 8, 0);
	x_archive_tar_write_octal((char*)header + 116, 8, 0);
	text_len = strlen(text);
	x_archive_tar_write_octal((char*)header + 124, 12, (unsigned long)text_len);
	x_archive_tar_write_octal((char*)header + 136, 12, 0);
	memset(header + 148, ' ', 8);
	header[156] = '0';
	memcpy(header + 257, "ustar", 5);
	memcpy(header + 263, "00", 2);

	checksum = 0;
	for ( i = 0; i < sizeof(header); ++i ) {
		checksum += header[i];
	}
	x_archive_tar_write_octal((char*)header + 148, 8, checksum);
	header[154] = '\0';
	header[155] = ' ';

	if ( fwrite(header, 1, sizeof(header), fp) != sizeof(header) ) {
		fclose(fp);
		return 0;
	}
	if ( fwrite(text, 1, text_len, fp) != text_len ) {
		fclose(fp);
		return 0;
	}
	pad_len = (512 - (text_len % 512)) % 512;
	if ( (pad_len != 0) && (fwrite(zero, 1, pad_len, fp) != pad_len) ) {
		fclose(fp);
		return 0;
	}
	if ( fwrite(zero, 1, sizeof(zero), fp) != sizeof(zero) ) {
		fclose(fp);
		return 0;
	}
	if ( fwrite(zero, 1, sizeof(zero), fp) != sizeof(zero) ) {
		fclose(fp);
		return 0;
	}
	fclose(fp);
	return 1;
}

XL_EXPORT int x_archive_tar_count(const char* path)
{
	FILE* fp;
	unsigned char header[512];
	unsigned long size;
	int count;

	if ( path == NULL ) {
		return 0;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		return 0;
	}

	count = 0;
	while ( fread(header, 1, sizeof(header), fp) == sizeof(header) ) {
		if ( x_archive_tar_is_zero_block(header) ) {
			break;
		}
		count += 1;
		size = x_archive_tar_read_octal((const char*)header + 124, 12);
		if ( fseek(fp, (long)(((size + 511u) / 512u) * 512u), SEEK_CUR) != 0 ) {
			break;
		}
	}
	fclose(fp);
	return count;
}

XL_EXPORT const char* x_archive_tar_first_name(const char* path)
{
	static char name[101];
	FILE* fp;
	unsigned char header[512];

	name[0] = '\0';
	if ( path == NULL ) {
		return name;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		return name;
	}
	if ( fread(header, 1, sizeof(header), fp) == sizeof(header) ) {
		if ( !x_archive_tar_is_zero_block(header) ) {
			memcpy(name, header, 100);
			name[100] = '\0';
		}
	}
	fclose(fp);
	return name;
}
