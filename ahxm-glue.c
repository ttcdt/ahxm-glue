/*

    ahxm-glue

    Glues a set of wave files together with some tweaks.

    ttcdt <dev@triptico.com>

    This software is released under the public domain.

*/

#define VERSION "1.01"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct wave {
    FILE *f;        /* the file containing the samples */
    off_t start;    /* start of sample in full mix */
    off_t end;      /* end of sample in full mix */
    off_t fade_out; /* from this position to end, fade out */

    struct wave *next;
};

int sample_rate = 0;

struct wave *waves = NULL;

char output_file[1024] = "output.wav";


off_t time_to_offset(double time)
/* converts a time in min.sec to samples */
{
    double mins, secs;

    mins = trunc(time);
    secs = (time - mins) * 100.0;

    return (off_t) (mins * 60.0 + secs) * sample_rate;
}


double offset_to_time(off_t offset)
/* converts a sample position or size to min.sec */
{
    double mins, secs;

    offset /= sample_rate;

    mins = offset / 60;
    secs = offset % 60;

    return (double) (mins + secs / 100.0);
}


static short fget16(FILE *f)
/* Reads a 16 bit integer from a file in big endian byte ordering */
{
    short c;

    c = fgetc(f);
    c = (fgetc(f) << 8) | c;

    return c;
}


static int fget32(FILE *f)
/* Reads a 32 bit integer from a file in big endian byte ordering */
{
    int c;

    c = fgetc(f);
    c += (fgetc(f) * 256);
    c += (fgetc(f) * 65536);
    c += (fgetc(f) * 16777216);

    return c;
}


int open_wav_file(char *fn, size_t *size, FILE **fp)
/* a very simple .wav file loader */
{
    FILE *f = NULL;
    char buf[1024];
    int sr, ret = 0;

    if ((*fp = fopen(fn, "rb")) == NULL) {
        ret = 10;
        printf("ERROR: cannot open '%s'\n", fn);
        goto done;
    }

    f = *fp;

    /* read header */
    fread(buf, 1, 4, f);
    buf[4] = 0;
    fget32(f);
    fread(buf, 1, 4, f);
    buf[4] = 0;

    if (strcmp(buf, "WAVE") != 0) {
        ret = 11;
        printf("ERROR: unrecognized wav file format\n");
        goto done;
    }

    fread(buf, 1, 4, f);
    buf[4] = 0;
    fget32(f);

    if (fget16(f) != 1) {
        ret = 12;
        printf("ERROR: unsupported compressed format\n");
        goto done;
    }

    if (fget16(f) != 2) {
        ret = 13;
        printf("ERROR: channels must be 2 (stereo)\n");
        goto done;
    }

    sr = fget32(f);

    if (sample_rate == 0)
        sample_rate = sr;
    else
    if (sr != sample_rate) {
        ret = 14;
        printf("ERROR: all samples must use the same sample rate as the 1st one (%d Hz)\n", sample_rate);
        goto done;
    }

    fget32(f);

    if (fget16(f) * 4 != 16) {
        ret = 15;
        printf("ERROR: sample size must be 16 bits\n");
        goto done;
    }

    fget16(f);
    fread(buf, 1, 4, f);
    buf[4] = 0;

    if (strcmp(buf, "data") != 0) {
        ret = 16;
        printf("ERROR: unrecognized data chunk\n");
        goto done;
    }

    *size = fget32(f) / 4;

done:
    if (ret && f != NULL) {
        fclose(f);
        f = NULL;
    }

    return ret;
}


int add_wave(char *fn, off_t *pos, double offset_time,
             double skip_time, double end_time, double fade_out_time)
{
    int ret = 0;
    FILE *f;
    size_t size;

    if (open_wav_file(fn, &size, &f) == 0) {
        struct wave *w;
        off_t offset;
        off_t skip;
        off_t end;

        w = calloc(sizeof(struct wave), 1);

        /* enqueue */
        w->next = waves;
        waves = w;

        /* store the file */
        w->f = f;

        /* number of samples this wave starts from the previous one */
        /* positive (insert silence) or negative (overlaps) */
        offset = time_to_offset(offset_time);

        *pos += offset;
        w->start = *pos;

        /* number of samples to skip from the start of the wave */
        skip = time_to_offset(skip_time);
        fseek(f, skip * 4, SEEK_CUR);
        size -= skip;

        /* number of samples for the end of the wave */
        /* positive (size) or negative (substract from wave size) */
        end = time_to_offset(end_time);

        if (end > 0)
            *pos += end;
        else
            *pos += size + end;

        w->end = *pos;

        /* fade out position */
        w->fade_out = w->end - time_to_offset(fade_out_time);

        printf("INFO : '%s' %.2f\n", fn, offset_to_time(w->start));
    }

    return ret;
}


static void fput16(short int i, FILE *f)
/* writes a 16 bit integer, in any machine order */
{
    fputc(i & 0x00ff, f);
    fputc((i & 0xff00) >> 8, f);
}


static void fput32(int i, FILE *f)
/* writes a 32 bit integer, in any machine order */
{
    fputc(i & 0x000000ff, f);
    fputc((i & 0x0000ff00) >> 8, f);
    fputc((i & 0x00ff0000) >> 16, f);
    fputc((i & 0xff000000) >> 24, f);
}


int mix(char *fn, off_t size)
{
    int ret = 0;
    FILE *f;
    off_t n;

    if ((f = fopen(fn, "wb")) == NULL) {
        printf("ERROR: cannot create '%s'\n", fn);
        ret = 20;
        goto done;
    }

    /* write the .wav header */
    fwrite("RIFF", 1, 4, f);
    fput32((size * 2 * 2) + 36, f); /* 36: "WAVE" + "fmt " + 4 + 16 + "data" + 4 */
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    fput32(16, f);                  /* this struct size */
    fput16(1, f);                   /* 1: uncompressed PCM */
    fput16(2, f);                   /* # of channels */
    fput32(sample_rate, f);         /* sample rate */
    fput32(sample_rate * 2 * 2, f); /* bytes per second */
    fput16(2 * 2, f);               /* 'block align' */
    fput16(16, f);                  /* 16 bits per sample */
    fwrite("data", 1, 4, f);
    fput32(size * 2 * 2, f);        /* data size in bytes */

    for (n = 0; n < size; n++) {
        struct wave *w;
        int l, r;

        l = r = 0;

        for (w = waves; w != NULL; w = w->next) {
            if (n >= w->start && n < w->end) {
                int pl, pr;

                pl = fget16(w->f);
                pr = fget16(w->f);

                if (n >= w->fade_out) {
                    int p = ((w->end - n) * 65536) / (w->end - w->fade_out);

                    pl = (pl * p) / 65536;
                    pr = (pr * p) / 65536;
                }

                l += pl;
                r += pr;
            }
        }

        fput16(l, f);
        fput16(r, f);
    }

    fclose(f);

done:
    return ret;
}


int parse_argv(int argc, char *argv[], off_t *cursor)
{
    int ret = 0;
    int n;
    char *wfn = NULL;
    double offset, skip, end, fade_out;

    for (n = 1; n <= argc; n++) {
        if (n == argc || strchr(argv[n], '=') == NULL) {
            /* is there a previous one? */
            if (wfn != NULL) {
                if (add_wave(wfn, cursor, offset, skip, end, fade_out)) {
                    ret = 31;
                    goto done;
                }
            }

            if (n != argc) {
                /* new file */
                wfn = argv[n];
                offset = skip = end = fade_out = 0.0;
            }
        }
        else {
            char *token = strdup(argv[n]);
            char *value = strchr(token, '=');

            *value = '\0';
            value++;

            if (strcmp(token, "output") == 0)
                strcpy(output_file, value);
            else
            if (strcmp(token, "offset") == 0)
                sscanf(value, "%lf", &offset);
            else
            if (strcmp(token, "skip") == 0)
                sscanf(value, "%lf", &skip);
            else
            if (strcmp(token, "end") == 0)
                sscanf(value, "%lf", &end);
            else
            if (strcmp(token, "fade_out") == 0)
                sscanf(value, "%lf", &fade_out);
            else {
                printf("ERROR: unrecognized token '%s'\n", token);
                ret = 32;
                goto done;
            }

            free(token);
        }
    }

done:
    return ret;
}


int usage(void)
{
    printf("ahxm-glue %s - Glues a set of .wav files together\n", VERSION);
    printf("ttcdt <dev@triptico.com>\n");
    printf("This software is released into the public domain.\n\n");

    printf("Usage: ahxm-glue {wave file} [options] [{wave file} [options]...]\n\n");

    printf("Options:\n\n");
    printf("output={wave file}  Output wave file (default: %s).\n", output_file);
    printf("offset={time}       Offset from previous file (default: 0).\n");
    printf("                    positive: insert silence.\n");
    printf("                    negative: overlap into previous file.\n");
    printf("                    0: start immediately.\n");
    printf("skip={time}         Time to skip from file (default: 0).\n");
    printf("end={time}          End time (default: 0).\n");
    printf("                    positive: absolute length.\n");
    printf("                    negative: substract time from wave length.\n");
    printf("                    0: full length.\n");
    printf("fade_out={time}     Fade out time (default: 0).\n");
    printf("\n");
    printf("Time is specified as minutes.seconds: 1.00, 2.23, 0.1535 (15.35 secs.)\n\n");
    printf("All options except output= affect only to the previous file.\n");
    printf("\n");
    printf("Examples:\n\n");
    printf("$ ahxm-glue 1.wav 2.wav 3.wav\n");
    printf("  Generates output.wav with the concatenation of the three files\n");
    printf("$ ahxm-glue 1.wav 2.wav offset=-0.02 3.wav\n");
    printf("  2.wav mixes 2 seconds into the end of 1.wav\n");
    printf("$ ahxm-glue 1.wav skip=1.30 2.wav offset=0.02 3.wav\n");
    printf("  Skip 1:30 mins from 1st and insert 2 silence seconds before 2nd\n");
    printf("$ ahxm-glue 1.wav 2.wav end=0.30 3.wav\n");
    printf("  2.wav only lasts 30 seconds\n");
    printf("$ ahxm-glue 1.wav fade_out=0.02 2.wav offset=-0.02 3.wav\n");
    printf("  1.wav starts fading out when 2.wav starts\n");
    printf("$ ahxm-glue 1.wav 2.wav 3.wav end=-0.1025 output=out.wav\n");
    printf("  3.wav lasts 10.25 seconds less and result stored in out.wav\n");

    return 1;
}


int main(int argc, char *argv[])
{
    int ret = 0;
    off_t cursor = 0;

    if (argc == 1)
        ret = usage();
    else
    if ((ret = parse_argv(argc, argv, &cursor)) == 0)
        ret = mix(output_file, cursor);

    return ret;
}
