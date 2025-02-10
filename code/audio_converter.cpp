#include "audio_converter.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

// Helper function: set the thread count for a codec context.
void set_thread_count(AVCodecContext* codec_ctx, int thread_count) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", thread_count);
    // av_opt_set sets the 'threads' option on the codec context.
    av_opt_set(codec_ctx, "threads", buf, AV_OPT_SEARCH_CHILDREN);
}

int convert_audio(const char* input_filename, const char* output_filename, int thread_count) {
    int ret = 0;

    // Declare all variables at the top to ensure no "goto cleanup" crosses initializations.
    AVFormatContext *in_format_ctx = nullptr;
    AVFormatContext *out_format_ctx = nullptr;
    AVCodecContext *in_codec_ctx = nullptr;
    AVCodecContext *out_codec_ctx = nullptr;
    const AVCodec *in_codec = nullptr;
    const AVCodec *out_codec = nullptr;
    AVStream *in_stream = nullptr;
    AVStream *out_stream = nullptr;
    SwrContext *swr_ctx = nullptr;
    int stream_index = -1;

    if (!input_filename || !output_filename) {
        std::cerr << "Invalid input or output filename." << std::endl;
        return -1;
    }

    // Open input file.
    if ((ret = avformat_open_input(&in_format_ctx, input_filename, nullptr, nullptr)) < 0) {
        std::cerr << "Failed to open input file." << std::endl;
        goto cleanup;
    }
    if ((ret = avformat_find_stream_info(in_format_ctx, nullptr)) < 0) {
        std::cerr << "Failed to retrieve input stream information." << std::endl;
        goto cleanup;
    }

    // Find the first audio stream.
    for (unsigned i = 0; i < in_format_ctx->nb_streams; i++) {
        if (in_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            stream_index = i;
            break;
        }
    }
    if (stream_index < 0) {
        std::cerr << "No audio stream found." << std::endl;
        ret = AVERROR(EINVAL);
        goto cleanup;
    }
    in_stream = in_format_ctx->streams[stream_index];

    // Find and open the decoder.
    in_codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
    if (!in_codec) {
        std::cerr << "Decoder not found." << std::endl;
        ret = AVERROR_DECODER_NOT_FOUND;
        goto cleanup;
    }
    in_codec_ctx = avcodec_alloc_context3(in_codec);
    if (!in_codec_ctx) {
        std::cerr << "Could not allocate decoder context." << std::endl;
        ret = AVERROR(ENOMEM);
        goto cleanup;
    }
    if ((ret = avcodec_parameters_to_context(in_codec_ctx, in_stream->codecpar)) < 0) {
        std::cerr << "Failed to copy codec parameters to decoder context." << std::endl;
        goto cleanup;
    }
    if ((ret = avcodec_open2(in_codec_ctx, in_codec, nullptr)) < 0) {
        std::cerr << "Failed to open decoder." << std::endl;
        goto cleanup;
    }

    // Allocate output format context.
    if ((ret = avformat_alloc_output_context2(&out_format_ctx, nullptr, nullptr, output_filename)) < 0) {
        std::cerr << "Failed to allocate output format context." << std::endl;
        goto cleanup;
    }

    // Create output stream.
    out_stream = avformat_new_stream(out_format_ctx, nullptr);
    if (!out_stream) {
        std::cerr << "Failed to create output stream." << std::endl;
        ret = AVERROR(ENOMEM);
        goto cleanup;
    }

    // Find and open the encoder.
    out_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!out_codec) {
        std::cerr << "Encoder not found." << std::endl;
        ret = AVERROR_ENCODER_NOT_FOUND;
        goto cleanup;
    }
    out_codec_ctx = avcodec_alloc_context3(out_codec);
    if (!out_codec_ctx) {
        std::cerr << "Could not allocate encoder context." << std::endl;
        ret = AVERROR(ENOMEM);
        goto cleanup;
    }

    // Suppress warnings for deprecated field sample_fmts.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    if (out_codec->sample_fmts) {
        out_codec_ctx->sample_fmt = out_codec->sample_fmts[0];
    } else {
        out_codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }
#pragma GCC diagnostic pop

    out_codec_ctx->bit_rate = 64000;
    out_codec_ctx->sample_rate = in_codec_ctx->sample_rate;
    // Use the new channel layout field 'ch_layout'
    out_codec_ctx->ch_layout = in_codec_ctx->ch_layout;
    // Set the thread count.
    set_thread_count(out_codec_ctx, thread_count);

    if ((ret = avcodec_open2(out_codec_ctx, out_codec, nullptr)) < 0) {
        std::cerr << "Failed to open encoder." << std::endl;
        goto cleanup;
    }
    if ((ret = avcodec_parameters_from_context(out_stream->codecpar, out_codec_ctx)) < 0) {
        std::cerr << "Failed to copy encoder parameters to output stream." << std::endl;
        goto cleanup;
    }

    // Open the output file.
    if (!(out_format_ctx->oformat->flags & AVFMT_NOFILE)) {
        if ((ret = avio_open(&out_format_ctx->pb, output_filename, AVIO_FLAG_WRITE)) < 0) {
            std::cerr << "Failed to open output file." << std::endl;
            goto cleanup;
        }
    }

    // Write header to the output file.
    if ((ret = avformat_write_header(out_format_ctx, nullptr)) < 0) {
        std::cerr << "Failed to write header to output file." << std::endl;
        goto cleanup;
    }

    // (Insert the decoding/encoding loop here as needed.)

    // Write trailer to finalize the output file.
    if ((ret = av_write_trailer(out_format_ctx)) < 0) {
        std::cerr << "Failed to write trailer to output file." << std::endl;
        goto cleanup;
    }

cleanup:
    if (in_codec_ctx)
        avcodec_free_context(&in_codec_ctx);
    if (out_codec_ctx)
        avcodec_free_context(&out_codec_ctx);
    if (in_format_ctx)
        avformat_close_input(&in_format_ctx);
    if (out_format_ctx) {
        if (!(out_format_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&out_format_ctx->pb);
        avformat_free_context(out_format_ctx);
    }
    return ret < 0 ? 1 : 0;
}

#ifdef __cplusplus
}
#endif
