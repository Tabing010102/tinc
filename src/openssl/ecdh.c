/*
    ecdh.c -- Diffie-Hellman key exchange handling
    Copyright (C) 2011 Guus Sliepen <guus@tinc-vpn.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "system.h"
#include "utils.h"
#include "xalloc.h"

#include <openssl/err.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>

#include "ecdh.h"
#include "logger.h"

EC_GROUP *secp256k1 = NULL;
EC_GROUP *secp384r1 = NULL;
EC_GROUP *secp521r1 = NULL;

// TODO: proper KDF
static void *kdf(const void *in, size_t inlen, void *out, size_t *outlen) {
	memcpy(out, in, inlen);
	*outlen = inlen;
	return out;
}

bool ecdh_generate_public(ecdh_t *ecdh, void *pubkey) {
	if(!secp521r1)
		secp521r1 = EC_GROUP_new_by_curve_name(NID_secp521r1);

	*ecdh = EC_KEY_new_by_curve_name(NID_secp521r1);
	if(!EC_KEY_generate_key(*ecdh)) {
		logger(LOG_ERR, "Generating EC key failed: %s", ERR_error_string(ERR_get_error(), NULL));
		abort();
	}
	
	const EC_POINT *point = EC_KEY_get0_public_key(*ecdh);
	if(!point) {
		logger(LOG_ERR, "Getting public key failed: %s", ERR_error_string(ERR_get_error(), NULL));
		abort();
	}

	size_t result = EC_POINT_point2oct(secp521r1, point, POINT_CONVERSION_COMPRESSED, pubkey, ECDH_SIZE, NULL);
	if(!result) {
		logger(LOG_ERR, "Converting EC_POINT to binary failed: %s", ERR_error_string(ERR_get_error(), NULL));
		abort();
	}

	return true;
}

bool ecdh_compute_shared(ecdh_t *ecdh, const void *pubkey, void *shared) {
	EC_POINT *point = EC_POINT_new(secp521r1);

	int result = EC_POINT_oct2point(secp521r1, point, pubkey, ECDH_SIZE, NULL);
	if(!point) {
		logger(LOG_ERR, "Converting binary to EC_POINT failed: %s", ERR_error_string(ERR_get_error(), NULL));
		abort();
	}

	result = ECDH_compute_key(shared, ECDH_SIZE, point, *ecdh, kdf);
	EC_POINT_free(point);
	EC_KEY_free(*ecdh);
	*ecdh = NULL;

	if(!result) {
		logger(LOG_ERR, "Computing Elliptic Curve Diffie-Hellman shared key failed: %s", ERR_error_string(ERR_get_error(), NULL));
		return false;
	}

	return true;
}
