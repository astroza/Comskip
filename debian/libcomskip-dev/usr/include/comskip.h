#ifndef COMSKIP
#define COMSKIP

#ifdef __cplusplus
extern "C"{
#endif 

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

//#define restrict
//#include <libavcodec/ac3dec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>

typedef struct 
{
    AVPacket *packet;
    AVPacket pkt1;
    int result;
    double old_clock;
    int empty_packet_count;
    int64_t last_packet_pos;
    int64_t last_packet_pts;
    double retry_target;
} comskip_decode_state;

void comskip_decode_init(comskip_decode_state *, int, char **);
void comskip_decode_loop(comskip_decode_state *state);
void comskip_decode_finish(comskip_decode_state *state);
FILE *LoadSettings(int argc, char ** argv);
void file_open();
void file_close();
void set_output_callback(void (*)(double, double, int, void *), void *);

#ifdef __cplusplus
}
#endif

#endif
#ifdef _WIN32
#define PACKAGE_STRING " Comskip 0.81.093"
#endif
#define _UNICODE
