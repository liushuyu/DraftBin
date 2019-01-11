#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "audio_samples.h"

#define SAMPLERATE 48000

const uint32_t freq_table[16] = {96000, 88200, 64000, 48000, 44100, 32000,
                                 24000, 22050, 16000, 12000, 11025, 8000,
                                 7350,  0,     0,     0};
const short channel_table[8] = {0, 1, 2, 3, 4, 5, 6, 8};

struct ADTSData {
  bool MPEG2;
  uint8_t profile;
  uint8_t channels;
  uint8_t channel_idx;
  uint8_t framecount;
  uint8_t samplerate_idx;
  uint32_t length;
  uint32_t samplerate;
};

struct BinaryRequest {
  uint16_t codec;
  uint16_t cmd;
  uint32_t fixed;
  uint32_t src_addr;
  uint32_t size;
  uint32_t dst_addr_ch0;
  uint32_t dst_addr_ch1;
  uint32_t unknown1;
  uint32_t unknown2;
};

uint32_t parse_adts(unsigned char *buffer, struct ADTSData *out) {
  uint32_t tmp = 0;

  // sync word 0xfff
  tmp = (buffer[0] << 8) | (buffer[1] & 0xf0);
  if ((tmp & 0xffff) != 0xfff0)
    return 0;
  out->MPEG2 = (buffer[1] >> 3) & 0x1;
  // bit 17 to 18
  out->profile = (buffer[2] >> 6) + 1;
  // bit 19 to 22
  tmp = (buffer[2] >> 2) & 0xf;
  out->samplerate_idx = tmp;
  out->samplerate = (tmp > 15) ? 0 : freq_table[tmp];
  // bit 24 to 26
  tmp = ((buffer[2] & 0x1) << 2) | ((buffer[3] >> 6) & 0x3);
  out->channel_idx = tmp;
  out->channels = (tmp > 7) ? 0 : channel_table[tmp];

  // bit 55 to 56
  out->framecount = (buffer[6] & 0x3) + 1;

  // bit 31 to 43
  tmp = (buffer[3] & 0x3) << 11;
  tmp |= buffer[4] << 3;
  tmp |= (buffer[5] >> 5) & 0x7;

  out->length = tmp;

  return tmp;
}

void send_dsp_loop(uint32_t *ch0, uint32_t *ch1, ndspWaveBuf waveBuf[2]) {
  uint8_t channel = 0;

  while (aptMainLoop()) {
    gfxSwapBuffers();
    gfxFlushBuffers();
    gspWaitForVBlank();

    if (waveBuf[channel].status == NDSP_WBUF_DONE) {
      memcpy(waveBuf[channel].data_pcm16, (channel ? ch0 : ch1), 2048);
      DSP_FlushDataCache(waveBuf[channel].data_pcm16, 2048);
      ndspChnWaveBufAdd(0, &waveBuf[channel]);

      channel++;

      if (channel > 1) {
        break;
      }
    }
  }

  return;
}

int main(int argc, char const *argv[]) {
  PrintConsole topScreen;
  ndspWaveBuf waveBuf[2];
  uint32_t tmp = 0;
  uint32_t *buffer = NULL;
  uint32_t *ch0 = NULL, *ch1 = NULL;
  struct ADTSData header;
  struct BinaryRequest request;
  Result res = 0;

  gfxInitDefault();

  consoleInit(GFX_TOP, &topScreen);

  consoleSelect(&topScreen);

  printf("libctru streaming audio\n");
  ndspInit();

  ndspSetOutputMode(NDSP_OUTPUT_STEREO);

  ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
  ndspChnSetRate(0, SAMPLERATE);
  ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

  float mix[12];
  memset(mix, 0, sizeof(mix));
  mix[0] = 1.0;
  mix[1] = 1.0;
  ndspChnSetMix(0, mix);

  buffer = (uint32_t *)linearAlloc(tmp);
  ch0 = (uint32_t *)linearAlloc(2048);
  ch1 = (uint32_t *)linearAlloc(2048);

  memset(waveBuf, 0, sizeof(waveBuf));
  waveBuf[0].data_vaddr = ch0;
  waveBuf[0].nsamples = 1024;
  waveBuf[1].data_vaddr = ch1;
  waveBuf[1].nsamples = 1024;

  ndspChnWaveBufAdd(0, &waveBuf[0]);
  ndspChnWaveBufAdd(0, &waveBuf[1]);

  while (aptMainLoop()) {

    gfxSwapBuffers();
    gfxFlushBuffers();
    gspWaitForVBlank();

    request.codec = 1;
    request.cmd = 0;

    printf("Initializing DSP AAC decoder... ");

    res = DSP_WriteProcessPipe(3, &request, 32);

    printf("returned: %ld.\n", res);

    for (size_t i = 0; i <= assets_Fastigium___Come_In_aac_len;) {
      tmp = parse_adts(assets_Fastigium___Come_In_aac + i, &header);

      if (!tmp) {
        printf("\rADTS decode error at %x\n", i);
        break;
      }

      // send samples to DSP
      memcpy(buffer, assets_Fastigium___Come_In_aac + i, tmp);
      request.cmd = 1;
      request.src_addr = osConvertVirtToPhys(buffer);
      request.size = tmp;
      request.dst_addr_ch0 = osConvertVirtToPhys(ch0);
      request.dst_addr_ch1 = osConvertVirtToPhys(ch1);
      res = DSP_WriteProcessPipe(3, &request, 32);

      // -- submit decoded buffer to DSP
      send_dsp_loop(ch0, ch1, waveBuf);

      // free the buffers
      linearFree(buffer);
      linearFree(ch0);
      linearFree(ch1);

      i += tmp;
      printf("\r-> %d", i);
    }

    break;
  }

  ndspExit();

  // linearFree(audioBuffer);

  gfxExit();
  return 0;
}
