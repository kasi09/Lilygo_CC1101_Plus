#include "nfc_frame.h"
namespace {
constexpr uint8_t Ack[] = {1,0,0,255,0,255,0};
constexpr uint8_t NoTarget[] = {1,0,0,255,3,253,213,75,0,224,0};
constexpr uint8_t Uid4[] = {1,0,0,255,12,244,213,75,1,1,0,4,0,4,1,2,3,4,204,0};
constexpr uint8_t BadUid[] = {1,0,0,255,12,244,213,75,1,1,0,4,0,255,1,2,3,4,209,0};
constexpr uint8_t BadChecksum[] = {1,0,0,255,12,244,213,75,1,1,0,4,0,4,1,2,3,4,203,0};
static_assert(NfcFrame::ack(Ack, sizeof(Ack)), "PN532 ACK frame");
static_assert(!NfcFrame::ack(Ack, 6), "short ACK rejected");
static_assert(NfcFrame::uidLength(NoTarget, sizeof(NoTarget)) == 0, "no tag is not an error");
static_assert(NfcFrame::uidLength(Uid4, sizeof(Uid4)) == 4, "four-byte UID");
static_assert(NfcFrame::uidLength(Uid4, sizeof(Uid4)-1) == -1, "truncated response rejected");
static_assert(NfcFrame::uidLength(BadUid, sizeof(BadUid)) == -1, "UID copy remains bounded");
static_assert(NfcFrame::uidLength(BadChecksum, sizeof(BadChecksum)) == -1, "corrupt frame rejected");
}
