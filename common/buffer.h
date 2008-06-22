/*
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The names of its contributors may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS
 *  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED.  IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
/*
 *  Copyright © 2006,2007,2008 Pierre Habouzit
 */

#ifndef MC_BASE_BUFFER_H
#define MC_BASE_BUFFER_H

#include "common/util.h"

typedef struct buffer_t {
    char *s;
    int len, size;
    unsigned alloced: 1;
    unsigned offs   :31;
} buffer_t;

extern char buffer_slop[1];


#define BUFFER_INIT  (buffer_t){ .s = buffer_slop, .size = 1 }

#define buffer_inita(b, sz)                                                 \
    ({ int __sz = (sz); assert (__sz < (64 << 10));                         \
       buffer_init_buf((b), alloca(__sz), __sz); })

static inline buffer_t *buffer_init(buffer_t *buf) {
    *buf = BUFFER_INIT;
    return buf;
}
static inline void buffer_init_buf(buffer_t *b, void *buf, int size) {
    *b = (buffer_t){ .s = buf, .size = size };
    b->s[0] = '\0';
}
static inline void buffer_wipe(buffer_t *buf) {
    if (buf->alloced)
        free(buf->s - buf->offs);
    buffer_init(buf);
}
static inline buffer_t *buffer_new(void) {
    return buffer_init(p_new(buffer_t, 1));
}
static inline void buffer_delete(buffer_t **buf) {
    if (*buf) {
        buffer_wipe(*buf);
        p_delete(buf);
    }
}

char *buffer_detach(buffer_t *buf);
void buffer_ensure(buffer_t *buf, int len);
static inline void buffer_grow(buffer_t *buf, int extra)
{
    assert (extra >= 0);
    if (buf->len + extra > buf->size) {
        buffer_ensure(buf, buf->len + extra);
    }
}


static inline void
buffer_splice(buffer_t *buf, int pos, int len, const void *data, int dlen)
{
    assert (pos >= 0 && len >= 0 && dlen >= 0);

    if (unlikely(pos > buf->len))
        pos = buf->len;
    if (unlikely(len > buf->len - pos))
        len = buf->len - pos;
    if (pos == 0 && len + buf->offs >= dlen) {
        buf->offs += len - dlen;
        buf->s    += len - dlen;
        buf->size -= len - dlen;
        buf->len  -= len - dlen;
        len = dlen;
    } else
    if (len != dlen) {
        buffer_ensure(buf, buf->len + dlen - len);
        memmove(buf->s + pos + dlen, buf->s + pos + len, buf->len - pos - len);
        buf->len += dlen - len;
        buf->s[buf->len] = '\0';
    }
    memcpy(buf->s + pos, data, dlen);
}


static inline void buffer_add(buffer_t *buf, void *data, int len) {
    buffer_splice(buf, buf->len, 0, data, len);
}
static inline void buffer_adds(buffer_t *buf, const char *s) {
    buffer_splice(buf, buf->len, 0, s, strlen(s));
}
static inline void buffer_addc(buffer_t *buf, int c) {
    buffer_grow(buf, 1);
    buf->s[buf->len++] = c;
    buf->s[buf->len]   = '\0';
}

void buffer_addvf(buffer_t *buf, const char *fmt, va_list)
    __attribute__((format(printf, 2, 0)));

void buffer_addf(buffer_t *buf, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif /* MC_BASE_BUFFER_H */