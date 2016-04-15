/**
 * dat_to_vcd.c - Convert FX2 logger output to VCD file for GTKWave
 *
 * Convert the FX2 logger binary trace format to a VCD file so it can be used
 * in GTKWave.
 * Usage: ./dat_to_vcd < in.dat > out.vcd
 * TODO: Currently the sample rate is fixed on 1 sample per 41.666667 us(24 KHz).
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

int main(int argc, char *argv[])
{
	int i, j;
	int sample=0;
	int val, last_val=0;
	size_t len=0;
	unsigned char buf[1024];

	printf("$date Sat Aug 25 11:46:27 2012 $end\n");
	printf("$version fx2_logger 0.1 $end\n");
	printf("$timescale 41667 ns $end\n");
	printf("$scope module fx2 $end\n");
	printf("$var wire 1 ! 1 $end\n");
	printf("$upscope $end\n");
	printf("$enddefinitions $end\n");
	printf("$dumpvars\n");

	while ((len = fread(buf, 1, sizeof(buf), stdin)) > 0) {
		for (i=0; i<len; i++) {
			for (j=0; j < 8; j++) {
				if ((buf[i] << j) & 0x80) {
					val = 1;
				} else {
					val = 0;
				}
				if (val != last_val || sample == 0) {
					printf("#%u\n%u!\n", sample, val);
				}
				last_val = val;
				sample++;
			}
		}
	}
	printf("$dumpoff\n");
	printf("$end\n");

	return 0;
}
