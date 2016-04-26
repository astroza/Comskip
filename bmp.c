/* (C) 2016 - Felipe Astroza (felipe@astroza.cl)
  */

#include "bmp.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef DEBUG
#include <stdio.h>
#endif


static int _width_padding(int width, unsigned int bytes_per_pixel) 
{
	return (4 - (width * bytes_per_pixel) % 4) % 4;
}

bitmap *bitmap_new(int width, int height, unsigned short deep)
{
	unsigned int bitmap_file_size;
	bitmap *bmap;

	bitmap_file_size = sizeof(bitmap) + width * height * (deep/8) + _width_padding(width, deep/8) * height;
#ifdef DEBUG
	printf("headers size: %ld, file size: %d\n", sizeof(bitmap), bitmap_file_size);
#endif
	bmap = (bitmap *)malloc(bitmap_file_size);
	if(bmap == NULL)
		return NULL;

	bmap->file_header.type = BITMAP_TYPE;
	bmap->file_header.size = bitmap_file_size;
	bmap->file_header.reserved1 = 0;
	bmap->file_header.reserved2 = 0;
	bmap->file_header.offset = sizeof(bitmap);

	bmap->info_header.header_size = sizeof(bitmap_info_header);
	bmap->info_header.image_width = width;
	bmap->info_header.image_height = height;
	bmap->info_header.color_planes = 1;
	bmap->info_header.bits_per_pixel = deep;
	bmap->info_header.compression_type = CT_RGB;
	bmap->info_header.image_size = 0;
	bmap->info_header.x_ppm = 0;
	bmap->info_header.y_pp = 0;
	bmap->info_header.colors = 0;
	bmap->info_header.important_colors = 0;

	return bmap;
}

static char *_pixel_pos(bitmap *bmap, int x, int y, unsigned short *ret_bpp)
{
	unsigned int ret;
	unsigned short bytes_per_pixel;
	int image_offset;

	image_offset = bmap->file_header.offset - sizeof(bitmap);
	bytes_per_pixel = bmap->info_header.bits_per_pixel/8;
	char *pos = image_offset + bmap->image_data + (bmap->info_header.image_height - y - 1) * (bmap->info_header.image_width * bytes_per_pixel + _width_padding(bmap->info_header.image_width, bytes_per_pixel)) + x * bytes_per_pixel;

	*ret_bpp = bytes_per_pixel;
	return pos;
}

int bitmap_read_pixel(bitmap *bmap, int x, int y) 
{
	int ret, i;
	unsigned short bytes_per_pixel;
	char *pos = _pixel_pos(bmap, x, y, &bytes_per_pixel);

	ret = 0;
	for(i = 0; i < bytes_per_pixel; i++) {
		// little endian 
		ret |= (pos[i] & 0xff) << i*8;
	}
	return ret;
}

void bitmap_write_pixel(bitmap *bmap, int x, int y, int value)
{
	int i;
	unsigned short bytes_per_pixel;
	char *pos = _pixel_pos(bmap, x, y, &bytes_per_pixel);

	for(i = 0; i < bytes_per_pixel; i++) {
		// little endian
		pos[i] = (char)((value & (0xff << i*8)) >> i*8);
	}
}
void bitmap_destroy(bitmap *bmap)
{
	free(bmap);
}

int bitmap_save(bitmap *bmap, const char *filename) 
{
	int fd;

	fd = open(filename, O_CREAT|O_WRONLY|O_TRUNC, 0644);
	if(fd == -1)
		return -1;

	if(write(fd, (void *)bmap, bmap->file_header.size) == -1) {
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

bitmap *bitmap_load(const char *filename) 
{
	bitmap_file_header file_header;
	bitmap *bmap = NULL;
	int fd;

	fd = open(filename, O_RDONLY);
	if(fd == 0)
		return NULL;

	if(read(fd, (void *)&file_header, sizeof(file_header)) != sizeof(file_header))
		goto error;

	bmap = (bitmap *)malloc(file_header.size);
	if(bmap == NULL)
		goto error;

	if(read(fd, (void *)&bmap->info_header, file_header.size - sizeof(file_header)) == -1)
		goto error;

	bmap->file_header = file_header;
	close(fd);
	return bmap;

error:
	close(fd);
	if(bmap)
		free(bmap);
	return NULL;
}

#ifdef TEST
#include <string.h>
int main(int c, char **v)
{
	bitmap *bmap;
	int x, y;
	int value;

	if(c < 2)
		return 1;

	if(strcmp(v[1], "save") == 0) {
		bmap = bitmap_new(200, 200, 24);
		for(y = 0; y < 200; y++)
			for(x = 0; x < 200; x++)
				bitmap_write_pixel(bmap, x, y, x+y);
		bitmap_save(bmap, "test.bmp");
		bitmap_destroy(bmap);
	}

	if(strcmp(v[1], "load") == 0) {
		bmap = bitmap_load("test.bmp");
		if(bmap == NULL) {
			printf("can't load bmp");
			return 1;
		}
       	for(y = 0; y < 200; y++)
               	for(x = 0; x < 200; x++) {
			value = bitmap_read_pixel(bmap, x, y);
			if(value != x+y) {
				printf("unexpected pixel (%d, %d)=0x%x\n", x, y, value);
			}
		}
		bitmap_destroy(bmap);
	}
	return 0;
}
#endif
