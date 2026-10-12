#pragma once

#include <functional>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

class VideoFrameReader {
public:
    enum class Status {
        Ok,
        Eof,
        Aborted,
        Error,
    };

    VideoFrameReader(AVFormatContext *fmt, AVCodecContext *dec, int streamIndex)
            : m_fmt(fmt), m_dec(dec), m_streamIndex(streamIndex) {
        m_pkt = av_packet_alloc();
        m_frame = av_frame_alloc();
    }

    ~VideoFrameReader() {
        av_frame_free(&m_frame);
        av_packet_free(&m_pkt);

    }

    VideoFrameReader(const VideoFrameReader &) = delete;
    VideoFrameReader &operator=(const VideoFrameReader &) = delete;

    std::function<bool()> shouldAbort;

    Status getNextFrame() {
        if (m_pkt == nullptr || m_frame == nullptr) {
            return Status::Error;
        }

        av_frame_unref(m_frame);

        for (;;) {
            if (shouldAbort && shouldAbort()) {
                return Status::Aborted;
            }

            int ret = avcodec_receive_frame(m_dec, m_frame);
            if (ret == 0) {
                return Status::Ok;
            }
            if (ret == AVERROR_EOF) {
                return Status::Eof;
            }
            if (ret != AVERROR(EAGAIN)) {
                return Status::Error;
            }

            if (m_draining) {
                return Status::Eof;
            }
            if (!feedNextPacket()) {
                return Status::Error;
            }
        }
    }

    bool seek(int64_t pts,
              int flags = AVSEEK_FLAG_BACKWARD) {
        int ret = av_seek_frame(m_fmt, m_streamIndex, pts, flags);
        if (ret < 0) {
            return false;
        }
        avcodec_flush_buffers(m_dec);
        av_frame_unref(m_frame);
        m_draining = false;
        return true;
    }

    AVFrame *frame() const { return m_frame; }

    double frameTimeSeconds() const {
        AVRational tb = m_fmt->streams[m_streamIndex]->time_base;
        return m_frame->best_effort_timestamp * av_q2d(tb);
    }

    int streamIndex() const { return m_streamIndex; }

private:

    bool feedNextPacket() {
        for (;;) {
            int ret = av_read_frame(m_fmt, m_pkt);
            if (ret < 0) {
                avcodec_send_packet(m_dec, nullptr);
                m_draining = true;
                return true;
            }

            if (m_pkt->stream_index != m_streamIndex) {
                av_packet_unref(m_pkt);
                continue;
            }

            ret = avcodec_send_packet(m_dec, m_pkt);
            av_packet_unref(m_pkt);
            if (ret < 0 && ret != AVERROR(EAGAIN)) {
                return false;
            }
            return true;
        }
    }

    AVFormatContext *m_fmt;
    AVCodecContext *m_dec;
    int m_streamIndex;

    AVPacket *m_pkt = nullptr;
    AVFrame *m_frame = nullptr;
    bool m_draining = false;
};