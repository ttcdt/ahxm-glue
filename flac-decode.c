/*

    flac-decode

    .flac to .wav converter

    ttcdt <dev@triptico.com>

    This software is released under the public domain.

*/

#include <stdio.h>

#ifdef DISABLE_FLAC

int is_flac_supported(void)
{
    return 0;
}

FILE *flac_convert(char *fn)
{
    return NULL;
}

#else /* DISABLE_FLAC */

#include <FLAC/stream_decoder.h>

/* copied from ahxm */

static FLAC__uint64 flac_total_samples  = 0;
static int          flac_sample_rate    = 0;
static int          flac_channels       = 0;
static int          flac_bps            = 0;

/* FIXME: ad-hoc WAV file writer -- should use mine */

void write_little_endian_uint16(FILE *f, FLAC__uint16 x)
{
    fputc(x, f);
    fputc(x >> 8, f);
}


void write_little_endian_int16(FILE *f, FLAC__int16 x)
{
    write_little_endian_uint16(f, (FLAC__uint16)x);
}


void write_little_endian_uint32(FILE *f, FLAC__uint32 x)
{
    fputc(x, f);
    fputc(x >> 8, f);
    fputc(x >> 16, f);
    fputc(x >> 24, f);
}


FLAC__StreamDecoderWriteStatus flac_wcb(const FLAC__StreamDecoder *decoder,
                                        const FLAC__Frame *frame,
                                        const FLAC__int32 * const buffer[],
                                        void *client_data)
{
    FILE *f = (FILE*)client_data;
    const FLAC__uint32 total_size =
        (FLAC__uint32)(flac_total_samples * flac_channels * (flac_bps/8));
    size_t i, n;

    if (flac_total_samples == 0 || flac_bps != 16)
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

    if (frame->header.number.sample_number == 0) {
        fwrite("RIFF", 1, 4, f);
        write_little_endian_uint32(f, total_size + 36);
        fwrite("WAVEfmt ", 1, 8, f);
        write_little_endian_uint32(f, 16);
        write_little_endian_uint16(f, 1);
        write_little_endian_uint16(f, (FLAC__uint16)flac_channels);
        write_little_endian_uint32(f, flac_sample_rate);
        write_little_endian_uint32(f, flac_sample_rate * flac_channels * (flac_bps/8));
        write_little_endian_uint16(f, (FLAC__uint16)(flac_channels * (flac_bps/8)));
        write_little_endian_uint16(f, (FLAC__uint16)flac_bps);
        fwrite("data", 1, 4, f);
        write_little_endian_uint32(f, total_size);
    }

    for (i = 0; i < frame->header.blocksize; i++) {
        for (n = 0; n < flac_channels; n++) {
            write_little_endian_int16(f, (FLAC__int16)buffer[n][i]);
        }
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


void flac_mcb(const FLAC__StreamDecoder *decoder,
              const FLAC__StreamMetadata *metadata,
              void *client_data)
{
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        flac_total_samples  = metadata->data.stream_info.total_samples;
        flac_sample_rate    = metadata->data.stream_info.sample_rate;
        flac_channels       = metadata->data.stream_info.channels;
        flac_bps            = metadata->data.stream_info.bits_per_sample;
    }
}


void flac_ecb(const FLAC__StreamDecoder *decoder,
              FLAC__StreamDecoderErrorStatus status,
              void *client_data)
{
}


int is_flac_supported(void)
{
    return 1;
}

FILE *flac_convert(const char *ifile)
{
    int ret = 0;
    FILE *o;
    FLAC__StreamDecoder *decoder;

    if ((decoder = FLAC__stream_decoder_new()) != NULL) {
        if ((o = tmpfile()) != NULL) {
            if (FLAC__stream_decoder_init_file(
                    decoder,
                    ifile,
                    flac_wcb,
                    flac_mcb,
                    flac_ecb,
                    o) == FLAC__STREAM_DECODER_INIT_STATUS_OK) {
                ret = FLAC__stream_decoder_process_until_end_of_stream(decoder);
            }

            FLAC__stream_decoder_delete(decoder);

            ret = 1;

            /* rewind the tmpfile */
            rewind(o);
        }
    }

    if (ret == 0) {
        fclose(o);
        o = NULL;
    }

    return o;
}


#endif /* DISABLE_FLAC */
