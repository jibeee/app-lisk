#include "dposutils.h"
#ifndef STRUCTS
#define STRUCTS

typedef struct signContext_t {
    cx_ecfp_private_key_t privateKey;
    cx_ecfp_public_key_t publicKey;
    uint16_t msgLength;
    uint8_t msg[500];
    bool hasRequesterPublicKey;
    uint64_t sourceAddress;
    char sourceAddressStr[22 + ADDRESS_SUFFIX_LENGTH + 1];
    bool isTx;
    struct transaction tx;
} signContext_t;

#endif