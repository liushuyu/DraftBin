// MFTest.cpp

#include "pch.h"
#include "test_data.h"
#include "main.h"
#include <iostream>
#include <fstream>

size_t convert_file(const char* filename, char** file_buf) {
	FILE* input = fopen(filename, "rb");
	size_t file_size = 0;
	*file_buf = NULL;

	if (input == NULL)
	{
		return 0;
	}

	std::cout << "File " << filename << "... opened";

	fseek(input, 0, SEEK_END);
	file_size = ftell(input);
	rewind(input);

	std::cout << " ... reading ...";

	*file_buf = (char*)malloc(file_size);
	fread(*file_buf, 1, file_size, input);

	std::cout << " ... copied ...";
	fclose(input);

	std::cout << "Done. \n";
	return file_size;
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
	DWORD output_len = 0;
	void *buffer = NULL;
	HRESULT hr = S_OK;
	DWORD flags;

	IMFSample *sample = NULL;
	IMFSample *output = NULL;
	ADTSData data;
	char *tag = (char*)calloc(1, 14);
	int status = 0;
	size_t sample_size = 300;
	char* sample_data = NULL;
	bool multi_frame = false;

	char wav_header[44] = {'R', 'I', 'F', 'F', 0xff, 0xff, 0xff, 0xff, 'W', 'A', 'V', 'E',
	'f', 'm', 't', ' ', 0x10, 0x00,0x00,0x00, 0x01, 0x00 , 0x02, 0x00,
	0x44, 0xac, 0x00, 0x00, 0x10, 0xb1, 0x02, 0x00, 0x04, 0x00, 0x10,
	0x00, 'd', 'a','t','a', 0xff, 0xff, 0xff, 0xff};
	uint32_t sample_total_len = 0;

	FILE* output_file = NULL;

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

	if (argc > 1)
	{
		sample_size = convert_file(argv[1], &sample_data);
		output_file = fopen("converted.wav", "wb");
		if (!output_file)
		{
			goto end;
		}
		fwrite(wav_header, 1, 44, output_file);
	}
	else {
		sample_data = (char*)sample_buffer;
	}

		if (!sample_data)
		{
			std::cout << "? buffer" << std::endl;
			goto end;
		}

		
		detect_mediatype((char*)sample_data, sample_size, &data, &tag);
		select_input_mediatype(transform, in_stream_id, data, (UINT8*)tag, 14);
		select_output_mediatype(transform, out_stream_id);
		free(tag);

		// optional messages, but can increase performance if you do this
		// b/c MFT will fully initialize if you notify it
		hr = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
		hr = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

		sample = create_sample((void*)sample_data, sample_size, 1, 0);
		std::cout << "-- Sample created" << std::endl;
		status = send_sample(transform, in_stream_id, sample);
		// status = send_sample(transform, in_stream_id, NULL);
		std::cout << "-- Processing input... " << (status == 0 ? "OK" : "Error") << std::endl;
		transform->GetOutputStatus(&flags);
		// the check below is very buggy for AAC MFT
		std::cout << "-- MFT: " << ((flags & MFT_OUTPUT_STATUS_SAMPLE_READY) ? "Output Ready" : "Try again") << std::endl;
		while (true)
		{
			status = receive_sample(transform, out_stream_id, &output);
			if (status <= 0)
			{
				break;
			}
			else if (status < 3)
			{
				status = multi_frame ? 0 : status;
				break;
			}
			copy_sample_to_buffer(output, &buffer, &output_len);
			multi_frame = true;
			if (output_file)
			{
				fwrite(buffer, 1, output_len, output_file);
				sample_total_len += output_len;
			}
			free(buffer);
		}
		// correct wav header
		if (output_file)
		{
			rewind(output_file);
			fseek(output_file, 40, SEEK_CUR);
			fwrite(&sample_total_len, 1, 4, output_file);
			fseek(output_file, -40, SEEK_CUR);
			sample_total_len += 36;
			fwrite(&sample_total_len, 1, 4, output_file);
			std::cout << "-- Output saved to converted.wav" << std::endl;
			goto end;
		}

		std::cout << "-- Generating output... " << (status == 0 ? "OK" : "Error") << std::endl;
		if (output)
		{
			std::cout << "-- Output received from MFT" << std::endl;
			status = copy_sample_to_buffer(output, &buffer, &output_len);
			std::cout << "-- Output received from MFT: " << (status == 0 ? "OK" : "Error") << ", Length = " << output_len << std::endl;
		}
		std::cout << "Dry run complete." << std::endl;
		goto end;

end:
	mf_flush(&transform);
	mf_deinit(&transform);
	return 0;
}
