/* ------------------------------------------------------------------
 * Copyright (C) 2011 Martin Storsjo
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

#if defined(_MSC_VER)
#include <getopt.h>
#else
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include "libAACenc/include/aacenc_lib.h"
#include "wavreader.h"
#include "bits.h"

void usage(const char* name) {
	fprintf(stderr, "%s [-r bitrate] [-t aot] [-s sbr] [-l framelength] in.wav out.raw\n", name);
	fprintf(stderr, "Supported AOTs:\n");
	fprintf(stderr, "\t2\tAAC-LC\n");
	fprintf(stderr, "\t5\tHE-AAC\n");
	fprintf(stderr, "\t29\tHE-AAC v2\n");
	fprintf(stderr, "\t23\tAAC-LD\n");
	fprintf(stderr, "\t39\tAAC-ELD\n");
    fprintf(stderr, "Supported sbr:\n");
    fprintf(stderr, "0: Use lib default (default)\n");
	fprintf(stderr, "1: downsampled SBR for ELD+SBR or dual-rate SBR for HE-AAC)\n");
	fprintf(stderr, "Supported framelength:\n");
	fprintf(stderr, "960|1024: AAC-LC/HE-AAC/HE-AAC v2\n");
	fprintf(stderr, "480|512: AAC-LD/AAC-ELD\n");
}

int main(int argc, char *argv[]) {
	int bitrate = 32000;
	int ch;
	const char *infile, *outfile;
	FILE *out;
	void *wav;
	int format, sample_rate, channels, bits_per_sample;
	int input_size;
	uint8_t* input_buf;
	int16_t* convert_buf;
	int aot = 2;
	int afterburner = 1;
	int sbr = 0;
    int framelength = 960;
    int transport_format = 0;
	HANDLE_AACENCODER handle;
	CHANNEL_MODE mode;
	AACENC_InfoStruct info = { 0 };
	while ((ch = getopt(argc, argv, "r:t:s:l:")) != -1) {
		switch (ch) {
		case 'r':
			bitrate = atoi(optarg);
			break;
		case 't':
			aot = atoi(optarg);
			break;
		case 's':
			sbr = atoi(optarg);
			break;
		case 'l':
			framelength = atoi(optarg);
			break;
		case '?':
		default:
			usage(argv[0]);
			return 1;
		}
	}
	if (argc - optind < 2) {
		usage(argv[0]);
		return 1;
	}
	infile = argv[optind];
	outfile = argv[optind + 1];
	if (framelength != 1024 && framelength != 960 
        && framelength != 512 && framelength != 480) {
		fprintf(stderr, "framelength = %d is error!\n", framelength);
		return 1;
	}
	wav = wav_read_open(infile);
	if (!wav) {
		fprintf(stderr, "Unable to open wav file %s\n", infile);
		return 1;
	}
	if (!wav_get_header(wav, &format, &channels, &sample_rate, &bits_per_sample, NULL)) {
		fprintf(stderr, "Bad wav file %s\n", infile);
		return 1;
	}
	if (format != 1) {
		fprintf(stderr, "Unsupported WAV format %d\n", format);
		return 1;
	}
	if (bits_per_sample != 16) {
		fprintf(stderr, "Unsupported WAV sample depth %d\n", bits_per_sample);
		return 1;
	}
	if (aot == 29 && channels == 1) {
	    fprintf(stderr, "HE-AAC(v2) is must channels 2, but current channels is %d\n", channels);
	    return 1;
	}
	switch (channels) {
	case 1: mode = MODE_1;       break;
	case 2: mode = MODE_2;       break;
	case 3: mode = MODE_1_2;     break;
	case 4: mode = MODE_1_2_1;   break;
	case 5: mode = MODE_1_2_2;   break;
	case 6: mode = MODE_1_2_2_1; break;
	default:
		fprintf(stderr, "Unsupported WAV channels %d\n", channels);
		return 1;
	}
	if (aacEncOpen(&handle, 0, channels) != AACENC_OK) {
		fprintf(stderr, "Unable to open encoder\n");
		return 1;
	}
	if (aacEncoder_SetParam(handle, AACENC_AOT, aot) != AACENC_OK) {
		fprintf(stderr, "Unable to set the AOT\n");
		return 1;
	}
    if (aacEncoder_SetParam(handle, AACENC_GRANULE_LENGTH, framelength) != AACENC_OK) {
		fprintf(stderr, "framelength = %d is error!\n", framelength);
        return 1;
    }
	if (aot == 39 && sbr) {
	    if (aacEncoder_SetParam(handle, AACENC_SBR_MODE, 1) != AACENC_OK) {
		    fprintf(stderr, "Unable to set SBR mode for ELD\n");
		    return 1;
	    }
	    if (aacEncoder_SetParam(handle, AACENC_SBR_RATIO, 1) != AACENC_OK) {
		    fprintf(stderr, "Unable to set SBR rate for ELD\n");
		    return 1;
	    }
	    if (aacEncoder_SetParam(handle, AACENC_SIGNALING_MODE, 1) != AACENC_OK) {
		    fprintf(stderr, "Unable to set SBR rate for ELD\n");
		    return 1;
	    }
	}
	if ((aot == 5 || aot == 29) && sbr) {
	    if (aacEncoder_SetParam(handle, AACENC_SBR_RATIO, 2) != AACENC_OK) {
		    fprintf(stderr, "Unable to set SBR rate for HE-AAC(v2)\n");
		    return 1;
	    }
	    if (aacEncoder_SetParam(handle, AACENC_SIGNALING_MODE, 1) != AACENC_OK) {
		    fprintf(stderr, "Unable to set SBR rate for HE-AAC(v2)\n");
		    return 1;
	    }
    }
	if (aacEncoder_SetParam(handle, AACENC_SAMPLERATE, sample_rate) != AACENC_OK) {
		fprintf(stderr, "Unable to set the AOT\n");
		return 1;
	}
	if (aacEncoder_SetParam(handle, AACENC_CHANNELMODE, mode) != AACENC_OK) {
		fprintf(stderr, "Unable to set the channel mode\n");
		return 1;
	}
	if (aacEncoder_SetParam(handle, AACENC_CHANNELORDER, 1) != AACENC_OK) {
		fprintf(stderr, "Unable to set the wav channel order\n");
		return 1;
	}
    if (aacEncoder_SetParam(handle, AACENC_BITRATE, bitrate) != AACENC_OK) {
	    fprintf(stderr, "Unable to set the bitrate\n");
	    return 1;
    }
	if (aacEncoder_SetParam(handle, AACENC_TRANSMUX, transport_format) != AACENC_OK) {
		fprintf(stderr, "Unable to set the ADTS transmux\n");
		return 1;
	}
	if (aacEncoder_SetParam(handle, AACENC_AFTERBURNER, afterburner) != AACENC_OK) {
		fprintf(stderr, "Unable to set the afterburner mode\n");
		return 1;
	}
	int ret = aacEncEncode(handle, NULL, NULL, NULL, NULL);
	if (ret != AACENC_OK) {
		fprintf(stderr, "Unable to initialize the encoder ret=%x\n",ret);
		return 1;
	}
	if (aacEncInfo(handle, &info) != AACENC_OK) {
		fprintf(stderr, "Unable to get the encoder info\n");
		return 1;
	}
	fprintf(stderr, "info frameLength=%d, confSize=%d\n", info.frameLength,info.confSize);
    uint8_t *conf = info.confBuf;
    int i;
    fprintf(stderr, "conf=[ ");
    for(i=0; i < info.confSize; i++) {
        fprintf(stderr, "%x ,",conf[i]);
    }
    fprintf(stderr, " ]\n");
	out = fopen(outfile, "wb");
	if (!out) {
		perror(outfile);
		return 1;
	}

	input_size = channels*2*info.frameLength;
	input_buf = (uint8_t*) malloc(input_size);
	convert_buf = (int16_t*) malloc(input_size);
	uint8_t *output_buf = (uint8_t*) malloc(input_size);
    uint8_t outbuf[20480];
    int flags = 1;
    int count = 1;
	while (1) {
		AACENC_BufDesc in_buf = { 0 }, out_buf = { 0 };
		AACENC_InArgs in_args = { 0 };
		AACENC_OutArgs out_args = { 0 };
		int in_identifier = IN_AUDIO_DATA;
		int in_size, in_elem_size;
		int out_identifier = OUT_BITSTREAM_DATA;
		int out_size, out_elem_size;
		int read, i;
		void *in_ptr, *out_ptr;
		AACENC_ERROR err;
		read = wav_read_data(wav, input_buf, input_size);
		for (i = 0; i < read/2; i++) {
			const uint8_t* in = &input_buf[2*i];
			convert_buf[i] = in[0] | (in[1] << 8);
		}
		if (read <= 0) {
			in_args.numInSamples = -1;
		} else {
			in_ptr = convert_buf;
			in_size = read;
			in_elem_size = 2;

			in_args.numInSamples = read/2;
			in_buf.numBufs = 1;
			in_buf.bufs = &in_ptr;
			in_buf.bufferIdentifiers = &in_identifier;
			in_buf.bufSizes = &in_size;
			in_buf.bufElSizes = &in_elem_size;
		}
    
		out_ptr = outbuf;
		out_size = sizeof(outbuf);
		out_elem_size = 1;
		out_buf.numBufs = 1;
		out_buf.bufs = &out_ptr;
		out_buf.bufferIdentifiers = &out_identifier;
		out_buf.bufSizes = &out_size;
		out_buf.bufElSizes = &out_elem_size;
		if ((err = aacEncEncode(handle, &in_buf, &out_buf, &in_args, &out_args)) != AACENC_OK) {
			if (err == AACENC_ENCODE_EOF)
				break;
			fprintf(stderr, "Encoding failed\n");
			return 1;
		}
		if (out_args.numOutBytes == 0)
			continue;
	    uint16_t value = info.confSize << 10 | out_args.numOutBytes;
        uint8_t *s = output_buf;
        uint32_t length = 2;
        bits_wb16(&s, value);
        length += info.confSize;
        bits_write(&s, info.confBuf, info.confSize);
        length += out_args.numOutBytes;
        bits_write(&s, outbuf, out_args.numOutBytes);
	    if (flags) {
	        long offset = ftell(out);
		    printf("offset = %ld\n",offset);
		    uint8_t *ptr = output_buf;
	        fprintf(stderr, "ptr=%x,%x,%x,%x\n",ptr[0],ptr[1],ptr[2],ptr[3]);
		    fprintf(stderr, "Encoding numOutBytes=%d,length=%d,count=%d\n",
		        out_args.numOutBytes,length,count++);
		    flags = 0;
        }
	    fwrite(output_buf, 1, length, out);
	}
	free(input_buf);
	free(convert_buf);
	fclose(out);
	wav_read_close(wav);
	aacEncClose(&handle);

	return 0;
}

