#include <errno.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "acm2snd.h"
#include "readers.h"
#include "general.h"
#include "riffhdr.h"
#include "portable-utils.h"

static CACMReader* acm = NULL;

void finalize() {
    if (acm) delete acm;
    acm = NULL;
}

int ConvertAcmWav(int fhandle, int maxlen, unsigned char*& memory, int32_t& samples_written, int forcestereo)
{
    int riff_chans;
    int32_t rawsize = 0;
    int32_t cnt, cnt1;
    RIFF_HEADER riff;

    memory = 0;
    if (maxlen == -1) maxlen = filelength(fhandle);
    try
    {
        if (read(fhandle, &riff, 4) != 4) return 3;

        if (!memcmp(riff.riff_sig, "RIFF", 4))
        {
            if (read(fhandle, &riff.total_len_m8, sizeof(RIFF_HEADER) - 4) == sizeof(RIFF_HEADER) - 4)
            {
                if (riff.formatex_len != (uint32_t)((BYTE*)riff.data_sig - (BYTE*)&riff.wFormatTag))
                {
                    cnt = riff.formatex_len - 24;
                    lseek(fhandle, cnt, SEEK_CUR);
                    read(fhandle, riff.data_sig, 8);
                    riff.formatex_len = 16;
                }
                if (!memcmp(riff.data_sig, "fact", 4))
                {
                    cnt = riff.raw_data_len;
                    lseek(fhandle, cnt, SEEK_CUR);
                    read(fhandle, riff.data_sig, 8);
                    if (memcmp(riff.data_sig, "data", 4))
                    {
                        finalize();
                        return 3;
                    }
                }
                memory = new unsigned char[riff.raw_data_len + sizeof(RIFF_HEADER)];
                if (!memory)
                {
                    finalize();
                    return 3;
                }
                maxlen -= sizeof(RIFF_HEADER);
                if (riff.raw_data_len > (uint32_t)maxlen)
                {
                    riff.total_len_m8 = maxlen + sizeof(RIFF_HEADER);
                    riff.raw_data_len = maxlen;
                }
                memcpy(memory, &riff, sizeof(RIFF_HEADER));
                samples_written = riff.raw_data_len + sizeof(RIFF_HEADER);
                if (read(fhandle, (unsigned char*)memory + sizeof(RIFF_HEADER), riff.raw_data_len) != (int32_t)riff.raw_data_len)
                {
                    finalize();
                    return 3;
                }
                finalize();
                return 0;
            }
        }
        lseek(fhandle, -4, SEEK_CUR);
        acm = (CACMReader*)CreateSoundReader(fhandle, SND_READER_ACM, maxlen);
        if (!acm)
        {
            finalize();
            return 1;
        }

        cnt = acm->get_length();
        riff_chans = acm->get_channels();
        if (forcestereo && (riff_chans == 1)) riff_chans = 2;
        if (riff_chans != 1 && riff_chans != 2)
        {
            finalize();
            return 4;
        }

        memory = new unsigned char[cnt * 2 + sizeof(RIFF_HEADER)];
        if (!memory)
        {
            finalize();
            return 3;
        }
        samples_written = sizeof(RIFF_HEADER);
        write_riff_header(memory, cnt, riff_chans, acm->get_samplerate());

        cnt1 = acm->read_samples((int16_t*)(memory + samples_written), cnt);
        rawsize = cnt1 * sizeof(int16_t);
        samples_written += rawsize;
        cnt -= cnt1;

        finalize();
        if (cnt) return 4;
        return 0;
    }
    catch (...)
    {
        finalize();
        return 4;
    }
}