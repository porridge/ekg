/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>,
 *                          Robert J. Wo¼ny <speedy@ziew.org>
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

#ifdef HAVE_VOIP

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>
#include <gsm.h>
#include "libgadu.h"
#include "voice.h"
#include "stuff.h"

int voice_fd = -1;
gsm voice_gsm_enc = NULL, voice_gsm_dec = NULL;

/*
 * voice_open()
 *
 * otwiera urz±dzenie d¼wiêkowe, inicjalizuje koder gsm, dopisuje do
 * list przegl±danych deskryptorów.
 *
 * je¶li siê powiedzie 0, je¶li b³±d to -1.
 */
int voice_open()
{
	struct gg_session s;
	int value;
	
	if ((voice_fd = open("/dev/dsp", O_RDWR)) == -1)
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
 * zamyka urz±dzenie audio i koder gsm.
 *
 * brak.
 */
void voice_close()
{
	struct list *l;

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
 * odtwarza próbki gsm.
 *
 *  - buf - bufor z danymi,
 *  - length - d³ugo¶æ bufora,
 *
 * je¶li siê uda³o 0, je¶li nie -1.
 */
int voice_play(char *buf, int length)
{
	gsm_signal output[160];
	char *pos = buf;

	while (pos <= (buf + length - 55)) {
		if (gsm_decode(voice_gsm_dec, pos, output))
			return -1;
		if (write(voice_fd, output, 320) != 320)
			return -1;
		pos += 33;
		if (gsm_decode(voice_gsm_dec, pos, output))
			return -1;
		if (write(voice_fd, output, 320) != 320)
			return -1;
		pos += 32;
	}

	return 0;
}

/*
 * voice_record()
 *
 * nagrywa próbki gsm.
 *
 *  - buf - bufor z danymi,
 *  - length - d³ugo¶æ bufora,
 *
 * je¶li siê uda³o 0, je¶li nie -1.
 */
int voice_record(char *buf, int length)
{
	gsm_signal input[160];
	char *pos = buf;

	while (pos <= (buf + length - 55)) {
		if (read(voice_fd, input, 320) != 320)
			return -1;
		gsm_encode(voice_gsm_enc, input, pos);
		pos += 32;
		if (read(voice_fd, input, 320) != 320)
			return -1;
		gsm_encode(voice_gsm_enc, input, pos);
		pos += 33;
	}

	return 0;
}

#endif /* HAVE_VOIP */

/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: notnil
 * End:
 *
 * vim: shiftwidth=8:
 */


