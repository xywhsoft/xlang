#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

static void x_archive_zip_write_u16(FILE* fp, unsigned int value)
{
	unsigned char bytes[2];

	bytes[0] = (unsigned char)(value & 0xffu);
	bytes[1] = (unsigned char)((value >> 8) & 0xffu);
	(void)fwrite(bytes, 1, sizeof(bytes), fp);
}

static void x_archive_zip_write_u32(FILE* fp, unsigned long value)
{
	unsigned char bytes[4];

	bytes[0] = (unsigned char)(value & 0xffu);
	bytes[1] = (unsigned char)((value >> 8) & 0xffu);
	bytes[2] = (unsigned char)((value >> 16) & 0xffu);
	bytes[3] = (unsigned char)((value >> 24) & 0xffu);
	(void)fwrite(bytes, 1, sizeof(bytes), fp);
}

static unsigned int x_archive_zip_read_u16(const unsigned char* bytes)
{
	return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
}

static unsigned long x_archive_zip_read_u32(const unsigned char* bytes)
{
	return (unsigned long)bytes[0] |
		((unsigned long)bytes[1] << 8) |
		((unsigned long)bytes[2] << 16) |
		((unsigned long)bytes[3] << 24);
}

static unsigned long x_archive_zip_crc32(const unsigned char* data, size_t size)
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

XL_EXPORT int x_archive_zip_write_test_zip(const char* path, const char* name, const char* text)
{
	FILE* fp;
	unsigned long crc;
	unsigned long text_len;
	unsigned long name_len;
	unsigned long central_offset;
	unsigned long central_size;

	if ( (path == NULL) || (name == NULL) || (text == NULL) || (name[0] == '\0') ) {
		return 0;
	}
	fp = fopen(path, "wb");
	if ( fp == NULL ) {
		return 0;
	}

	text_len = (unsigned long)strlen(text);
	name_len = (unsigned long)strlen(name);
	crc = x_archive_zip_crc32((const unsigned char*)text, (size_t)text_len);

	x_archive_zip_write_u32(fp, 0x04034b50UL);
	x_archive_zip_write_u16(fp, 20);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u32(fp, crc);
	x_archive_zip_write_u32(fp, text_len);
	x_archive_zip_write_u32(fp, text_len);
	x_archive_zip_write_u16(fp, (unsigned int)name_len);
	x_archive_zip_write_u16(fp, 0);
	(void)fwrite(name, 1, name_len, fp);
	(void)fwrite(text, 1, text_len, fp);

	central_offset = 30UL + name_len + text_len;
	x_archive_zip_write_u32(fp, 0x02014b50UL);
	x_archive_zip_write_u16(fp, 20);
	x_archive_zip_write_u16(fp, 20);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u32(fp, crc);
	x_archive_zip_write_u32(fp, text_len);
	x_archive_zip_write_u32(fp, text_len);
	x_archive_zip_write_u16(fp, (unsigned int)name_len);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u32(fp, 0);
	x_archive_zip_write_u32(fp, 0);
	(void)fwrite(name, 1, name_len, fp);

	central_size = 46UL + name_len;
	x_archive_zip_write_u32(fp, 0x06054b50UL);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u16(fp, 0);
	x_archive_zip_write_u16(fp, 1);
	x_archive_zip_write_u16(fp, 1);
	x_archive_zip_write_u32(fp, central_size);
	x_archive_zip_write_u32(fp, central_offset);
	x_archive_zip_write_u16(fp, 0);

	fclose(fp);
	return 1;
}

XL_EXPORT int x_archive_zip_count(const char* path)
{
	FILE* fp;
	unsigned char header[4];
	int count = 0;

	if ( path == NULL ) {
		return 0;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		return 0;
	}
	while ( fread(header, 1, sizeof(header), fp) == sizeof(header) ) {
		unsigned long sig = x_archive_zip_read_u32(header);
		if ( sig == 0x02014b50UL ) {
			count += 1;
		}
		if ( fseek(fp, -3L, SEEK_CUR) != 0 ) {
			break;
		}
	}
	fclose(fp);
	return count;
}

XL_EXPORT const char* x_archive_zip_first_name(const char* path)
{
	static char name[256];
	FILE* fp;
	unsigned char header[46];
	unsigned int name_len;

	name[0] = '\0';
	if ( path == NULL ) {
		return name;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		return name;
	}

	while ( fread(header, 1, 4, fp) == 4 ) {
		if ( x_archive_zip_read_u32(header) == 0x02014b50UL ) {
			if ( fread(header + 4, 1, 42, fp) != 42 ) {
				break;
			}
			name_len = x_archive_zip_read_u16(header + 28);
			if ( name_len >= sizeof(name) ) {
				name_len = (unsigned int)sizeof(name) - 1u;
			}
			if ( fread(name, 1, name_len, fp) != name_len ) {
				name[0] = '\0';
				break;
			}
			name[name_len] = '\0';
			break;
		}
		if ( fseek(fp, -3L, SEEK_CUR) != 0 ) {
			break;
		}
	}
	fclose(fp);
	return name;
}
