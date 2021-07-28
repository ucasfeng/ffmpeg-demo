
#include <iostream>
#include <string>
//#include "decoder.hpp"
//#include "encoder.hpp"
//#include "streamPusher.hpp"
//#include "exception.hpp"
using namespace std;
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
#include <libavutil/avutil.h>
}

char wp_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
#define wp_err2str(errnum) av_make_error_string(wp_error, AV_ERROR_MAX_STRING_SIZE, errnum)

void printerr(int err) {
    cout << wp_err2str(err) << endl;
}

int main()
{
    string playUrls[] = {
            "ws://192.168.2.181:80/rtp/2505CB09.flv",
            "http://192.168.2.181:80/rtp/2505CB09.flv",
            "rtsp://192.168.2.181:554/rtp/2505CB09",
            "rtmp://192.168.2.181:1935/rtp/2505CB09",
            "http://192.168.2.181:80/rtp/2505CB09/hls.m3u8",
            "http://192.168.2.181:80/rtp/2505CB09.live.ts",
            "ws://192.168.2.181:80/rtp/2505CB09.live.ts",
            "http://192.168.2.181:80/rtp/2505CB09.live.mp4",
            "ws://192.168.2.181:80/rtp/2505CB09.live.mp4" };

    int ret = 0;
    string rmtpUrl = "rtmp://192.168.2.181:1936/live/test";
    string inurl = playUrls[3];
    int insdx = 0; // input video stream index
    AVFormatContext* inctx = avformat_alloc_context();
    AVCodec* codec = nullptr;
    AVCodecContext* dectx = nullptr;
    AVCodecContext* enctx = nullptr;



    // input stream.
    if ((ret = avformat_open_input(&inctx, inurl.data(), nullptr, nullptr)) < 0){
        printerr(ret);
        return 0;
    }

    // input stream info.
    if ((ret = avformat_find_stream_info(inctx, nullptr)) < 0) {
        printerr(ret);
        return 0;
    }

    // video stream and codec.
    if ((ret = av_find_best_stream(inctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0)) < 0) {
        printerr(ret);
        return 0;
    }
    insdx = ret;

    av_dump_format(inctx, insdx, inurl.data(), 0);

    // decoder context
    if ((dectx = avcodec_alloc_context3(codec)) == nullptr) {
        cout << "avcodec_alloc_context3 err" << endl;
        return 0;
    }
    
    // decoder context paras
    if ((ret = avcodec_parameters_to_context(dectx, inctx->streams[insdx]->codecpar) < 0)) {
        printerr(ret);
        return 0;
    }

    if ((ret = avcodec_open2(dectx, codec, nullptr)) < 0) {
		printerr(ret);
		return 0;
    }

    // encoder context
	if ((enctx = avcodec_alloc_context3(codec)) == nullptr) {
		cout << "avcodec_alloc_context3 enctx err" << endl;
		return 0;
    }else {
        enctx->bit_rate = 400000;
        enctx->width = dectx->width;
        enctx->height = dectx->height;
        enctx->time_base = AVRational{ 1,25 };
        enctx->framerate = AVRational{ 25,1 };
        enctx->gop_size = 10;
        enctx->max_b_frames = 1;
        enctx->pix_fmt = AV_PIX_FMT_YUV420P;
    }

	if ((ret = avcodec_open2(enctx, codec, nullptr)) < 0) {
		printerr(ret);
		return 0;
	}

    AVFrame* frame = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();
    int64_t pts = 0;
    for (;;){
        // read pkt.
        ret = av_read_frame(inctx, pkt);
        if (ret < 0){
            printerr(ret);
            break;
        }
        // filter by video index
        if (pkt->stream_index != insdx){
            av_packet_unref(pkt);
            continue;
        }

        // send pkt to decoder.
        if (pkt->size > 0){
            if ((ret = avcodec_send_packet(dectx, pkt)) < 0) {
                printerr(ret);
                if (ret != AVERROR(EAGAIN)){
                    break;
                }
            } else{
                av_packet_unref(pkt);
            }
        }

        // recv frame from decoder.
        if ((ret=avcodec_receive_frame(dectx, frame))<0){
            printerr(ret);
			if (ret != AVERROR(EAGAIN)) {
				break;
			}
        }else{
            // success.
            printf("[decode-recv] format=%d pts=%lld width=%d height=%d\n",frame->format,  frame->pts, frame->width, frame->height);
            av_frame_unref(frame);
        }
        

        // encode
        frame->format = enctx->pix_fmt;
        frame->width = enctx->width;
        frame->height = enctx->height;
        frame->pts = pts++;
        if ((ret = avcodec_send_frame(enctx, frame)) < 0) {
            printerr(ret);
			if (ret != AVERROR(EAGAIN)) {
				break;
			}
        }else {
            av_frame_unref(frame);
        }

		if ((ret = avcodec_receive_packet(enctx,pkt)) < 0) {
			printerr(ret);
			if (ret != AVERROR(EAGAIN)) {
				break;
			}
		}else {
            printf("[encode-recv] pts=%lld size=%d\n", pkt->pts, pkt->size);
            av_packet_unref(pkt);
		}
    }

    // free.
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&dectx);
    avformat_free_context(inctx);
    return 0;
}
