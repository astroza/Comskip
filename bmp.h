#ifndef BMP_H
#define BMP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __attribute__((packed)) {
    unsigned short type;           /* Magic number for file */
    unsigned int   size;           /* Size of file */
    unsigned short reserved1;      /* Reserved */
    unsigned short reserved2;      /* ... */
    unsigned int   offset;         /* Offset to bitmap data */
} bitmap_file_header;

#define BITMAP_TYPE 0x4D42          /* "MB" */

typedef struct __attribute__((packed)) {
    unsigned int   header_size;
    int            image_width;
    int            image_height;
    unsigned short color_planes; 
    unsigned short bits_per_pixel;
    unsigned int   compression_type; 
    unsigned int   image_size; 
    int            x_ppm;               /* X pixels per meter */
    int            y_pp;               /* Y pixels per meter */
    unsigned int   colors;
    unsigned int   important_colors; 
} bitmap_info_header;

typedef struct {
    bitmap_file_header file_header;
    bitmap_info_header info_header;
    char image_data[];
} bitmap;

bitmap *bitmap_new(int width, int height, unsigned short deep);
int bitmap_read_pixel(bitmap *bmap, int x, int y);
void bitmap_write_pixel(bitmap *bmap, int x, int y, int value);
void bitmap_destroy(bitmap *bmap);
int bitmap_save(bitmap *bmap, const char *filename);
bitmap *bitmap_load(const char *filename);


#ifdef __cplusplus
}
#endif


/*
 * Constants for compression_type
 */
#define CT_RGB       0             /* No compression - straight BGR data */
#define CT_RLE8      1             /* 8-bit run-length compression */
#define CT_RLE4      2             /* 4-bit run-length compression */
#define CT_BITFIELDS 3             /* RGB bitmap with RGB masks */


#endif
