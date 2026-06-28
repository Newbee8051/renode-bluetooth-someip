/*
 * BLE COUNTER — CLIENT (central)
 *
 * The simplest useful BLE client.
 */
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include <string.h>
#include "../../proto.h"

static struct bt_conn *conn_handle;
static struct bt_gatt_discover_params  discover_params;
static struct bt_gatt_subscribe_params subscribe_params;
static struct bt_uuid_128 target_uuid = BT_UUID_INIT_128(BT_UUID_CNT_CHRC_VAL);

/* STEP 5: notifications arrive here. This is the payoff — the counter the
 * server sends every second shows up in this callback. */
static uint8_t notify_cb(struct bt_conn *conn,
                         struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length)
{
    if (!data) {                       /* NULL data == server unsubscribed us */
        printk("client: unsubscribed\n");
        params->value_handle = 0U;
        return BT_GATT_ITER_STOP;
    }
    if (length == sizeof(uint32_t)) {
        uint32_t counter;
        memcpy(&counter, data, sizeof(counter));
        printk("client: RECEIVED counter = %u\n", counter);
    }
    return BT_GATT_ITER_CONTINUE;      /* keep receiving */
}

/* STEP 4: walk the server's GATT table. First find the characteristic, then
 * find its CCC descriptor, then subscribe. Discovery is staged: each callback
 * kicks off the next stage. This is the part people find fiddly — read slowly. */
static uint8_t discover_cb(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *params)
{
    if (!attr) { printk("client: discovery done\n"); return BT_GATT_ITER_STOP; }

    if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        /* Found the counter characteristic. Remember its value handle, then
         * go find the CCC descriptor that sits just after it. */
        subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);
        memcpy(&target_uuid, BT_UUID_GATT_CCC, sizeof(target_uuid));
        discover_params.uuid         = &target_uuid.uuid;
        discover_params.start_handle = attr->handle + 2;
        discover_params.type         = BT_GATT_DISCOVER_DESCRIPTOR;
        bt_gatt_discover(conn, &discover_params);
    } else {
        /* Found the CCC. Subscribe: this writes the CCC on the server, which
         * flips notify_enabled there and starts the counter flowing. */
        subscribe_params.notify     = notify_cb;
        subscribe_params.value      = BT_GATT_CCC_NOTIFY;
        subscribe_params.ccc_handle = attr->handle;
        int err = bt_gatt_subscribe(conn, &subscribe_params);
        printk("client: %s\n",
               (err && err != -EALREADY) ? "subscribe FAILED" : "subscribed!");
        return BT_GATT_ITER_STOP;
    }
    return BT_GATT_ITER_STOP;
}

/* STEP 3: once connected, start discovery for our characteristic UUID. */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) { printk("client: connect failed (0x%02x)\n", err); return; }
    conn_handle = bt_conn_ref(conn);
    printk("client: connected, discovering...\n");

    memcpy(&target_uuid, BT_UUID_CNT_CHRC, sizeof(target_uuid));
    discover_params.uuid         = &target_uuid.uuid;
    discover_params.func         = discover_cb;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type         = BT_GATT_DISCOVER_CHARACTERISTIC;
    bt_gatt_discover(conn, &discover_params);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("client: disconnected (0x%02x), re-scanning\n", reason);
    if (conn_handle) { bt_conn_unref(conn_handle); conn_handle = NULL; }
    bt_le_scan_start(BT_LE_SCAN_PASSIVE, NULL);
}

BT_CONN_CB_DEFINE(conn_cbs) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* STEP 1+2: scan, and when we see a device advertising our name, connect. */
static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                    struct net_buf_simple *adv)
{
    char name[32] = {0};
    /* Minimal advertisement parse: find the complete-name field (type 0x09). */
    for (uint16_t i = 0; i + 1 < adv->len; ) {
        uint8_t len = adv->data[i];
        if (len == 0) break;
        uint8_t adtype = adv->data[i + 1];
        if (adtype == BT_DATA_NAME_COMPLETE) {
            uint8_t n = MIN(len - 1, (int)sizeof(name) - 1);
            memcpy(name, &adv->data[i + 2], n);
        }
        i += len + 1;
    }
    if (strcmp(name, BLE_DEVICE_NAME) == 0) {
        printk("client: found '%s', connecting...\n", name);
        bt_le_scan_stop();
        bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
                          BT_LE_CONN_PARAM_DEFAULT, &conn_handle);
    }
}

int main(void)
{
    int err = bt_enable(NULL);
    if (err) { printk("client: bt_enable failed (%d)\n", err); return 0; }

    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, scan_cb);
    if (err) { printk("client: scan start failed (%d)\n", err); return 0; }
    printk("client: scanning for '%s'\n", BLE_DEVICE_NAME);
    return 0;
}
