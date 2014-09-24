/**
 * decode_somfy.c - Decode Somfy RTS packets from bit stream
 *
 * This tool decodes the Somfy RTS packets from a raw bit stream. The bit
 * stream is what comes out of the OOK demodulator and should be sampled 36 us
 * per sample. The samples are packed into a byte with the MSB the first bit
 * and the LSB the last.
 *
 * The Addresses of the remotes can be resolved to human readable names. This
 * is done by creating a file called 'remotes.txt' in the current directory.
 * The file should contain a hexadecimal remote address followed by the name.
 *
 * Usage:
 * ------
 * This program expects a raw bit stream outputted by a OOK demodulator as
 * input on stdin.
 *
 * One way to obtain this bitstream is using Software defined radio,
 * for instance RTL-SDR. In this the following command can be used.
 *   rtl_fm -M am -g 5 -f 433.42M -s 270K | \
 *      ./converters/am_to_ook -d 10 -t 1500 -  | \
 *      ./decoders/decode_somfy
 * Note that the rtl_fm gain and am_to_ook threshold values will need tweaking.
 * One way to do this is to first dump the rtl_fm output to file and then run
 * am_to_ook with the '-a' option to get the peak values. Then choose a
 * threshold somewhere below that peak value.
 *
 * An other way to obtain the bitstream is by using a microcontroller to sample
 * the output pin of a simple OOK 433 MHz receiver every ~36 us. Every byte
 * contains 8 samples with the MSB being the first sample.
 *
 * Copyright (c) 2014, David Imhoff <dimhoff_devel@xs4all.nl>
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>

int verbose = 0;
int one_line = 0;
int numeric = 0;

int data_len;
uint64_t data;
uint64_t data2;
unsigned int key2=0x11;
enum state_t { idle, preamble, data0, data1 } state = idle, new_state;
// idle -> preamble: new_level = 0 & len == 68 ~10%
// preamble -> data0: new_level = 0 & len == 130 ~5%
// preamble -> idle: len != 68 ~10%
// data0 -> data1: new_level = 0 & len == 17.5 ~10%
// data0 -> data0: new_level = 0 & len == 34.5 ~10%
// data1 -> data0: new_level = 0 & len == 17.5 ~10%
// data0 -> idle: else
// data1 -> idle: else

typedef uint64_t somfy_frame_t;

void usage(char *my_name) {
	fprintf(stderr, "Usage: %s [-1nvh]\n", my_name);
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, " -1    Use single line output mode\n");
	fprintf(stderr, " -n    Don't display human readable control and address names\n");
	fprintf(stderr, " -v    Increase verbose level, can be used multiple times\n");
	fprintf(stderr, " -h    Display this help\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "This program expects the raw bit stream form the OOK demodulator as input on\n");
	fprintf(stderr, "stdin. For example when using RTL-SDR the following command line can be used:\n");
	fprintf(stderr, "  rtl_fm -M am -g 5 -f 433.42M -s 270K | \\\n");
	fprintf(stderr, "  ./converters/am_to_ook -d 10 -t 1500 -  | \\\n");
	fprintf(stderr, "  ./decoders/decode_somfy\n");
	fprintf(stderr, "Note that the rtl_fm gain and am_to_ook threshold values will need tweaking\n");
}

uint8_t somfy_frame_get_encryption_key(somfy_frame_t frame) {
	return (frame >> (6*8)) & 0xFF;
}

uint8_t somfy_frame_get_control(somfy_frame_t frame) {
	return (frame >> (5*8 + 4)) & 0xF;
}

uint8_t somfy_frame_get_chksum(somfy_frame_t frame) {
	return (frame >> (5*8)) & 0xF;
}

uint16_t somfy_frame_get_rolling_code(somfy_frame_t frame) {
	return (frame >> (3*8)) & 0xFFFF;
}

uint32_t somfy_frame_get_addr(somfy_frame_t frame) {
	return ((frame & 0xFF) << 16) | (frame & 0xFF00) | ((frame & 0xFF0000) >> 16);
}

uint8_t somfy_calc_checksum(somfy_frame_t frame) {
	int checksum=0;
	int i;
	for (i=0; i < 14; i++) {
		checksum ^= frame & 0xf;
		frame = frame >> 4;
	}

	return checksum;
}

const char *somfy_frame_get_control_name(somfy_frame_t frame) {
	static const char *names[16] = {
		"c0",
		"MY",
		"UP",
		"MY+UP",
		"DOWN",
		"MY+DOWN",
		"UP+DOWN",
		"c7",
		"PROG",
		"SUN+FLAG",
		"FLAG",
		"c11",
		"c12",
		"c13",
		"c14",
		"c15"
	};
	
	return names[somfy_frame_get_control(frame)];
}


/************** address resolving ********************/
typedef struct somfy_host_cache_entry {
	uint32_t addr;
	char *name;
	struct somfy_host_cache_entry *next;
} somfy_hosts_cache_t;

somfy_hosts_cache_t *somfy_hosts_cache = NULL;

void somfy_hosts_cache_init(const char *cache_file)
{
	FILE *ifp;
	char *line = NULL;
	size_t len = 0;
	ssize_t bytes_read;

	if ((ifp = fopen(cache_file, "r")) == NULL) {
		return;
	}

	while ((bytes_read = getline(&line, &len, ifp)) != -1) {
		char *sp;

		// trim '\n', '\r' and other space from the end of the line
		while (bytes_read > 0 && isspace(line[bytes_read - 1])) {
			line[bytes_read - 1] = '\0';
			bytes_read--;
		}

		if (bytes_read < 8)
			continue;

		somfy_hosts_cache_t *entry = (somfy_hosts_cache_t *) malloc(sizeof(somfy_hosts_cache_t));

		// addr
		entry->addr = strtol(line, &sp, 16);
		if (sp != &(line[6]) || !isspace(*sp)) {
			free(entry);
			continue;
		}

		// delim
		while (isspace(*sp)) {
			sp++;
		}
		if (strlen(sp) == 0) {
			free(entry);
			continue;
		}

		// name
		entry->name = strdup(sp);

		entry->next = somfy_hosts_cache;
		somfy_hosts_cache = entry;
	}

	free(line);

}

#define ESAN_NONAME -1
int somfy_addr_to_name(uint32_t addr, char *dst, size_t len)
{
	somfy_hosts_cache_t *entry = somfy_hosts_cache;
	while (entry != NULL && entry->name != NULL) {
		if (entry->addr == addr) {
			strncpy(dst, entry->name, len);
			dst[len-1] = '\0';
			return 0;
		}
		entry = entry->next;
	}

	return ESAN_NONAME;
}

void print_frame_long(uint64_t frame)
{
	uint8_t checksum = 0;

	printf("%.14jx:\n", (uintmax_t) frame);

	checksum = somfy_calc_checksum(frame);
	if (checksum == 0) {
		uint32_t addr;
		char addr_str[1024];

		printf("checksum = OK\n");
		printf("Encryption Key = %.2x\n", somfy_frame_get_encryption_key(frame));
		printf("Control=%.2x", somfy_frame_get_control(frame));
		if (! numeric) {
			printf(" (%s)", somfy_frame_get_control_name(frame)); 
		}
		printf(", ");
		printf("Rolling Code = %.4x\n", somfy_frame_get_rolling_code(frame));
		
		addr = somfy_frame_get_addr(frame);
		printf("Address = %.6x", addr);
		if (! numeric && somfy_addr_to_name(addr, addr_str, sizeof(addr_str)) == 0) {
			printf(" (%s)", addr_str);
		}
		putchar('\n');
	} else {
		printf("checksum = FAILED (%.2x)\n", checksum);
	}
	printf("--------------------------------------------------------------------------------\n");
}

void print_frame_oneline(uint64_t frame)
{
	uint8_t checksum = 0;

	printf("%.14jx: ", (uintmax_t) frame);

	checksum = somfy_calc_checksum(frame);
	if (checksum == 0) {
		uint32_t addr;
		char addr_str[1024];

		printf("checksum=OK, ");
		printf("Encryption Key=%.2x, ", somfy_frame_get_encryption_key(frame));
		printf("Control=%.2x", somfy_frame_get_control(frame));
		if (! numeric) {
			printf("(%s)", somfy_frame_get_control_name(frame)); 
		}
		printf(", ");
		printf("Rolling Code=%.4x, ", somfy_frame_get_rolling_code(frame));
		
		addr = somfy_frame_get_addr(frame);
		printf("Address=%.6x", addr);
		if (! numeric && somfy_addr_to_name(addr, addr_str, sizeof(addr_str)) == 0) {
			printf("(%s)", addr_str);
		}
		putchar('\n');
	} else {
		printf("checksum=FAILED(%.2x)\n", checksum);
	}
}

void level_change_cb(int new_level, unsigned int len) 
{
	switch (state) {
	case idle:
		if (new_level == 0 && len >= 64 && len <= 72) {
			new_state = preamble;
		} else {
			new_state = idle;
		}
		break;
	case preamble:
		if (new_level == 0 && len >= 127 && len <= 133) {
			new_state = data0;
		} else if (len >= 64 && len <= 72) {
			new_state = preamble;
		} else {
			new_state = idle;
		}
		break;
	case data0:
		if (len >= 30 && len <= 40) {
			new_state = data0;
		} else if (len >= 10 && len <= 25) {
			new_state = data1;
		} else {
			new_state = idle;
		}
		break;
	case data1:
		if (len >= 10 && len <= 25) {
			new_state = data0;
		} else {
			new_state = idle;
		}
		break;
	}
	if ((state == data0 || state == data1) &&
	    (new_state != data0 && new_state != data1))
	{
		if (data_len) {
			if (verbose > 0) printf(", len=%u, dat=%jx\n", data_len, (uintmax_t) data);

			if (data_len == 56) {
				int j;
				uint64_t m=0xff000000000000;

				// Decrypt by for N=1..len: m[N] = c[N] ^ c[N-1]
				data2=data;
				for (j=0; j<6; j++) {
					data = (data ^ ((data2 & m) >> 8));
					m = m >> 8;
				}

				if (one_line) {
					print_frame_oneline(data);
				} else {
					print_frame_long(data);
				}
			}
		} else {
			if (verbose > 0) putchar('\n');
		}
	}
	if (state == preamble && new_state == data0) {
		data_len = 0;
		data = 0;
		if (verbose > 0) printf("start: ");
	}
	if ((state == data0 || state == data1) && new_state == data0) {
	/*
		if (state == data1) {
			printf("s %c\n", (new_level == 1) ? '^' : 'v');
		} else {
			printf("l %c\n", (new_level == 1) ? '^' : 'v');
		}
	*/
		data = (data << 1) | new_level;
		data_len++;
		if (verbose > 0) {
			printf("%d", new_level); // rising edge == 1, faling edge == 0
			if ((data_len % 8) == 0) {
				printf(" ");
			}
		}
	}
	// printf(" old: %d, new %d\n", state, new_state);


	state = new_state;
}

#define FILTER_DEPTH 8
#define FILTER_TRESHOLD 2

int main(int argc, char *argv[])
{
	int opt;
	int i, j;
	off_t sample=0;
	int level=0;
	int new_level=0;
	off_t last_change=0;
	size_t len=0;
	unsigned char buf[1024];
	unsigned int mask;
#ifdef WITH_LPF
	int one_cnt = 0;
	int filter_bits = 0;
#endif
	
	while ((opt = getopt(argc, argv, "1nvh")) != -1) {
		switch (opt) {
		case '1':
			one_line = 1;
			break;
		case 'n':
			numeric = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		default: /* '?' */
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	somfy_hosts_cache_init("remotes.txt");

	while ((len = fread(buf, 1, sizeof(buf), stdin)) > 0) {
		for (i=0; i<len; i++) {
			mask=0x80;
			for (j=0; j < 8; j++) {
#ifdef WITH_LPF
				if (filter_bits & (1 << FILTER_DEPTH)) {
					one_cnt--;
				}
				filter_bits <<= 1;
				if (((buf[i] & mask) != 0)) {
					filter_bits |= 0x01;
					one_cnt++;
				}

				if (level && one_cnt <= FILTER_TRESHOLD) {
					new_level = 0;
				} else if (! level && one_cnt >= (FILTER_DEPTH - FILTER_TRESHOLD)) {
					new_level = 1;
				} else {
					new_level = level;
				}
#else
				new_level = ((buf[i] & mask) != 0);
#endif

				if (new_level != level) {
					off_t len = sample - last_change;

//printf("@%ju %d->%d %ju\n", sample, !new_level, new_level, len);
					level_change_cb(new_level, len);

					level = new_level;
					last_change = sample;
				}
				sample++;
				mask = mask >> 1;
			}
		}
	}

	level_change_cb(!level, sample - last_change);

	printf("\n");
	return 0;
}
