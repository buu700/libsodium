
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "crypto_core_hsalsa20.h"
#include "crypto_onetimeauth_poly1305.h"
#include "crypto_secretbox.h"
#include "crypto_stream_salsa20.h"
#include "utils.h"

static const unsigned char sigma[16] = {
    'e', 'x', 'p', 'a', 'n', 'd', ' ', '3', '2', '-', 'b', 'y', 't', 'e', ' ', 'k'
};

int
crypto_secretbox_easy(unsigned char *c, const unsigned char *m,
                      unsigned long long mlen, const unsigned char *n,
                      const unsigned char *k)
{
    crypto_onetimeauth_poly1305_state state;
    unsigned char                     block0[64U];
    unsigned char                     subkey[crypto_stream_salsa20_KEYBYTES];
    unsigned long long                i;
    unsigned long long                mlen0;

    if (mlen > SIZE_MAX - crypto_secretbox_MACBYTES) {
        return -1;
    }
    crypto_core_hsalsa20(subkey, n, k, sigma);

    memset(block0, 0U, crypto_secretbox_ZEROBYTES);
    (void) sizeof(int[64U > crypto_secretbox_ZEROBYTES ? 1 : -1]);
    mlen0 = mlen;
    if (mlen0 > 64U - crypto_secretbox_ZEROBYTES) {
        mlen0 = 64U - crypto_secretbox_ZEROBYTES;
    }
    for (i = 0U; i < mlen0; i++) {
        block0[i + crypto_secretbox_ZEROBYTES] = m[i];
    }
    crypto_stream_salsa20_xor(block0, block0,
                              mlen0 + crypto_secretbox_ZEROBYTES,
                              n + 16, subkey);
    crypto_onetimeauth_poly1305_init(&state, block0);

    memcpy(c + crypto_secretbox_MACBYTES,
           block0 + crypto_secretbox_ZEROBYTES, mlen0);
    sodium_memzero(block0, sizeof block0);
    if (mlen > mlen0) {
        crypto_stream_salsa20_xor_ic(c + crypto_secretbox_MACBYTES + mlen0,
                                     m + mlen0, mlen - mlen0,
                                     n + 16, 1U, subkey);
    }
    sodium_memzero(subkey, sizeof subkey);

    crypto_onetimeauth_poly1305_update
        (&state, c + crypto_secretbox_MACBYTES, mlen);
    crypto_onetimeauth_poly1305_final(&state, c);
    sodium_memzero(&state, sizeof state);

    return 0;
}

int
crypto_secretbox_open_easy(unsigned char *m, const unsigned char *c,
                           unsigned long long clen, const unsigned char *n,
                           const unsigned char *k)
{
    unsigned char                     block0[64U];
    unsigned char                     subkey[crypto_stream_salsa20_KEYBYTES];
    unsigned long long                i;
    unsigned long long                mlen0;

    if (clen < crypto_secretbox_MACBYTES) {
        return -1;
    }
    crypto_core_hsalsa20(subkey, n, k, sigma);
    crypto_stream_salsa20(block0, crypto_stream_salsa20_KEYBYTES,
                          n + 16, subkey);
    if (crypto_onetimeauth_poly1305_verify(c, c + crypto_secretbox_MACBYTES,
                                           clen - crypto_secretbox_MACBYTES,
                                           block0) != 0) {
        sodium_memzero(subkey, sizeof subkey);
        return -1;
    }
    mlen0 = clen - crypto_secretbox_MACBYTES;
    if (mlen0 > 64U - crypto_secretbox_ZEROBYTES) {
        mlen0 = 64U - crypto_secretbox_ZEROBYTES;
    }
    memcpy(block0 + crypto_secretbox_ZEROBYTES,
           c + crypto_secretbox_MACBYTES, mlen0);
    crypto_stream_salsa20_xor(block0, block0,
                              crypto_secretbox_ZEROBYTES + mlen0,
                              n + 16, subkey);
    for (i = 0U; i < mlen0; i++) {
        m[i] = block0[i + crypto_secretbox_ZEROBYTES];
    }
    if (clen - crypto_secretbox_MACBYTES > mlen0) {
        crypto_stream_salsa20_xor_ic(m + mlen0,
                                     c + crypto_secretbox_MACBYTES + mlen0,
                                     clen - crypto_secretbox_MACBYTES - mlen0,
                                     n + 16, 1U, subkey);
    }
    sodium_memzero(subkey, sizeof subkey);

    return 0;
}
