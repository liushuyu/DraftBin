// MFTest.cpp

#include "pch.h"
#include "test_data.h"
#include "main.h"
#include <iostream>
#include <fstream>

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
	mf_deinit(&transform);
	return 0;
}
