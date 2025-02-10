#ifndef AUDIO_CONVERTER_H
#define AUDIO_CONVERTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

// Sets the thread count for a given codec context.
void set_thread_count(AVCodecContext* codec_ctx, int thread_count);

// Converts the audio in the given input file to AAC and writes the output file.
// Returns 0 on success, non-zero on failure.
int convert_audio(const char* input_filename, const char* output_filename, int thread_count);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_CONVERTER_H
