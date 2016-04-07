/*
 * mpeg2dec.c
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "platform.h"
#include "vo.h"
#ifdef HAVE_SDL
#include <SDL.h>
#endif
#include <argtable2.h>
#define SELFTEST

extern int pass;
extern double test_pts;

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>

typedef struct VideoPicture
{
    int width, height; /* source height & width */
    int allocated;
    double pts;
} VideoPicture;

typedef struct VideoState
{
    AVFormatContext *pFormatCtx;
    AVCodecContext *dec_ctx;
    int             videoStream, audioStream, subtitleStream;

    int             av_sync_type;
//     double          external_clock; /* external clock base */
//     int64_t         external_clock_time;
    int             seek_req;
    int             seek_by_bytes;
    int             seek_no_flush;
    double           seek_pts;
    int             seek_flags;
    int64_t          seek_pos;
    double          audio_clock;
    AVStream        *audio_st;
    AVStream        *subtitle_st;

    //DECLARE_ALIGNED(16, uint8_t, audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2]);
    unsigned int    audio_buf_size;
    unsigned int    audio_buf_index;
    AVPacket        audio_pkt;
    AVPacket        audio_pkt_temp;
//  uint8_t         *audio_pkt_data;
//  int             audio_pkt_size;
    int             audio_hw_buf_size;
    double          audio_diff_cum; /* used for AV difference average computation */
    double          audio_diff_avg_coef;
    double          audio_diff_threshold;
    int             audio_diff_avg_count;
    double          frame_timer;
    double          frame_last_pts;
    double          frame_last_delay;
    double          video_clock; ///<pts of last decoded frame / predicted pts of next decoded frame
    double          video_clock_submitted;
    double          video_current_pts; ///<current displayed pts (different from video_clock if frame fifos are used)
    int64_t         video_current_pts_time;  ///<time (av_gettime) at which we updated video_current_pts - used to have running video pts
    AVStream        *video_st;
    AVFrame         *pFrame;
    char            filename[1024];
    int             quit;
    AVFrame         *frame;
    double			 duration;
    double			 fps;
} VideoState;

extern VideoState      *is;

extern int height,width, videowidth;
extern int output_debugwindow;
extern int output_console;
extern int output_timing;
extern int output_srt;
extern int output_smi;

extern int retries;

extern char HomeDir[256];
extern int processCC;
extern int reorderCC;

static FILE * in_file;
static FILE * sample_file;
static FILE * timing_file = 0;

extern void set_fps(double frame_delay, double dfps, int ticks, double rfps, double afps);
extern void dump_video (char *start, char *end);
extern void dump_audio (char *start, char *end);
extern void	Debug(int level, char* fmt, ...);
extern void dump_video_start(void);
extern void dump_audio_start(void);
extern void file_open();
extern int DetectCommercials(int, double);
extern int BuildMasterCommList(void);
extern FILE* LoadSettings(int argc, char ** argv);
extern void ProcessCCData(void);
extern void dump_data(char *start, int length);
extern void close_data();
extern void DoSeekRequest(VideoState *is);
extern void file_close();
extern void Set_seek(VideoState *is, double pts);
extern int video_packet_process(VideoState *is,AVPacket *packet);
extern void audio_packet_process(VideoState *is, AVPacket *pkt);

extern int csRestart;
extern int framenum;
extern int selftest;
extern int live_tv;
extern double selftest_target;
extern int live_tv_retries;
extern int lastFrameCommCalculated;

char tempstring[512];
extern char	inbasename[];

extern VideoState *global_video_state;
extern AVPacket flush_pkt;

static int sound_frame_counter = 0;
static int max_volume_found = 0;

#define DUMP_HEADER if (timing_file) fprintf(timing_file, "sep=,\ntype   ,real_pts, step        ,pts         ,clock       ,delta       ,offset, repeat\n");
#define DUMP_OPEN if (output_timing) { sprintf(tempstring, "%s.timing.csv", inbasename); timing_file = myfopen(tempstring, "w"); DUMP_HEADER }
#define DUMP_CLOSE if (timing_file) { fclose(timing_file); timing_file = NULL; }

#ifdef PROCESS_CC
extern void CEW_reinit();
extern long process_block (unsigned char *data, long length);
#endif

int main (int argc, char ** argv)
{
    AVPacket pkt1, *packet = &pkt1;
    int result = 0;
    int ret;
    double tfps;
    double old_clock = 0.0;
                    int empty_packet_count = 0;

    int64_t last_packet_pos = 0;
    int64_t last_packet_pts = 0;
    double retry_target = 0.0;

#ifdef SELFTEST
    //int tries = 0;
#endif
    retries = 0;

    char *ptr;
    size_t len;

#ifdef __MSVCRT_VERSION__

    int i;
    int _argc = 0;
    wchar_t **_argv = 0;
    wchar_t **dummy_environ = 0;
    _startupinfo start_info;
    start_info.newmode = 0;
    __wgetmainargs(&_argc, &_argv, &dummy_environ, -1, &start_info);



    char *aargv[20];
    char (argt[20])[1000];

    argv = aargv;
    argc = _argc;

    for (i= 0; i< argc; i++)
    {
        argv[i] = &(argt[i][0]);
        WideCharToMultiByte(CP_UTF8, 0,_argv[i],-1, argv[i],  1000, NULL, NULL );
    }

#endif

#ifndef _DEBUG
//	__tr y
    {
        //      raise_ exception();
#endif

//		output_debugwindow = 1;

        if (strstr(argv[0],"comskipGUI"))
            output_debugwindow = 1;
        else
        {
#ifdef _WIN32
            //added windows specific
            SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif
        }
        //get path to executable
        ptr = argv[0];
        if (*ptr == '\"')
        {
            ptr++; //strip off quotation marks
            len = (size_t)(strchr(ptr,'\"') - ptr);
        }
        else
        {
            len = strlen(ptr);
        }
        strncpy(HomeDir, ptr, len);

        len = (size_t)max(0,strrchr(HomeDir,'\\') - HomeDir);
        if (len==0)
        {
            HomeDir[0] = '.';
            HomeDir[1] = '\0';
        }
        else
        {
            HomeDir[len] = '\0';
        }

        fprintf (stderr, "%s, made using ffmpeg\n", PACKAGE_STRING);

#ifndef DONATOR
        fprintf (stderr, "Public build\n");
#else
        fprintf (stderr, "Donator build\n");
#endif

#ifdef _WIN32
#ifdef HAVE_IO_H
//		_setmode (_fileno (stdin), O_BINARY);
//		_setmode (_fileno (stdout), O_BINARY);
#endif
#endif


#ifdef _WIN32
        //added windows specific
//		if (!live_tv) SetThreadPriority(GetCurrentThread(), /* THREAD_MODE_BACKGROUND_BEGIN */ 0x00010000); // This will fail in XP but who cares

#endif
        /*
        #define ES_AWAYMODE_REQUIRED    0x00000040
        #define ES_CONTINUOUS           0x80000000
        #define ES_SYSTEM_REQUIRED      0x00000001
        */

#if (_WIN32_WINNT >= 0x0500 || _WIN32_WINDOWS >= 0x0410)

        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);
#endif

//
// Wait until recording is complete...
//

//        av_log_set_level(AV_LOG_WARNING);
//        av_log_set_flags(AV_LOG_SKIP_REPEATED);
//        av_log_set_callback(log_callback_report);
//        av_log_set_level(AV_LOG_WARNING);
        in_file = LoadSettings(argc, argv);

        file_open();



        csRestart = 0;
        framenum = 0;

//        DUMP_OPEN

        if (output_timing)
        {
            sprintf(tempstring, "%s.timing.csv", inbasename);
            timing_file = myfopen(tempstring, "w");
            DUMP_HEADER
        }

        av_log_set_level(AV_LOG_INFO);
//        av_log_set_flags(AV_LOG_SKIP_REPEATED);
//        av_log_set_callback(log_callback_report);
        is = global_video_state;
        packet = &(is->audio_pkt);

        // main decode loop
again:
        for(;;)
        {
            if(is->quit)
            {
                break;
            }
            // seek stuff goes here
            if(is->seek_req)
            {
                if (is->seek_pts > 0.0)
                {
                    DoSeekRequest(is);
                }
                else
                {
                    is->seek_req = 0;
                    file_close();
                    file_open();

                    DUMP_CLOSE
                    DUMP_OPEN
                    DUMP_HEADER
                    close_data();
#ifdef PROCESS_CC
                    if (output_srt || output_smi) CEW_reinit();
#endif
                }
            }
nextpacket:
            ret=av_read_frame(is->pFormatCtx, packet);

            if (ret>=0 && is->seek_req)
            {
                double packet_time = (packet->pts - (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0)) * av_q2d(is->video_st->time_base);
                if (packet->pts==AV_NOPTS_VALUE || packet->pts == 0 )
                {
                    av_packet_unref(packet);
                    goto nextpacket;
                }
                if (is->seek_req < 6 && (is->seek_flags & AVSEEK_FLAG_BYTE) &&  is->duration > 0 && fabs(packet_time - (is->seek_pts - 2.5) ) < is->duration / (10 * is->seek_req))
                {
                    is->seek_pos += ((is->seek_pts - 2.5 - packet_time) / is->duration ) * avio_size(is->pFormatCtx->pb) * 0.9;
                    is->seek_req++;
                    goto again;
                }
                if (retries) Debug( 9,"Retry t_pos=%" PRId64 ", l_pos=%" PRId64 ", t_pts=%" PRId64 ", l_pts=%" PRId64 "\n", last_packet_pos, packet->pos, last_packet_pts, packet->pts);
                is->seek_req = 0;
            }
            /*
                    if (ret < 0 && is->seek_req && !is->seek_by_bytes) {
                        is->seek_by_bytes = 1;
                        Set_seek(is, is->seek_pts, is->duration);
                        goto again;
                    }
            */
            is->seek_req = 0;



#define REOPEN_TIME 500.0

            if ((selftest == 3 && retries==0 && is->video_clock >=REOPEN_TIME))
            {
                ret=AVERROR_EOF;  // Simulate EOF
                live_tv = 1;
            }
            if ((selftest == 4 && retries==0 && framenum > 0 && (framenum % 500) == 0))
            {
                ret=AVERROR_EOF;  // Simulate EOF
                live_tv = 1;
            }
            if(ret < 0 )
            {
                if (ret == AVERROR_EOF || is->pFormatCtx->pb->eof_reached)
                {
                    if (selftest == 3)   // Either simulated EOF or real EOF before REOPEN_TIME
                    {
                        if (retries > 0)
                        {
                            if (is->video_clock < selftest_target - 0.05 || is->video_clock > selftest_target + 0.05)
                            {
//                                sample_file = fopen("seektest.log", "a+");
//                                fprintf(sample_file, "\"%s\": reopen file failed, size=%8.1f, pts=%6.2f\n", is->filename, is->duration, is->video_clock );
//                                fclose(sample_file);
//                                exit(1);
                            }
                        }
                        else
                        {
                            if (is->video_clock < REOPEN_TIME)
                            {
                                selftest_target = is->video_clock - 2.0;
                            }
                            else
                            {
                                selftest_target = REOPEN_TIME;
                            }
                            selftest_target = fmax(selftest_target,0.5);
                            live_tv = 1;
                        }
                    }

                    if ((live_tv && retries < live_tv_retries) /* || (selftest == 3 && retries == 0) */)
                    {
//                    uint64_t retry_target;
                        if (retries == 0)
                        {
                            if (selftest == 3)
                                retry_target = selftest_target;
//                        retry_target = avio_tell(is->pFormatCtx->pb);
                            else
                                retry_target = is->video_clock;
                        }
                        file_close();
                        Debug( 1,"\nRetry=%d at frame=%d, time=%8.2f seconds\n", retries, framenum, retry_target);
                        Debug( 9,"Retry target pos=%" PRId64 ", pts=%" PRId64 "\n", last_packet_pos, last_packet_pts);

                        if (selftest == 0) Sleep(4000L);
                        file_open();
                        Set_seek(is, retry_target);

                        retries++;
                        goto again;
                    }

                    break;
                }


            }

            if (packet->pts != AV_NOPTS_VALUE && packet->pts != 0 )
            {
                last_packet_pts = packet->pts;
            }
            if (packet->pos != 0 && packet->pos != -1)
            {
                last_packet_pos = packet->pos;
            }

            if(packet->stream_index == is->videoStream)
            {
                video_packet_process(is, packet);
            }
            else if(packet->stream_index == is->audioStream)
            {
                audio_packet_process(is, packet);
            }
            else
            {
                /*
                			  ccDataLen = (int)packet->size;
                			  for (i=0; i<ccDataLen; i++) {
                				  ccData[i] = packet->data[i];
                			  }
                			  dump_data((char *)ccData, (int)ccDataLen);
                					if (output_srt)
                						process_block(ccData, (int)ccDataLen);
                					if (processCC) ProcessCCData();
                */
            }
            av_packet_unref(packet);
            if (is->video_clock == old_clock)
            {
                empty_packet_count++;
                if (empty_packet_count > 1000)
                    Debug(0, "Empty input\n");
            }
            else
            {
                old_clock = is->video_clock;
                empty_packet_count = 0;
            }
#ifdef SELFTEST
            if (selftest == 1 && pass == 0 && is->seek_req == 0 && framenum == 50)
            {
                if (is->duration > 2) {
                    selftest_target = fmin(450.0, is->duration - 2);
                } else {
                    selftest_target = 1.0;
                }
                Set_seek(is, selftest_target);
                pass = 1;
                framenum++;
            }
#endif
        }

        if (selftest == 1 && pass == 1 /*&& framenum > 501 && is->video_clock > 0 */)
        {
            if (is->video_clock < selftest_target - 0.08 || is->video_clock > selftest_target + 0.08)
            {
                sample_file = fopen("seektest.log", "a+");
                fprintf(sample_file, "Seek error: target=%8.1f, result=%8.1f, error=%6.3f, size=%8.1f, mode=%s\"%s\"\n",
                        is->seek_pts,
                        is->video_clock,
                        is->video_clock - is->seek_pts,
                        is->duration,
                        (is->seek_by_bytes ? "byteseek": "timeseek" ),
                        is->filename);
                fclose(sample_file);
            }
            /*
                            if (tries ==  0 && fabs((double) av_q2d(is->video_st->time_base)* ((double)(packet->pts - is->video_st->start_time - is->seek_pos ))) > 2.0) {
             				   is->seek_req=1;
            				   is->seek_pos = 20.0 / av_q2d(is->video_st->time_base);
            				   is->seek_flags = AVSEEK_FLAG_BYTE;
            				   tries++;
                           } else
             */
            selftest = 3;
            pass = 0;
            //exit(1);
        }



        if (live_tv)
        {
            lastFrameCommCalculated = 0;
            BuildCommListAsYouGo();
        }

        tfps = print_fps (1);

        Debug( 10,"\nParsed %d video frames and %d audio frames at %8.2f fps\n", framenum, sound_frame_counter, tfps);
        Debug( 10,"\nMaximum Volume found is %d\n", max_volume_found);


        in_file = 0;
        if (framenum>0)
        {
            if(BuildMasterCommList())
            {
                result = 1;
                printf("Commercials were found.\n");
            }
            else
            {
                result = 0;
                printf("Commercials were not found.\n");
            }
            if (output_debugwindow)
            {
                processCC = 0;
                printf("Close window when done\n");

                DUMP_CLOSE
                if (output_timing)
                {
                    output_timing = 0;
                }

#ifdef _WIN32
                while(1)
                {
                    ReviewResult();
                    vo_refresh();
                    Sleep(100L);
                }
#endif
                //		printf(" Press Enter to close debug window\n");
                //		gets(HomeDir);
            }
        }

#ifndef _DEBUG
    }
//	__exc ept(filter()) /* Stage 3 */
//	{
//      printf("Exception raised, terminating\n");/* Stage 5 of terminating exception */
//		exit(result);
//	}
#endif

//
// Clear EXECUTION_STATE flags to disable away mode and allow the system to idle to sleep normally.
//
#if (_WIN32_WINNT >= 0x0500 || _WIN32_WINDOWS >= 0x0410)
    SetThreadExecutionState(ES_CONTINUOUS);
#endif

#ifdef _WIN32
    exit (result);
#else
    exit (!result);
#endif
}
