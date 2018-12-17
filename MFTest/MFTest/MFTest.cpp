// MFTest.cpp

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

	hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
	if (hr != S_OK)
	{
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

IMFSample* create_sample(void *data, DWORD len, DWORD alignment = 16) {
	HRESULT hr = S_OK;
	IMFMediaBuffer *buf = NULL;
	IMFSample *sample = NULL;

	hr = MFCreateSample(&sample);
	if (FAILED(hr)) return NULL;
	hr = MFCreateAlignedMemoryBuffer(len, alignment - 1, &buf);
	if (FAILED(hr)) return NULL;
	if (data)
	{
		BYTE *buffer;
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
	SafeRelease(&buf);
	return sample;
}

int select_input_mediatype(IMFTransform *transform, int in_stream_id, GUID audio_format = MFAudioFormat_AAC) {
	HRESULT hr = S_OK;
	GUID tmp;
	IMFMediaType *t;

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
				// 0: raw aac 1: adts 2: adif 3: latm/laos
				t->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 1);
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
		std::cout << "flush command failed" << std::endl;
	}
	hr = (*transform)->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
	if (FAILED(hr))
	{
		std::cout << "failed to end streaming for MFT" << std::endl;
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

		if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT && drain) {
			drain_complete = true;
		}
		else {
			std::cout << "MFT: decoding failure: " << hr << std::endl;
			break;
		}

		break;
	}

	// TODO: handle try again and EOF cases
	if (!out_sample && drain_complete)
	{
		return 0;
	}
	else
	{
		return 1; // try again
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
		std::cout << "Failed to get sample buffer" << std::endl;
		return -1;
	}

	hr = buffer->Lock(&data, NULL, NULL);
	if (FAILED(hr))
	{
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
		return 1;
	}

	std::cout << "OK\n";

	IMFTransform *transform = NULL;
	DWORD in_stream_id = 0;
	DWORD out_stream_id = 0;
	HRESULT hr = S_OK;
	MFT_INPUT_STREAM_INFO in_info;
	DWORD flags;

	if (mf_decoder_init(&transform) != 0)
	{
		return 1;
	}

	std::cout << "Decoder initialized" << std::endl;

	hr = transform->GetStreamIDs(1, &in_stream_id, 1, &out_stream_id);
	if (hr == E_NOTIMPL)
	{
		in_stream_id = 0;
		out_stream_id = 0;
	}
	else if (FAILED(hr)) {
		std::cout << "Decoder failed to initialize the stream ID" << std::endl;
		SafeRelease(&transform);
		return 1;
	}

	hr = transform->GetInputStreamInfo(in_stream_id, &in_info);

	// hr = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
	// hr = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

	if (argc < 2) {
		std::cout << "Dry run complete." << std::endl;
	}
	else {
		/* output_adts.aac (2018/12/14 20:52:15)
   StartOffset(h): 00000000, EndOffset(h): 00000172, Length(h): 00000173 */

		unsigned char buffer[371] = {
			0xFF, 0xF1, 0x50, 0x80, 0x2E, 0x7F, 0xFC, 0x21, 0x00, 0x05, 0x00, 0xA0,
			0x1B, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0xA3, 0x80, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7B
		};


		if (!buffer)
		{
			std::cout << "? buffer" << std::endl;
			goto end;
		}

			IMFSample *sample = NULL;
			IMFSample *output = NULL;
			int status = 0;
			select_input_mediatype(transform, in_stream_id);
			select_output_mediatype(transform, out_stream_id);
				hr = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
				sample = create_sample(buffer, 371);
				std::cout << "-- Sample created" << std::endl;
				status = send_sample(transform, in_stream_id, sample);
				std::cout << "-- Processing input... " << status << std::endl;
				transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
				transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
				transform->GetOutputStatus(&flags);
				std::cout << "-- MFT: " << ((flags & MFT_OUTPUT_STATUS_SAMPLE_READY) ? "Output Ready" : "Try again") << std::endl;
				std::cout << "-- Generating output... " << ( receive_sample(transform, out_stream_id, &output) == 0 ? "OK" : "Error" ) << std::endl;
	}

end:
	mf_flush(&transform);
	deinit(&transform);
	return 0;
}
