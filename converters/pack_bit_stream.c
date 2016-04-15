/**
 * pack_bit_stream.c - Convert 1-bit per byte stream to packed bit stream
 *
 * Convert a 1-bit per byte bit stream, as outputed by GnuRadio, to a packed bit
 * stream.  The output is a byte stream with 8 bits packed into one byte with
 * the MSB the first byte and the LSB the last.
 *
 * Usage: ./pack_bit_stream < in.gdat > out.dat
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
#include <stdint.h>

int main(int argc, char *argv[])
{
	int j;
	uint8_t b;
	size_t i;
	size_t len=0;
	unsigned char buf[1024];

	j=0;
	b=0;
	while ((len = fread(buf, 1, sizeof(buf), stdin)) > 0) {
		for (i=0; i<len; i++) {
			b <<= 1;
			b |= (buf[i] & 0x01);
			if (++j == 8) {
				fputc(b, stdout);
				j = 0;
				b = 0;
			}
		}
	}

	if (j < 8) {
		b <<= (8-j);
		fputc(b, stdout);
	}

	return 0;
}
