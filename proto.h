/*
 * proto.h — the shared BLE contract. Both nodes include this so their
 * service/characteristic UUIDs match. This is the ONE thing that must agree
 * between a BLE server and client: they have to be talking about the same
 * service + characteristic.
 *
 * (Custom 128-bit UUIDs. A 16-bit UUID like 0x180D is reserved for official
 *  SIG-defined services; custom services use a random 128-bit UUID.)
 */
#ifndef PROTO_H
#define PROTO_H

#include <zephyr/bluetooth/uuid.h>

/* Service = "the mailbox", characteristic = "the slot you read/get notified on". */
#define BT_UUID_CNT_SVC_VAL \
    BT_UUID_128_ENCODE(0x12340001, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_CNT_CHRC_VAL \
    BT_UUID_128_ENCODE(0x12340002, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)

#define BT_UUID_CNT_SVC   BT_UUID_DECLARE_128(BT_UUID_CNT_SVC_VAL)
#define BT_UUID_CNT_CHRC  BT_UUID_DECLARE_128(BT_UUID_CNT_CHRC_VAL)

#define BLE_DEVICE_NAME   "ble_counter"   /* client scans for this name */

#endif /* PROTO_H */
