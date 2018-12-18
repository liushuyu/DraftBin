#pragma once

#ifndef MF_DECODER
#define MF_DECODER

#define WINVER _WIN32_WINNT_WIN7

#include <stdio.h>
#include <assert.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#include <comdef.h>

#include <iostream>
#include <string>

template <class T> void SafeRelease(T **ppT);
void ReportError(std::wstring msg, HRESULT hr);

// exported functions
int mf_coinit();
int mf_decoder_init(IMFTransform **transform, GUID audio_format = MFAudioFormat_AAC);
void mf_deinit(IMFTransform **transform);
IMFSample* create_sample(void *data, DWORD len, DWORD alignment = 1, LONGLONG duration = 0);
int select_input_mediatype(IMFTransform *transform, int in_stream_id, GUID audio_format = MFAudioFormat_AAC);
int select_output_mediatype(IMFTransform *transform, int out_stream_id, GUID audio_format = MFAudioFormat_PCM);
int mf_flush(IMFTransform **transform);
int send_sample(IMFTransform *transform, DWORD in_stream_id, IMFSample* in_sample);
int receive_sample(IMFTransform *transform, DWORD out_stream_id, IMFSample** out_sample);
int copy_sample_to_buffer(IMFSample* sample, void** output, DWORD* len);

#endif // MF_DECODER
