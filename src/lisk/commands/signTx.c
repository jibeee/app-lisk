//
// Created by andrea on 09/12/18.
//

#include "signTx.h"

#include "../../ui_utils.h"
#include "../lisk_approval.h"
#include "../lisk_internals.h"
#include "../lisk_utils.h"
// #include "./txs/2_0_transfer.h"
#include "./txs/common_parser.h"
#include "./txs/createMultiSig.h"
#include "./txs/createSignatureTx.h"
#include "./txs/registerDelegateTx.h"
#include "./txs/voteTx.h"
#include "cx.h"

typedef void (*tx_parse_fn)();
typedef void (*tx_end_fn)();

tx_parse_fn tx_parse;
tx_end_fn tx_end;

static void ui_display_sign_tx() {
  // Delegate showing UI to the transaction type handlers
  tx_end();
}

void handleSignTxPacket(commPacket_t *packet, commContext_t *context) {

  uint32_t headerBytesRead = 0;

  // if first packet with signing header
  if (packet->first) {

    PRINTF("PACKET FIRST\n");

    cx_sha256_init(&txContext.txHash);
    txContext.tx_parsing_state = BEGINNING;
    txContext.tx_parsing_group = COMMON;
    txContext.bufferPointer = NULL;

    // IMPORTANT this logic below only works if the first packet contains the needed information (Which it should)
    // Set signing context by parsing bip32paths and other info. function returns number of bytes read (not part of TX)
    headerBytesRead = setReqContextForSign(packet);

    /*
    PRINTF("after setReqContextForSign\n");

    //Read networkIdentifier
    os_memmove(txContext.network_id, packet->data + headerBytesRead, NETWORK_ID_LENGTH);
    PRINTF("after networkIdentifier\n");
    //Read moduleId
    txContext.module_id = lisk_read_u32(packet->data + headerBytesRead + NETWORK_ID_LENGTH, 0, 0);
    PRINTF("after module_id\n");
    //Read assetId
    txContext.asset_id = lisk_read_u32(packet->data + headerBytesRead + NETWORK_ID_LENGTH + 4, 0, 0);
    PRINTF("after asset_id\n");

    PRINTF("txContext.network_id:\n %.*H \n\n", NETWORK_ID_LENGTH, txContext.network_id);
    PRINTF("txContext.module_id:\n %u \n\n", txContext.module_id);
    PRINTF("txContext.asset_id:\n %u \n\n", txContext.asset_id);
    */

  }

    //insert at beginning saveBufferForNextChunk if present
    if(txContext.saveBufferLength > 0) {
      //Shift TX payload (without header) of saveBufferLength bytes on the right
      os_memmove(
          packet->data + headerBytesRead + txContext.saveBufferLength,
          packet->data + headerBytesRead,
          packet->length - headerBytesRead
      );
      //Copy saved buffer in the correct position (beginning of new tx data)
      os_memmove(
          packet->data + headerBytesRead,
          txContext.saveBufferForNextChunk,
          txContext.saveBufferLength
      );
      packet->length += txContext.saveBufferLength;
      txContext.saveBufferLength = 0;
      os_memset(txContext.saveBufferForNextChunk, 0, sizeof(txContext.saveBufferForNextChunk));
    }

    txContext.bufferPointer = packet->data + headerBytesRead;
    txContext.bytesChunkRemaining = packet->length - headerBytesRead;

    BEGIN_TRY {
      TRY {
        switch(txContext.tx_parsing_group) {
        case COMMON:
          parse_group_common();
        case TX_SPECIFIC:
          PRINTF("case TX_SPECIFIC \n\n");
          THROW(INVALID_STATE);
          if(txContext.tx_parsing_group != TX_SPECIFIC) {
            THROW(INVALID_STATE);
          }
          // tx_parse();
        case CHECK_SANITY_BEFORE_SIGN:
          check_sanity_before_sign();
          break;
        default:
          THROW(INVALID_STATE);
        }
      }
      CATCH_OTHER(e) {
        if(e == NEED_NEXT_CHUNK) {
          os_memmove(txContext.saveBufferForNextChunk, txContext.bufferPointer, txContext.bytesChunkRemaining);
          txContext.saveBufferLength = txContext.bytesChunkRemaining;
        } else {
          //Unexpected Error during parsing. Let the client know
          THROW(e);
        }
      }
      FINALLY {}
    }
    END_TRY;

    /*
    // IMPORTANT this logic below only works if the first packet contains the needed information (Which it should)
    // Set signing context from first packet and patches the .data and .length by removing header length
    setSignContext(packet);

    // Reset sha256
    cx_sha256_init(&txHash);

    // fetch transaction type
    transaction.type = packet->data[0];
    uint32_t recIndex = 1  // type
                        + 4 // timestamp
                        + 32 // senderPublicKey
                        ;
    transaction.recipientId = deriveLegacyAddressFromUintArray(packet->data + recIndex, false);
    uint32_t i = 0;

    transaction.amountSatoshi = 0;
    for (i = 0; i < 8; i++) {
      transaction.amountSatoshi |= ((uint64_t )packet->data[recIndex + 8 + i]) << (8*i);
    }

    switch (transaction.type) {
    case TXTYPE_SEND:
      tx_init  = tx_init_send;
      tx_chunk = tx_chunk_send;
      tx_end   = tx_end_send;
      break;
    case TXTYPE_CREATEMULTISIG:
      tx_init  = tx_init_multisig;
      tx_chunk = tx_chunk_multisig;
      tx_end   = tx_end_multisig;
      break;
    case TXTYPE_VOTE:
      tx_init  = tx_init_vote;
      tx_chunk = tx_chunk_vote;
      tx_end   = tx_end_vote;
      break;
    case TXTYPE_REGISTERDELEGATE:
      tx_init  = tx_init_regdel;
      tx_chunk = tx_chunk_regdel;
      tx_end   = tx_end_regdel;
      break;
    case TXTYPE_CREATESIGNATURE:
      tx_init  = tx_init_2ndsig;
      tx_chunk = tx_chunk_2ndsig;
      tx_end   = tx_end_2ndsig;
      break;
    default:
      THROW(0x6a80); // INCORRECT_DATA
    }
    tx_init();
  }

  // Lets skip first bytes pass data starting from asset
  uint8_t assetIndex = ! packet->first ? 0 :  1 // type
                                            + 4 // timestamp
                                            + 32 // senderPublicKey
                                            + 8 // recid
                                            + 8 // amount
                                            ;
  tx_chunk(packet->data + assetIndex, packet->length - assetIndex, packet, &transaction);

  cx_hash(&txHash.header, 0, packet->data, packet->length, NULL, 0);

  */
}

void finalizeSignTx(volatile unsigned int *flags) {
  if(txContext.tx_parsing_group != TX_PARSED || txContext.tx_parsing_state != READY_TO_SIGN)
    THROW(INVALID_STATE);

  // Close sha256 and hash again
  cx_hash_finalize(txContext.digest, DIGEST_LENGTH);

  ui_display_sign_tx();

  // We set the flag after displaying UI as there might be some validation code
  // that throws in the final tx_end call, that should be returned synchronously
  *flags |= IO_ASYNCH_REPLY;
}
