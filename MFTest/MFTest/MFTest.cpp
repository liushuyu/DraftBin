﻿// MFTest.cpp

#include "pch.h"
#include "main.h"
#include <iostream>
#include <fstream>

bool drain = false;
bool drain_complete = false;

int mf_coinit() {
	HRESULT hr = S_OK;

	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (hr != RPC_S_OK)
	{
		std::cout << "Failed to initialize COM" << std::endl;
		return -1;
	}

	// lite startup is faster and all what we need is included
	hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
	if (hr != S_OK)
	{
		// Do you know you can't initialize MF in test mode or safe mode?
		std::cout << "Failed to initialize Media Foundation" << std::endl;
		return -1;
	}

	return 0;
}

int mf_decoder_init(IMFTransform **transform, GUID audio_format = MFAudioFormat_AAC) {
	HRESULT hr = S_OK;
	MFT_REGISTER_TYPE_INFO reg = { 0 };
	GUID category = MFT_CATEGORY_AUDIO_DECODER;
	IMFActivate **activate;
	UINT32 num_activate;

	reg.guidMajorType = MFMediaType_Audio;
	reg.guidSubtype = audio_format;

	hr = MFTEnumEx(category, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_SORTANDFILTER, &reg, NULL, &activate, &num_activate);
	if (FAILED(hr) || num_activate < 1)
	{
		std::cout << "Failed to enumerate decoders" << std::endl;
		CoTaskMemFree(activate);
		return -1;
	}
	std::cout << "Found " << num_activate << " decoder(s)" << std::endl;
	for (unsigned int n = 0; n < num_activate; n++) {
		hr = activate[n]->ActivateObject(IID_IMFTransform, (void**)transform);
		if (FAILED(hr)) *transform = NULL;
		activate[n]->Release();
	}
	if (*transform == NULL)
	{
		std::cout << "Failed to initialize MFT" << std::endl;
		CoTaskMemFree(activate);
		return -1;
	}
	CoTaskMemFree(activate);
	return 0;
}

void deinit(IMFTransform **transform) {
	MFShutdownObject(*transform);
	SafeRelease(transform);
	CoUninitialize();
}

IMFSample* create_sample(void *data, DWORD len, DWORD alignment = 1, LONGLONG duration = 0) {
	HRESULT hr = S_OK;
	IMFMediaBuffer *buf = NULL;
	IMFSample *sample = NULL;

	hr = MFCreateSample(&sample);
	if (FAILED(hr)) return NULL;
	// Yes, the argument for alignment is the actual alignment - 1
	hr = MFCreateAlignedMemoryBuffer(len, alignment - 1, &buf);
	if (FAILED(hr)) return NULL;
	if (data)
	{
		BYTE *buffer;
		// lock the MediaBuffer
		// this is actually not a thread-safe lock
		hr = buf->Lock(&buffer, NULL, NULL);
		if (FAILED(hr))
		{
			SafeRelease(&sample);
			SafeRelease(&buf);
			return NULL;
		}

		memcpy(buffer, data, len);

		buf->SetCurrentLength(len);
		buf->Unlock();
	}

	sample->AddBuffer(buf);
	hr = sample->SetSampleDuration(duration);
	SafeRelease(&buf);
	return sample;
}

int select_input_mediatype(IMFTransform *transform, int in_stream_id, GUID audio_format = MFAudioFormat_AAC) {
	HRESULT hr = S_OK;
	GUID tmp;
	IMFMediaType *t;

	// actually you can get rid of the whole block of searching and filtering mess
	// if you know the exact parameters of your media stream
	for (DWORD i = 0; ; i++)
	{
		hr = transform->GetInputAvailableType(in_stream_id, i, &t);
		if (hr == MF_E_NO_MORE_TYPES || hr == E_NOTIMPL)
		{
			return 0;
		}
		if (FAILED(hr))
		{
			std::cout << "failed to get input types for MFT." << std::endl;
			continue;
		}

		hr = t->GetGUID(MF_MT_SUBTYPE, &tmp);
		if (!FAILED(hr))
		{
			if (IsEqualGUID(tmp, audio_format)) {
				// see https://docs.microsoft.com/en-us/windows/desktop/medfound/aac-decoder#example-media-types
				// and https://docs.microsoft.com/zh-cn/windows/desktop/api/mmreg/ns-mmreg-heaacwaveinfo_tag
				// for the meaning of the byte array below

				// for integrate into a larger project, it is recommended to wrap the parameters into a struct
				// and pass that struct into the function
				const UINT8 aac_data[] = { 0x01, 0x00, 0xfe, 00, 00, 00, 00, 00, 00, 00, 00, 00, 0x12, 0x10 };
				// 0: raw aac 1: adts 2: adif 3: latm/laos
				t->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 1);
				t->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
				t->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
				// 0xfe = 254 = "unspecified"
				t->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 254);
				t->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
				t->SetBlob(MF_MT_USER_DATA, aac_data, 14);
				hr = transform->SetInputType(in_stream_id, t, 0);
				if (FAILED(hr))
				{
					std::cout << "failed to select input types for MFT." << std::endl;
					return -1;
				}
				return 0;
			}
		}

		return -1;
	}
	return -1;
}

int select_output_mediatype(IMFTransform *transform, int out_stream_id, GUID audio_format = MFAudioFormat_PCM) {
	HRESULT hr = S_OK;
	UINT32 tmp;
	IMFMediaType *t;

	// If you know what you need and what you are doing, you can specify the condition instead of searching
	// but it's better to use search since MFT may or may not support your output parameters
	for (DWORD i = 0; ; i++)
	{
		hr = transform->GetOutputAvailableType(out_stream_id, i, &t);
		if (hr == MF_E_NO_MORE_TYPES || hr == E_NOTIMPL)
		{
			return 0;
		}
		if (FAILED(hr))
		{
			std::cout << "failed to get output types for MFT." << std::endl;
			continue;
		}

		hr = t->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &tmp);

		if (FAILED(hr)) continue;
		if (tmp == 16)
		{
			hr = transform->SetOutputType(out_stream_id, t, 0);
			if (FAILED(hr))
			{
				std::cout << "failed to select output types for MFT." << std::endl;
				return -1;
			}
			return 0;
		}
		else {
			continue;
		}

		return -1;
	}
	return -1;
}

int mf_flush(IMFTransform **transform) {
	HRESULT hr = (*transform)->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
	if (FAILED(hr))
	{
		std::cout << "Flush command failed: " << hr << std::endl;
	}
	hr = (*transform)->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
	if (FAILED(hr))
	{
		std::cout << "Failed to end streaming for MFT" << hr << std::endl;
	}

	drain = false;
	drain_complete = false;
	return 0;
}

int send_sample(IMFTransform *transform, DWORD in_stream_id, IMFSample* in_sample) {
	HRESULT hr = S_OK;

	if (in_sample)
	{
		hr = transform->ProcessInput(in_stream_id, in_sample, 0);
		if (hr == MF_E_NOTACCEPTING)
		{
			return 1; // try again
		}
		else if (FAILED(hr)) {
			std::cout << "MFT: Failed to process input: " << hr << std::endl;
			return -1;
		} // FAILED(hr)
	} /** in_sample **/ else if (!drain) {
		hr = transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
		// ffmpeg: Some MFTs (AC3) will send a frame after each drain command (???), so
		// ffmpeg: this is required to make draining actually terminate.
		if (FAILED(hr)) {
			std::cout << "MFT: Failed to drain when processing input" << std::endl;
		}
		drain = true;
	}
	else {
		return 0; // EOF
	}


	return 0;
}

int receive_sample(IMFTransform *transform, DWORD out_stream_id, IMFSample** out_sample) {
	HRESULT hr;
	MFT_OUTPUT_DATA_BUFFER out_buffers;
	IMFSample *sample = NULL;
	MFT_OUTPUT_STREAM_INFO out_info;
	DWORD status = 0;
	bool mft_create_sample = false;

	hr = transform->GetOutputStreamInfo(out_stream_id, &out_info);

	if (FAILED(hr))
	{
		std::cout << "MFT: Failed to get stream info" << std::endl;
		return -1;
	}
	mft_create_sample = (out_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) || (out_info.dwFlags & MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES);

	while (true)
	{
		sample = NULL;
		*out_sample = NULL;
		status = 0;

		if (!mft_create_sample)
		{
			sample = create_sample(NULL, out_info.cbSize, out_info.cbAlignment);
			if (!sample)
			{
				std::cout << "MFT: Unable to allocate memory for samples" << std::endl;
				return -1;
			}
		}

		out_buffers.dwStreamID = out_stream_id;
		out_buffers.pSample = sample;

		hr = transform->ProcessOutput(0, 1, &out_buffers, &status);

		if (!FAILED(hr))
		{
			*out_sample = out_buffers.pSample;
			break;
		}

		if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
			// TODO: handle try again and EOF cases using drain value
			ReportError(L"MFT: decoding failure", hr);
			break;
		}

		break;
	}

	if (out_sample != NULL)
	{
		return 0;
	}

	// TODO: handle try again and EOF cases using drain value
	if (!out_sample)
	{
		ReportError(L"MFT: decoding failure", hr);
		return 1;
	}

	return 0;
}

int copy_sample_to_buffer(IMFSample* sample, void** output, DWORD* len) {
	IMFMediaBuffer *buffer;
	HRESULT hr = S_OK;
	BYTE *data;

	hr = sample->GetTotalLength(len);
	if (FAILED(hr))
	{
		std::cout << "Failed to get the length of sample buffer" << std::endl;
		return -1;
	}

	sample->ConvertToContiguousBuffer(&buffer);
	if (FAILED(hr))
	{
		ReportError(L"Failed to get sample buffer", hr);
		return -1;
	}

	hr = buffer->Lock(&data, NULL, NULL);
	if (FAILED(hr))
	{
		ReportError(L"Failed to lock the buffer", hr);
		SafeRelease(&buffer);
		return -1;
	}

	*output = malloc(*len);
	memcpy(*output, data, *len);

	buffer->Unlock();
	SafeRelease(&buffer);
	return 0;
}

int main(int argc, const char* argv[])
{
    std::cout << "Initializing MF...";

	if (mf_coinit())
	{
		std::cout << "Error\n";
		return 1;
	}

	std::cout << "OK\n";

	IMFTransform *transform = NULL;
	DWORD in_stream_id = 0;
	DWORD out_stream_id = 0;
	HRESULT hr = S_OK;
	DWORD flags;

	if (mf_decoder_init(&transform) != 0)
	{
		return 1;
	}

	std::cout << "Decoder initialized" << std::endl;

	hr = transform->GetStreamIDs(1, &in_stream_id, 1, &out_stream_id);
	if (hr == E_NOTIMPL)
	{
		// if not implemented, it means this MFT does not assign stream ID for you
		in_stream_id = 0;
		out_stream_id = 0;
	}
	else if (FAILED(hr)) {
		ReportError(L"Decoder failed to initialize the stream ID", hr);
		SafeRelease(&transform);
		return 1;
	}

	if (argc < 2) {
		if (!sample_buffer)
		{
			std::cout << "? buffer" << std::endl;
			goto end;
		}

		IMFSample *sample = NULL;
		IMFSample *output = NULL;
		int status = 0;
		select_input_mediatype(transform, in_stream_id);
		select_output_mediatype(transform, out_stream_id);

		// optional messages, but can increase performance if you do this
		// b/c MFT will fully initialize if you notify it
		hr = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
		hr = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

		sample = create_sample((void*)sample_buffer, 1486, 1, 0);
		std::cout << "-- Sample created" << std::endl;
		status = send_sample(transform, in_stream_id, sample);
		std::cout << "-- Processing input... " << (status == 0 ? "OK" : "Error") << std::endl;
		transform->GetOutputStatus(&flags);
		// the check below is very buggy for AAC MFT
		std::cout << "-- MFT: " << ((flags & MFT_OUTPUT_STATUS_SAMPLE_READY) ? "Output Ready" : "Try again") << std::endl;
		std::cout << "-- Generating output... " << (receive_sample(transform, out_stream_id, &output) == 0 ? "OK" : "Error") << std::endl;
		std::cout << "Dry run complete." << std::endl;
		goto end;
	}

end:
	mf_flush(&transform);
	deinit(&transform);
	return 0;
}