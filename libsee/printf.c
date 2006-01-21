/*
 * Copyright (c) 2006
 *      David Leonard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Mr Leonard nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID LEONARD AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID LEONARD OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 
 * A simplified printf implementation. Formats understood are:
 *
 *      %[-|0][#]d     signed int
 *      %[-|0][#]u     unsigned int
 *      %[-|0][#]x     unsigned int
 *      %c             char                [0x00..0x7f]
 *      %C             SEE_char_t          [0x00..0xffff]
 *      %[-][#][.#]s   char *
 *      %[-][#][.#]S   struct SEE_string *
 *      %[-][#]p       void *
 *
 * Arguments to %s,%S,%c,%C are padded without heeding Unicode combining chars
 * The format string and arguments to %s and %c are assumed to be 7 bit ASCII
 * Unknown %-escapes are passed without change
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_STRING_H
# include <string.h>
#endif

#include <see/type.h>
#include <see/string.h>
#include <see/mem.h>

#include "printf.h"

#define isdigit(c) ((c) >= '0' && (c) <= '9')

#define UNDEF (-1)
#define STAR (-2)

/* Returns the number of digits required to represent the
 * unsigned integer n in digits of the given base */
static unsigned int
baselen(n, base)
	unsigned int n, base;
{
	unsigned int digits;

	digits = 1;
	while (n >= base) {
	    digits++;
	    n /= base;
	}
	return digits;
}

/*
 * Fills in a given string structure, allocating a new SEE_char_t array
 * containing the format string, with %-expansions.
 */
void
_SEE_vsprintf(interp, s, fmt, ap)
    struct SEE_interpreter *interp;
    struct SEE_string *s;
    const char *fmt;
    va_list ap;
{
    va_list ap0;
    unsigned int i, nlen, slen, base, unsig;
    unsigned int uint, outlen = 0;
    signed int sint;
    unsigned int factor, digit;
    int phase;
    const char *str = 0;
    char strch, fmtch;
    SEE_char_t *out, *outbuf = 0, *sstr = 0, sstrch;
    const char *fmtstart = 0;

#define OUTPUT(c) do { \
	if (out) *out++ = (c); else outlen++; \
    } while (0)

    /*
     * Phase 0: scan fmt to figure out how much buffer space is needed
     * Phase 1: re-scan fmt to fill out the buffer
     */
    for (phase = 0; phase < 2; phase++) {

	if (phase == 0) {
	    va_copy(ap0, ap);
	    outlen = 0;
	    out = 0;
	    fmtstart = fmt;
	} else {
	    va_copy(ap, ap0);
	    fmt = fmtstart;
	    outbuf = SEE_NEW_STRING_ARRAY(interp, SEE_char_t, outlen);
	    out = outbuf;
	}

	while (*fmt) {
	    int pad_zero = 0;
	    int width = UNDEF;
	    int precis = UNDEF;
	    int left = 0, minus;
	    const char *fmtrestart;

	    if (*fmt != '%' || fmt[1] == 0) {	/* Literal char */
		OUTPUT(*fmt);
		fmt++;
		continue;
	    }

	    fmtrestart = ++fmt;
	    if (*fmt == '%') {			/* "%%" -> output "%" */
		OUTPUT('%');
		fmt++;
		continue;
	    }

	    if (*fmt == '-') {			/* "%-" left-aligned */
		left = 1;
		fmt++;
		if (*fmt == '0')		/* "%-0" is illegal */
		   goto badform;
	    } else if (*fmt == '0') {		/* "%0" zero padding */
		pad_zero = 1;
		fmt++;
	    }

	    if (*fmt == '*') {			/* "%*" -> read later */
		width = STAR;
		fmt++;
	    } else if (isdigit(*fmt)) {
		width = 0;
		while (isdigit(*fmt)) {		/* "%nnn" -> width */
		    width = width * 10 + *fmt - '0';
		    fmt++;
		}
	    }
	    if (*fmt == '.') {
		fmt++;
		if (*fmt == '*') {		/* "%.*" -> read later */
		    precis = STAR;
		    fmt++;
		} else if (isdigit(*fmt)) {
		    precis = 0;
		    while (isdigit(*fmt)) {	/* "%.nnn" -> precision */
			precis = precis * 10 + *fmt - '0';
			fmt++;
		    }
		} else
		    goto badform;		/* require digits after dot */
	    }

	    switch ((fmtch = *fmt++)) {

	    /* Integer formats */
	    case 'u': unsig = 1; base = 10; goto number;  /* unsigned decimal */
	    case 'd': unsig = 0; base = 10; goto number;  /* signed decimal */
	    case 'x': unsig = 1; base = 16; goto number;  /* unsigned hex */
	    case 'p': unsig = 1; base = 16; goto number;  /* pointer hex */
	    number:
		if (precis != UNDEF) goto badform;	/* precision is bad */
		if (width == STAR) {
		    width = va_arg(ap, int);
		    if (width < 0)
			    width = 0;
		}

		/* Convert the argument into an unsigned int with minus flag */
		minus = 0;
		if (unsig) {
		    uint = va_arg(ap, unsigned int);
		} else {
		    sint = va_arg(ap, signed int);
		    if (sint < 0) {
			uint = (unsigned int)-sint;
			minus = 1;
		    } else {
			uint = (unsigned int)sint;
		    }
		}

		/* Figure out the width of the representation */
		nlen = baselen(uint, base);
		if (minus) nlen++;

		/* Grow the width to fit the representation (we don't trunc) */
		if (width < 0 || width < nlen) 
		    width = nlen;
		if (minus) { 
		    nlen--;
		    width--;
		}

		/* Perfom left-hand padding and minus sign insertion */
		if (pad_zero) {
		    if (minus) 
			OUTPUT('-');			/* "-000" */
		    for (i = 0; i < width - nlen; i++)
			OUTPUT('0');
		} else if (!left) {
		    for (i = 0; i < width - nlen; i++)	/* "   -" */
			OUTPUT(' ');
		    if (minus) 
			OUTPUT('-');
		} else 
		    if (minus)
			OUTPUT('-');
		
		/* Perform left-to-right conversion (slow?) */
		factor = 1;
		for (i = 0; i < nlen - 1; i++)
		    factor *= base;
		for (i = 0; i < nlen; i++) {
		    digit = uint / factor;
		    uint -= digit * factor;
		    if (digit < 10)
			OUTPUT('0'+digit);
		    else
			OUTPUT('a'+digit-10);
		    factor /= base;
		}

		/* Perform right-hand padding */
		if (left)
		    for (i = 0; i < width - nlen; i++)
			OUTPUT(' ');
		break;

	    /* String formats */
	    case 'c': 
	    case 'C': 
	    case 's':
	    case 'S':
		if (pad_zero)			/* "%0s" is illegal */
		    goto badform;
		if (width == STAR) {
		    width = va_arg(ap, int);
		    if (width < 0)
			    width = 0;
		}
		if (precis == STAR) {
		    precis = va_arg(ap, int);
		    if (precis < 0)
			    precis = 0;
		}
		if (fmtch == 'c') {
		    strch = va_arg(ap, int);	/* char is promoted to int */
		    str = &strch;
		    slen = 1;
		} else if (fmtch == 'C') {
		    sstrch = va_arg(ap, int);
		    sstr = &sstrch;
		    slen = 1;
		} else if (fmtch == 's') {
		    str = va_arg(ap, const char *);
		    if (!str)
			str = "(NULL)";		/* convert NULL to "(NULL)" */
		    /* Figure out the string's length */
		    slen = 0; 
		    while ((precis == UNDEF || slen < precis) && str[slen]) 
			slen++;
		} else /* fmtch == 'S' */ {
		    struct SEE_string *ss = va_arg(ap, struct SEE_string *);
		    static SEE_char_t snull[] = { '(','N','U','L','L',')' };
		    slen = ss ? ss->length : (sizeof snull / sizeof snull[0]);
		    sstr = ss ? ss->data : snull;
		}
	        if (precis != UNDEF && slen > precis)
		    slen = precis;
		/* Stretch width to fit the string */
		if (width < 0 || width < slen)
		    width = slen;
		/* Figure out padding */
		if (!left) {
		    if (!out)
		        outlen += width - slen;
		    else 
		        for (i = 0; i < width - slen; i++)
			    *out++ = ' ';
		}
		/* Insert the argument string */
	        if (!out)
		    outlen += slen;
		else if (fmtch == 'S' || fmtch == 'C') {
		    memcpy(out, sstr, slen * sizeof *out);
		    out += slen;
		} else
		    for (i = 0; i < slen; i++)
		        *out++ = str[i];
		/* Perform right-hand-side padding */
		if (left) {
		    if (!out)
		    	outlen += width - slen;
		    else
		        for (i = 0; i < width - slen; i++)
			    *out++ = ' ';
		}
		break;

	    /* Unknown formats */
	    default: badform:
		OUTPUT('%');
		fmt = fmtrestart;
	    }
	}
    }
    s->data = outbuf;
    s->length = outlen;
}


void
_SEE_sprintf(interp, s, fmt)
	struct SEE_interpreter *interp;
	struct SEE_string *s;
	const char *fmt;
{
	va_list ap;

	va_start(ap, fmt);
	_SEE_vsprintf(interp, s, fmt, ap);
	va_end(ap);
}
