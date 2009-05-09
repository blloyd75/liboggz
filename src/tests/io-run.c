/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* Derived from io-seek.c; perform an oggz_run on a stream
   containing a Vorbis b_o_s packet without OGGZ_AUTO enabled
   to check that the oggz_auto.c functions will not cause a
   problem in this mode.  Theora, Speex et al. tests to be added.
*/

#include "config.h"

#include <stdio.h>
#include <string.h>

#include "oggz/oggz.h"

#include "oggz_tests.h"

/* #define DEBUG */

#define DATA_BUF_LEN 1024

static long serialno;
static int read_called = 0;

static int offset_end = 0;
static int my_offset = 0;

/* A nonsensical Vorbis ident header.  For testing purposes
   we may as well try to break something. */
static unsigned char vorbis_bos[] = {1, 'v', 'o', 'r', 'b', 'i', 's',
		     0, 0, 0, 0, /*version*/
		     0, /* channels */
		     0, 0, 0, 0, /* sample rate */
		     0, 0, 0, 0, /* br max */
		     0, 0, 0, 0, /* br nom */
		     0, 0, 0, 0, /* br_min */
		     0,          /* blocksize 0/1 */
		     1           /* Framing bit */
};
		     

static int
hungry (OGGZ * oggz, int empty, void * user_data)
{
  unsigned char buf[1];
  ogg_packet op;
  static int iter = 0;
  static long b_o_s = 1;
  static long e_o_s = 0;

  if (iter > 10) return 1;

  buf[0] = 'a' + iter;

  op.packet = buf;
  op.bytes = 1;
  op.b_o_s = b_o_s;
  op.e_o_s = e_o_s;
  op.granulepos = iter;
  op.packetno = iter;

  if(b_o_s) {
    op.packet = vorbis_bos;
    op.bytes = sizeof vorbis_bos;
  }

  /* Main check */
   if (oggz_write_feed (oggz, &op, serialno, 0, NULL) != 0)
    FAIL ("Oggz write failed");

  iter++;
  b_o_s = 0;
  if (iter == 10) e_o_s = 1;
  
  return 0;
}

static int
read_packet (OGGZ * oggz, ogg_packet * op, long serialno, void * user_data)
{
  static int iter = 0;
  static long b_o_s = 1;
  static long e_o_s = 0;

#ifdef DEBUG
  printf ("%08" PRI_OGGZ_OFF_T "x: serialno %010lu, "
	  "granulepos %" PRId64 ", packetno %" PRId64,
	  oggz_tell (oggz), serialno, op->granulepos, op->packetno);

  if (op->b_o_s) {
    printf (" *** bos");
  }

  if (op->e_o_s) {
    printf (" *** eos");
  }

  printf ("\n");
#endif

  //  if (op->bytes != 1 || op->b_o_s && op->bytes == sizeof vorbis_bos )
  //  FAIL ("Packet too long");

  //if (op->packet[0] != 'a' + iter)
  //  FAIL ("Packet contains incorrect data");

  if ((op->b_o_s == 0) != (b_o_s == 0))
    FAIL ("Packet has incorrect b_o_s");

  if ((op->e_o_s == 0) != (e_o_s == 0))
    FAIL ("Packet has incorrect e_o_s");

  if (op->granulepos != -1 && op->granulepos != iter)
    FAIL ("Packet has incorrect granulepos");

  if (op->packetno != iter)
    FAIL ("Packet has incorrect packetno");

  iter++;
  b_o_s = 0;
  if (iter == 10) {
    e_o_s = 1;
  } else if (iter == 11) {
    iter = 0;
    b_o_s = 1;
    e_o_s = 0;
  }

  return 0;
}

static size_t
my_io_read (void * user_handle, void * buf, size_t n)
{
  unsigned char * data_buf = (unsigned char *)user_handle;
  int len;

  /* Mark that the read IO method was actually used */
  read_called++;

  len = MIN ((int)n, offset_end - my_offset);
  memcpy (buf, &data_buf[my_offset], len);

  my_offset += len;

  return len;
}

static int
my_io_seek (void * user_handle, long offset, int whence)
{
  switch (whence) {
  case SEEK_SET:
    my_offset = offset;
    break;
  case SEEK_CUR:
    my_offset += offset;
    break;
  case SEEK_END:
    my_offset = offset_end + offset;
    break;
  default:
    return -1;
  }

  return 0;
}

static long
my_io_tell (void * user_handle)
{
  return my_offset;
}

int
main (int argc, char * argv[])
{
  OGGZ * reader, * writer;
  unsigned char data_buf[DATA_BUF_LEN];
  long n;

  INFO ("Testing oggz_run works without OGGZ_AUTO");

  writer = oggz_new (OGGZ_WRITE);
  if (writer == NULL)
    FAIL("newly created OGGZ writer == NULL");

  serialno = oggz_serialno_new (writer);

  if (oggz_write_set_hungry_callback (writer, hungry, 1, NULL) == -1)
    FAIL("Could not set hungry callback");

  reader = oggz_new (OGGZ_READ);
  if (reader == NULL)
    FAIL("newly created OGGZ reader == NULL");

  oggz_io_set_read (reader, my_io_read, data_buf);
  oggz_io_set_seek (reader, my_io_seek, data_buf);
  oggz_io_set_tell (reader, my_io_tell, data_buf);

  oggz_set_read_callback (reader, -1, read_packet, NULL);

  n = oggz_write_output (writer, data_buf, DATA_BUF_LEN);

  if (n >= DATA_BUF_LEN)
    FAIL("Too much data generated by writer");

  offset_end = n;

  if ( oggz_run (reader) != 0 )
    FAIL("oggz_run did not complete");

  if (read_called == 0)
    FAIL("Read method ignored");

  if (oggz_seek (reader, 0, SEEK_SET) != 0)
    FAIL("Seek failure");

  read_called = 0;

  oggz_read (reader, n);

  if (read_called == 0)
    FAIL("Read method ignored after seeking");

  if (oggz_close (reader) != 0)
    FAIL("Could not close OGGZ reader");

  if (oggz_close (writer) != 0)
    FAIL("Could not close OGGZ writer");

  exit (0);
}
