// InterPlay ACM codec tool.
//
//   acmtool -e <input.wav>  <output.acm>   encode a PCM WAV into InterPlay ACM
//   acmtool -d <input.acm>  <output.wav>   decode an InterPlay ACM into a PCM WAV
//
// Mono input is encoded as stereo (the channel is duplicated): the whole
// ACM ecosystem (game engines, libacm/vgmstream) treats plain ACM files as
// stereo, and a genuine mono stream would play at half length.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "snd2acm.h"
#include "acm2snd.h"

// The Convert* helpers take a file descriptor and internally fdopen() it;
// the sound reader owns and closes that descriptor. Hand them a duplicate
// so the original FILE* stays owned (and closable) here - closing the same
// descriptor twice makes the MSVC CRT fast-fail (0xC0000409).
static int dup_fd(int fd)
{
#ifdef _WIN32
    return _dup(fd);
#else
    return dup(fd);
#endif
}

static void usage(const char* exe)
{
    printf("Usage:\n"
           "  %s -e <input.wav>  <output.acm>   encode WAV -> InterPlay ACM\n"
           "  %s -d <input.acm>  <output.wav>   decode InterPlay ACM -> WAV\n",
           exe, exe);
}

static int do_encode(const char* in, const char* out)
{
    FILE* finp = fopen(in, "rb");
    if (!finp) {
        printf("Error: cannot open input file '%s'.\n", in);
        return 1;
    }
    FILE* foutp = fopen(out, "wb");
    if (!foutp) {
        printf("Error: cannot create output file '%s'.\n", out);
        fclose(finp);
        return 1;
    }

    int rc = ConvertWavAcm(dup_fd(fileno(finp)), -1, foutp, false);

    fclose(finp);
    fclose(foutp);

    switch (rc) {
    case 0:
        return 0;
    case 1:
        printf("Error: not a valid PCM WAV file, or unsupported format.\n");
        break;
    default:
        printf("Error: encoding failed (code %d).\n", rc);
        break;
    }
    remove(out);
    return rc;
}

static int do_decode(const char* in, const char* out)
{
    FILE* finp = fopen(in, "rb");
    if (!finp) {
        printf("Error: cannot open input file '%s'.\n", in);
        return 1;
    }
    FILE* foutp = fopen(out, "wb");
    if (!foutp) {
        printf("Error: cannot create output file '%s'.\n", out);
        fclose(finp);
        return 1;
    }

    unsigned char* memory = NULL;
    int32_t samples_written = 0;
    int rc = ConvertAcmWav(dup_fd(fileno(finp)), -1, memory, samples_written, 0);

    fclose(finp);

    if (rc == 0 && memory && samples_written > 0) {
        // samples_written already includes the RIFF header - write exactly that.
        if (fwrite(memory, 1, samples_written, foutp) != (size_t) samples_written) {
            printf("Error: short write to '%s'.\n", out);
            rc = 4;
        }
    } else {
        if (rc == 0) rc = 5; // nothing decoded - make sure the stub gets removed
        printf("Error: decoding failed (code %d).\n", rc);
    }

    if (memory) {
        delete[] memory;
        memory = NULL;
    }
    fclose(foutp);
    if (rc != 0)
        remove(out);
    return rc;
}

int main(int argc, char** argv)
{
    if (argc != 4 || argv[1][0] != '-' || argv[1][2] != '\0') {
        usage(argv[0]);
        return 1;
    }

    switch (tolower((unsigned char) argv[1][1])) {
    case 'e':
        return do_encode(argv[2], argv[3]);
    case 'd':
        return do_decode(argv[2], argv[3]);
    default:
        usage(argv[0]);
        return 1;
    }
}
