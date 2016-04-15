/**
 * am_to_ook.c - Convert output of rtl_fm's AM demodulation to binary stream
 *
 * Convert output of rtl_fm's AM demodulation to binary stream by means of a set
 * threshold. The output is a byte stream with 8 bits packed into one byte with
 * the MSB the first byte and the LSB the last. Down-sampling can be used to
 * capture a wider band.
 *
 * Copyright (c) 2014, David Imhoff <dimhoff.devel@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the author nor the names of its contributors may
 *       be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>

#define THRESHOLD 0x4000

void usage(char *my_name) {
	fprintf(stderr, "Convert AM levels to Binary stream\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s [options] <input> <output>\n", my_name);
	fprintf(stderr, "   or: %s -a [options] <input>\n", my_name);
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-a            Analyse input file and print "
					"summary\n");
	fprintf(stderr, "\t-d <ratio>    Down-sample with given ratio\n");
	fprintf(stderr, "\t-t <level>    Set threshold above which a sample "
					"is considered '1'\n");
	fprintf(stderr, "\t-u            Don't pack output but use one bit "
					"per byte\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "When input or output are not specified or equal to\n");
	fprintf(stderr, "'-', stdin and stdout are used\n");
}

int main(int argc, char *argv[])
{
	FILE *ifp = stdin;
	FILE *ofp = stdout;

	int opt;
	bool do_analyse = false;
	bool output_unpacked = false;
	int downsample_rate = 1;
	uint16_t threshold = THRESHOLD;

	struct {
		unsigned int min_unsigned;
		unsigned int max_unsigned;
		int min_signed;
		int max_signed;
	} stats = { 0 };

	int downsample_threshold = 1;
	int downsample_cnt = 0;
	int one_cnt = 0;

	size_t len;
	uint8_t buf[4096];
	uint16_t *vals;
	size_t i;
	uint8_t b;
	int j;

	while ((opt = getopt(argc, argv, "ad:t:uh")) != -1) {
		switch (opt) {
		case 'a':
			do_analyse = true;
			break;
		case 'd':
			downsample_rate = strtol(optarg, NULL, 0);
			if (downsample_rate <= 0) {
				downsample_rate = 1;
			}
			break;
		case 't':
			threshold = strtol(optarg, NULL, 0);
			break;
		case 'u':
			output_unpacked = true;
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		default: /* '?' */
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (argc - optind > 2) {
		fprintf(stderr, "Too many arguments\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	// Input file
	if (argc - optind > 0) {
		if (strcmp(argv[optind], "-") != 0) {
			if ((ifp = fopen(argv[optind], "rb")) == NULL) {
				perror("Failed opening input file");
				exit(EXIT_FAILURE);
			}
		}
		optind++;
	}

	// Output file
	if (argc - optind > 0 && ! do_analyse) {
		if (strcmp(argv[optind], "-") != 0) {
			if ((ofp = fopen(argv[optind], "wb")) == NULL) {
				if (ifp != stdin)
					fclose(ifp);
				perror("Failed opening output file");
				exit(EXIT_FAILURE);
			}
		}
		optind++;
	}

	if (downsample_rate != 1) {
		downsample_threshold = downsample_rate >> 1;
	}

	b=0;
	j=0;
	one_cnt=0;
	downsample_cnt=0;
	while ((len = fread(buf, 1, sizeof(buf), ifp)) > 0) {
		//NOTE: From the doc's I expected the output to be signed, but
		// the range of the AM demodulated data seems to be in the
		// order of 0 -> (2^31 + a bit). So using unsigned.
		vals = (uint16_t *) buf;
		for (i = 0; i < len / 2; i++) {
			if (do_analyse) {
				if (vals[i] > stats.max_unsigned)
					stats.max_unsigned = vals[i];
				if (vals[i] < stats.min_unsigned)
					stats.min_unsigned = vals[i];
				if ((int16_t) vals[i] > stats.max_signed)
					stats.max_signed = vals[i];
				if ((int16_t) vals[i] < stats.min_signed)
					stats.min_signed = vals[i];
			} else {
				if (vals[i] > threshold) {
					one_cnt++;
				}
				if (++downsample_cnt == downsample_rate) {
					b <<= 1;
					if (one_cnt >= downsample_threshold) {
						b |= 1;
					}
					if (++j == 8 || output_unpacked) {
						fputc(b, ofp);
						j = 0;
						b = 0;
					}
					one_cnt = 0;
					downsample_cnt = 0;
				}
			}
		}
	}

	if (do_analyse)	{
		printf("Analysis\n");
		printf("--------\n");
		printf("Unsigned Minimal level: %u\n", stats.min_unsigned);
		printf("Unsigned Maximum level: %u\n", stats.max_unsigned);
		printf("Signed Minimal level: %u\n", stats.min_signed);
		printf("Signed Maximum level: %u\n", stats.max_signed);
	} else {
		if (j != 0) {
			b <<= (8-j);
			fputc(b, ofp);
		}
	}

	if (ifp != stdin)
		fclose(ifp);
	if (ofp != stdout)
		fclose(ofp);
	return EXIT_SUCCESS;
}
