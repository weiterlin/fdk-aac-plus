/* ------------------------------------------------------------------
 * Copyright (C) 2013 Martin Storsjo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include "libAACdec/include/aacdecoder_lib.h"
#include "wavwriter.h"

int main(int argc, char *argv[]) {
	const char *infile, *outfile;
	FILE *in;
	void *wav = NULL;
	int output_size;
	uint8_t *output_buf;
	int16_t *decode_buf;
	HANDLE_AACDECODER handle;
	int frame_size = 0;
	if (argc < 3) {
		fprintf(stderr, "%s in.raw out.wav\n", argv[0]);
		return 1;
	}
	infile = argv[1];
	outfile = argv[2];
	handle = aacDecoder_Open(TT_MP4_RAW, 1);
	in = fopen(infile, "rb");
	if (!in) {
		perror(infile);
		return 1;
	}
	output_size = 8*2*1024;
	output_buf = (uint8_t*) malloc(output_size);
	decode_buf = (int16_t*) malloc(output_size);
	uint8_t packet[10240];
    int count = 1;
    long offset;
	while (1) {
	    //offset = ftell(in);
		//printf("offset = %ld\n",offset); 
		int n = fread(packet, 1, 2, in);
		if (n != 2)
			break;
	    //fprintf(stderr, "%d,%d\n",packet[0],packet[1]);
		uint32_t extradata_size = packet[0] >> 2;
		uint32_t framedata_size = (packet[0]&0x03) << 8 | packet[1];

		uint32_t packet_size = extradata_size + framedata_size;
		//fprintf(stderr, "extradata_size=%d,framedata_size=%d,packet_size=%d,count=%d\n",
		//    extradata_size, framedata_size,packet_size,count++);
		n = fread(packet, 1, packet_size, in);
		if (n != packet_size) {
			fprintf(stderr, "Partial packet\n");
			break;
		}
	    //offset = ftell(in);
		//printf("offset = %ld\n",offset);  
		uint8_t *extradata = packet;
	    AAC_DECODER_ERROR err = aacDecoder_ConfigRaw(handle, &extradata, &extradata_size);
	    if (err != AAC_DEC_OK) {
		    fprintf(stderr, "Unable to decode the ASC\n");
		    return 1;
	    }
	    uint8_t *ptr = packet + extradata_size;
		UINT valid = framedata_size;
		//fprintf(stderr, "ptr=%x,%x,%x,%x\n",ptr[0],ptr[1],ptr[2],ptr[3]);
		err = aacDecoder_Fill(handle, &ptr, &framedata_size, &valid);
		if (err != AAC_DEC_OK) {
			fprintf(stderr, "Fill failed: %x\n", err);
			break;
		}
		err = aacDecoder_DecodeFrame(handle, decode_buf, output_size, 0);
		if (err == AAC_DEC_NOT_ENOUGH_BITS)
			continue;
		if (err != AAC_DEC_OK) {
			fprintf(stderr, "Decode failed: %x\n", err);
			continue;
		}
		if (!wav) {
			CStreamInfo *info = aacDecoder_GetStreamInfo(handle);
			fprintf(stderr, "stream info aacSamplesPerFrame=%d\n",info->aacSamplesPerFrame);
			if (!info || info->sampleRate <= 0) {
				fprintf(stderr, "No stream info\n");
				break;
			}
			frame_size = info->frameSize * info->numChannels;
			// Note, this probably doesn't return channels > 2 in the right order for wav
			wav = wav_write_open(outfile, info->sampleRate, 16, info->numChannels);
			if (!wav) {
				perror(outfile);
				break;
			}
		}
		for (int i = 0; i < frame_size; i++) {
			uint8_t* out = &output_buf[2*i];
			out[0] = decode_buf[i] & 0xff;
			out[1] = decode_buf[i] >> 8;
		}
		wav_write_data(wav, output_buf, 2*frame_size);
	}
	free(output_buf);
	free(decode_buf);
	fclose(in);
	if (wav)
		wav_write_close(wav);
	aacDecoder_Close(handle);
	return 0;
}

