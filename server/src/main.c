/*
 * BLE COUNTER — SERVER (peripheral)
 *
 * The simplest useful BLE server:
 *   1. defines a GATT service with one NOTIFY characteristic
 *   2. advertises so a client can find it
 *   3. every second, increments a counter and notifies it to the subscriber
 *
 * That's the whole thing. No threads, no buffers — just the BLE essentials.
 */
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include "../../proto.h"

static volatile bool notify_enabled;   /* set when the client subscribes */

/* Called when the client enables/disables notifications (writes the CCC). */
static void ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    printk("server: notifications %s\n", notify_enabled ? "ENABLED" : "disabled");
}

/* The GATT service: one primary service + one notify characteristic + its CCC.
 * The CCC (Client Characteristic Configuration) descriptor is what lets a
 * client turn notifications on — every notify characteristic needs one. */
BT_GATT_SERVICE_DEFINE(counter_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_CNT_SVC),
    BT_GATT_CHARACTERISTIC(BT_UUID_CNT_CHRC,
                           BT_GATT_CHRC_NOTIFY,   /* this char only notifies */
                           BT_GATT_PERM_NONE,
                           NULL, NULL, NULL),
    BT_GATT_CCC(ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* attrs[1] is the characteristic VALUE attribute (0=svc, 1=value, 2=CCC).
 * That's the attribute we notify on. */
#define COUNTER_ATTR (&counter_svc.attrs[1])

/* Advertising data: flags + our device name, so the client can match by name. */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, BLE_DEVICE_NAME, sizeof(BLE_DEVICE_NAME) - 1),
};

static void connected(struct bt_conn *conn, uint8_t err)
{
    printk("server: %s\n", err ? "connection failed" : "connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("server: disconnected (0x%02x), re-advertising\n", reason);
    notify_enabled = false;
    bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
}

BT_CONN_CB_DEFINE(conn_cbs) = {
    .connected = connected,
    .disconnected = disconnected,
};

int main(void)
{
    int err = bt_enable(NULL);
    if (err) { printk("server: bt_enable failed (%d)\n", err); return 0; }

    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) { printk("server: adv start failed (%d)\n", err); return 0; }
    printk("server: advertising as '%s'\n", BLE_DEVICE_NAME);

    uint32_t counter = 0;
    while (1) {
        k_sleep(K_SECONDS(1));
        if (notify_enabled) {
            counter++;
            bt_gatt_notify(NULL, COUNTER_ATTR, &counter, sizeof(counter));
            printk("server: notified counter = %u\n", counter);
        }
    }
    return 0;
}
