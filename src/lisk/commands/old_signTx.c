//
// Created by andrea on 09/12/18.
//

#include "old_signTx.h"

#include "../../ui_utils.h"
#include "../lisk_approval.h"
#include "../lisk_internals.h"
#include "../lisk_utils.h"
#include "./txs/old_sendTx.h"
#include "./txs/createMultiSig.h"
#include "./txs/createSignatureTx.h"
#include "./txs/registerDelegateTx.h"
#include "./txs/voteTx.h"
#include "cx.h"

#define TXTYPE_SEND 0
#define TXTYPE_CREATESIGNATURE 1
#define TXTYPE_REGISTERDELEGATE 2
#define TXTYPE_VOTE 3
#define TXTYPE_CREATEMULTISIG 4

typedef void (*old_tx_init_fn)();
typedef void (*old_tx_chunk_fn)(uint8_t * data, uint8_t length, commPacket_t *sourcePacket, transaction_t *tx);
typedef void (*old_tx_end_fn)(transaction_t *tx);

old_tx_init_fn old_tx_init;
old_tx_chunk_fn old_tx_chunk;
old_tx_end_fn old_tx_end;

static cx_sha256_t txHash;
transaction_t transaction;

static void old_ui_display_sign_tx() {
  // Delegate showing UI to the transaction type handlers
  old_tx_end(&transaction);
}

void old_handleSignTxPacket(commPacket_t *packet, commContext_t *context) {
  // if first packet with signing header
  if (packet->first) {
    // IMPORTANT this logic below only works if the first packet contains the needed information (Which it should)
    // Set signing context from first packet and patches the .data and .length by removing header length
    setSignContext(packet);

    // Reset sha256
    cx_sha256_init(&txHash);

    // fetch transaction type
    transaction.type = packet->data[0];
    uint32_t recIndex = 1 /*type*/
                        + 4 /*timestamp*/
                        + 32 /*senderPublicKey */;
    transaction.recipientId = deriveLegacyAddressFromUintArray(packet->data + recIndex, false);
    uint32_t i = 0;

    transaction.amountSatoshi = 0;
    for (i = 0; i < 8; i++) {
      transaction.amountSatoshi |= ((uint64_t )packet->data[recIndex + 8 + i]) << (8*i);
    }

    switch (transaction.type) {
    case TXTYPE_SEND:
      old_tx_init  = tx_init_send;
      old_tx_chunk = tx_chunk_send;
      old_tx_end   = tx_end_send;
      break;
    case TXTYPE_CREATEMULTISIG:
      old_tx_init  = tx_init_multisig;
      old_tx_chunk = tx_chunk_multisig;
      old_tx_end   = tx_end_multisig;
      break;
    case TXTYPE_VOTE:
      old_tx_init  = tx_init_vote;
      old_tx_chunk = tx_chunk_vote;
      old_tx_end   = tx_end_vote;
      break;
    case TXTYPE_REGISTERDELEGATE:
      old_tx_init  = tx_init_regdel;
      old_tx_chunk = tx_chunk_regdel;
      old_tx_end   = tx_end_regdel;
      break;
    case TXTYPE_CREATESIGNATURE:
      old_tx_init  = tx_init_2ndsig;
      old_tx_chunk = tx_chunk_2ndsig;
      old_tx_end   = tx_end_2ndsig;
      break;
    default:
      THROW(0x6a80); // INCORRECT_DATA
    }
    old_tx_init();
  }

  // Lets skip first bytes pass data starting from asset
  uint8_t assetIndex = ! packet->first ? 0 :  1 /*type*/
                                            + 4 /*timestamp*/
                                            + 32 /*senderPublicKey */
                                            + 8 /*recid */
                                            + 8 /*amount */;
  old_tx_chunk(packet->data + assetIndex, packet->length - assetIndex, packet, &transaction);

  cx_hash(&txHash.header, 0, packet->data, packet->length, NULL, 0);
}

void old_finalizeSignTx(volatile unsigned int *flags) {
  // Get the digest for the block
  uint8_t finalHash[sizeof(signContext.digest)];
  cx_hash(&txHash.header, CX_LAST, NULL, 0, finalHash, sizeof(finalHash));
  os_memmove(signContext.digest, finalHash, sizeof(finalHash));

  old_ui_display_sign_tx();

  // We set the flag after displaying UI as there might be some validation code
  // that throws in the final tx_end call, that should be returned synchronously
  *flags |= IO_ASYNCH_REPLY;
}
