/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp_log.h"
#include "nvs_flash.h"
#include <stdbool.h>
#include "esp_peripheral.h"
#include "modlog/modlog.h"

/* BLE */
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#ifndef BLE_H
#define BLE_H
#endif

/* 16 Bit SPP Service UUID */
#define BLE_SVC_SPP_UUID16                                  0xABF0

/* 16 Bit SPP Service Characteristic UUID */
#define BLE_SVC_SPP_CHR_UUID16                              0xABF1

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

static int ble_spp_server_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t own_addr_type;
int gatt_svr_register(void);
static bool conn_handle_subs[CONFIG_BT_NIMBLE_MAX_CONNECTIONS + 1]; // set max conn to 1
static uint16_t ble_spp_svc_gatt_read_val_handle;

// Yes, we can have multiple connections, but let's just assume for now that we
// have only one. This lets us avoid a for() loop through the conn_handle_subs[]
// array every time we send a notification. -1 when not valid, >= 0 when valid.
int conn_num = -1;

void ble_store_config_init(void);

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void ble_spp_server_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(BLE_SVC_SPP_UUID16)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_spp_server_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * ble_spp_server uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unused by
 *                                  ble_spp_server.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
ble_spp_server_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        MODLOG_DFLT(INFO, "\n================\nconnection %s; status=%d ",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            //ble_spp_server_print_conn_desc(&desc);

            conn_num = event->connect.conn_handle; // Save our connection number.
        }
        if (event->connect.status != 0 || CONFIG_BT_NIMBLE_MAX_CONNECTIONS > 1) {
            /* Connection failed or if multiple connection allowed; resume advertising. */
            ble_spp_server_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);

        conn_handle_subs[event->disconnect.conn.conn_handle] = false;
        conn_num = -1;

        /* Connection terminated; resume advertising. */
        ble_spp_server_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        MODLOG_DFLT(INFO, "connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "advertise complete; reason=%d",
                    event->adv_complete.reason);
        ble_spp_server_advertise();
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
                    "reason=%d prevn=%d curn=%d previ=%d curi=%d",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle,
                    event->subscribe.reason,
                    event->subscribe.prev_notify,
                    event->subscribe.cur_notify,
                    event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);
        conn_handle_subs[event->subscribe.conn_handle] = true;
        return 0;

    default:
        return 0;
    }
}

static void ble_spp_server_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Printing ADDR */
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    MODLOG_DFLT(INFO, "Device Address: ");
    print_addr(addr_val);
    /* Begin advertising. */
    ble_spp_server_advertise();
}

// Simple BLE_printf(), to send string as notification to PC.
static char pr_strbuf[1000];
int BLE_printf(char *format, ...)
{
    if(conn_num < 0) return -1;

    va_list args;
    va_start (args, format);
    vsprintf (pr_strbuf,format, args);

    // Now send the composed string via BLE.
    struct os_mbuf *txom;
    txom = ble_hs_mbuf_from_flat(pr_strbuf, strlen(pr_strbuf));
    int rc = ble_gatts_notify_custom(conn_num, ble_spp_svc_gatt_read_val_handle, txom);

    va_end (args);

    return rc;
}

// Callback for when we receive BLE data.
static void (*__rcv_callback)(char *str) = 0;

void BLE_set_rcv_callback(void (*f)(char *))
{
    __rcv_callback = f;
}

// ==============================================================
//
// Callback to handle BLE events, i.e. "write" operations to us. We don't use
// the READ callback, and hopefully the DEFAULT callback is never used.
//
static char rcvstr[900];
static int  ble_svc_gatt_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        MODLOG_DFLT(INFO, "Callback for read");
        break;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if(!__rcv_callback) break;

        // Extract BLE data buffer as a string.
        int len = ctxt->om->om_len;
        strncpy(rcvstr, (char *)ctxt->om->om_data, len);
        rcvstr[len] = 0;

        // If user has set up a callback, then call it.
        __rcv_callback(rcvstr);
        break;

    default:
        MODLOG_DFLT(INFO, "\nDefault Callback");
        break;
    }
    return 0;
}

/* Define new custom service */
static const struct ble_gatt_svc_def new_ble_svc_gatt_defs[] = {
    {
        /*** Service: SPP */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_SPP_UUID16),
        .characteristics = (struct ble_gatt_chr_def[])
        { {
                /* Support SPP service */
                .uuid = BLE_UUID16_DECLARE(BLE_SVC_SPP_CHR_UUID16),
                .access_cb = ble_svc_gatt_handler,
                .val_handle = &ble_spp_svc_gatt_read_val_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_WRITE_NO_RSP,
            }, {
                0, /* No more characteristics */
            }
        },
    },
    {
        0, /* No more services. */
    },
};

int gatt_svr_init(void)
{
    int rc = 0;
    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(new_ble_svc_gatt_defs);

    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(new_ble_svc_gatt_defs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}


// This required function is passed to nimble_port_freertos_init() in main.
void ble_spp_server_host_task(void *param)
{    
    nimble_port_run();  // This will return only when nimble_port_stop() is executed
    nimble_port_freertos_deinit();
}

void myfunc(int);

//void app_main(void)
void BLE_init(char *NameStr)
{
    int rc;

    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        MODLOG_DFLT(ERROR, "Failed to init nimble %d \n", ret);
        return;
    }

    /* Initialize connection_handle array */
    for (int i = 0; i <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        conn_handle_subs[i] = false;
    }

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.sync_cb = ble_spp_server_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = 3;//CONFIG_EXAMPLE_IO_TYPE;

    /* Register custom service */
    rc = gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set(NameStr);
    assert(rc == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(ble_spp_server_host_task);
}
