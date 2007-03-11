/* $Id$ */

/*
 *  (C) Copyright 2002-2006 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Adam Wysocki <gophi@ekg.chmurka.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"

#include <sys/ioctl.h>
#include <linux/soundcard.h>

#include <fcntl.h>
#include <gsm.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libgadu.h"
#include "voice.h"
#include "stuff.h"
#include "xmalloc.h"

int voice_fd = -1;
gsm voice_gsm_enc = NULL, voice_gsm_dec = NULL;

/*
 * voice_open()
 *
 * otwiera urz�dzenie d�wi�kowe, inicjalizuje koder gsm, dopisuje do
 * list przegl�danych deskryptor�w.
 *
 * 0/-1.
 */
int voice_open()
{
	const char *pathname = "/dev/dsp";
	struct gg_session s;
	gsm_signal tmp;
	int value;
	
	if (voice_fd != -1)
		return -1;

	if (config_audio_device) {
		pathname = config_audio_device;

		if (pathname[0] == '-')
			pathname++;
	}

	if ((voice_fd = open(pathname, O_RDWR)) == -1)
		goto fail;

	value = 8000;
	
	if (ioctl(voice_fd, SNDCTL_DSP_SPEED, &value) == -1)
		goto fail;
	
	value = 16;
	
	if (ioctl(voice_fd, SNDCTL_DSP_SAMPLESIZE, &value) == -1)
		goto fail;

	value = 1;
	
	if (ioctl(voice_fd, SNDCTL_DSP_CHANNELS, &value) == -1)
		goto fail;

	value = AFMT_S16_LE;
	
	if (ioctl(voice_fd, SNDCTL_DSP_SETFMT, &value) == -1)
		goto fail;

	if (read(voice_fd, &tmp, sizeof(tmp)) != sizeof(tmp))
		goto fail;

	if (!(voice_gsm_dec = gsm_create()) || !(voice_gsm_enc = gsm_create()))
		goto fail;

	value = 1;

	gsm_option(voice_gsm_dec, GSM_OPT_FAST, &value);
	gsm_option(voice_gsm_dec, GSM_OPT_WAV49, &value);
	gsm_option(voice_gsm_dec, GSM_OPT_VERBOSE, &value);
	gsm_option(voice_gsm_dec, GSM_OPT_LTP_CUT, &value);
	gsm_option(voice_gsm_enc, GSM_OPT_FAST, &value);
	gsm_option(voice_gsm_enc, GSM_OPT_WAV49, &value);

	s.fd = voice_fd;
	s.check = GG_CHECK_READ;
	s.state = GG_STATE_READING_DATA;
	s.type = GG_SESSION_USER2;
	s.id = 0;
	s.timeout = -1;
	list_add(&watches, &s, sizeof(s));

	return 0;

fail:
	voice_close();
	return -1;
}

/*
 * voice_close()
 *
 * zamyka urz�dzenie audio i koder gsm.
 *
 * brak.
 */
void voice_close()
{
	list_t l;

	for (l = watches; l; l = l->next) {
		struct gg_session *s = l->data;

		if (s->type == GG_SESSION_USER2) {
			list_remove(&watches, s, 1);
			break;
		}
	}
		
	if (voice_fd != -1) {
		close(voice_fd);
		voice_fd = -1;
	} 
	
	if (voice_gsm_dec) {
		gsm_destroy(voice_gsm_dec);
		voice_gsm_dec = NULL;
	}
	
	if (voice_gsm_enc) {
		gsm_destroy(voice_gsm_enc);
		voice_gsm_enc = NULL;
	}
}

/*
 * voice_play()
 *
 * odtwarza pr�bki gsm.
 *
 *  - buf - bufor z danymi,
 *  - length - d�ugo�� bufora,
 *  - null - je�li 1, dekoduje, ale nie odtwarza,
 *
 * 0/-1.
 */
int voice_play(const char *buf, int length, int null)
{
	gsm_signal output[160];
	const char *pos = buf;

	if (length <= 0)
		return 0;

	/* XXX g�upi �orkaraund do rozm�w g�osowych GG 5.0.5 */
	if (length == GG_DCC_VOICE_FRAME_LENGTH_505 && *buf == 0) {
		pos++;
		buf++;
		length--;
	}

	while (pos <= (buf + length - 65)) {
		if (gsm_decode(voice_gsm_dec, (unsigned char *) pos, output))
			return -1;
		if (!null && write(voice_fd, output, 320) != 320)
			return -1;
		pos += 33;
		if (gsm_decode(voice_gsm_dec, (unsigned char *) pos, output))
			return -1;
		if (!null && write(voice_fd, output, 320) != 320)
			return -1;
		pos += 32;
	}

	return 0;
}

/*
 * voice_record()
 *
 * nagrywa pr�bki gsm.
 *
 *  - buf - bufor z danymi,
 *  - length - d�ugo�� bufora,
 *  - null - je�li 1, nie koduje,
 *
 * 0/-1.
 */
int voice_record(char *buf, int length, int null)
{
	gsm_signal input[160];
	const char *pos = buf;

	/* XXX g�upi �orkaraund do rozm�w g�osowych GG 5.0.5 */
	if (length == GG_DCC_VOICE_FRAME_LENGTH_505) {
		*buf = 0;
		pos++;
		buf++;
		length--;
	}
	
	while (pos <= (buf + length - 65)) {
		if (read(voice_fd, input, 320) != 320)
			return -1;
		if (!null)
			gsm_encode(voice_gsm_enc, input, (unsigned char *) pos);
		pos += 32;
		if (read(voice_fd, input, 320) != 320)
			return -1;
		if (!null)
			gsm_encode(voice_gsm_enc, input, (unsigned char *) pos);
		pos += 33;
	}

	return 0;
}

/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: notnil
 * End:
 *
 * vim: shiftwidth=8:
 */
