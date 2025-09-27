// src/espnow_comm.c — conductor is silent (no local audio), performers play

#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "device_config.h"       // device_config_get_role(), ROLE_CONDUCTOR / ROLE_PART_x
#include "espnow_discovery.h"    // espnow_discovery_recv_cb(...)
#include "orchestra.h"           // orchestra_play_song(), orchestra_stop()
#include "espnow_comm.h"         // espnow_msg_t, msg_type_t, prototypes

static const char *TAG = "ESPNOW";

// Broadcast MAC address (FF:FF:FF:FF:FF:FF)
static const uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] =
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// Queue for receiving ESP-NOW control messages
static QueueHandle_t s_espnow_queue = NULL;

// Device ID (optional; use however you like)
static uint8_t s_device_id = 0;

// Clock offset in microseconds: offset = conductor_time - local_time
// Updated by HEARTBEAT messages. Use int64_t for signed offset.
static int64_t s_clock_offset_us = 0;
static TaskHandle_t s_heartbeat_task = NULL;

// ------------------- Callbacks -------------------

static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (mac_addr) {
        ESP_LOGD(TAG, "Send -> %02X:%02X:%02X:%02X:%02X:%02X status=%d",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], status);
    } else {
        ESP_LOGD(TAG, "Send status=%d", status);
    }
}

// Receive callback: hand to discovery first; if it matches our control
// struct size, push to queue for playback handling
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info,
                           const uint8_t *data, int len)
{
    // Basic logging to help trace messages on the radio
    if (recv_info && recv_info->src_addr) {
        ESP_LOGD(TAG, "Recv from %02X:%02X:%02X:%02X:%02X:%02X len=%d",
                 recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
                 recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5], len);
    }

    // Let discovery parse/consume its own frames (role assign, announce, etc.)
    espnow_discovery_recv_cb(recv_info, data, len);

    if (len != (int)sizeof(espnow_msg_t)) {
        // Not one of our control frames — log first byte for diagnostics
        if (len > 0) {
            ESP_LOGD(TAG, "Non-control espnow frame first byte=0x%02X len=%d", data[0], len);
        } else {
            ESP_LOGD(TAG, "Empty espnow frame len=0");
        }
        return;
    }

    espnow_msg_t msg;
    memcpy(&msg, data, sizeof(msg));

    // Ignore messages that we ourselves sent. This prevents the conductor
    // (which broadcasts control messages) from acting on its own broadcasts
    // and ensures only other devices (performers) react.
    if (msg.sender_id == s_device_id) {
        ESP_LOGD(TAG, "Ignoring self-sent message (type=%d sender=%u)", (int)msg.type, (unsigned)msg.sender_id);
        return;
    }

    if (s_espnow_queue && xQueueSend(s_espnow_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Control queue full, dropping message (type=%d sender=%u)", (int)msg.type, (unsigned)msg.sender_id);
    }
}

// ------------------- Worker task -------------------

static void espnow_task(void *pvParameters)
{
    espnow_msg_t msg;

    for (;;) {
        if (xQueueReceive(s_espnow_queue, &msg, portMAX_DELAY) == pdTRUE) {
                device_role_t role = device_config_get_role();
                ESP_LOGI(TAG, "RX: type=%d song=%u sender=%u (role=%d)",
                     (int)msg.type, (unsigned)msg.song_id, (unsigned)msg.sender_id, (int)role);

            switch (msg.type) {
            case MSG_SYNC_START:
                if (role == ROLE_CONDUCTOR) {
                    // Conductor needs to call orchestra_play_song to handle visuals
                    // (even though it usually doesn't play audio)
                    ESP_LOGI(TAG, "Conductor: START received, updating visuals");
                    orchestra_play_song(msg.song_id);
                } else {
                    // Performers schedule playback to conductor timestamp (msg.timestamp is microseconds)
                    int64_t local_now_us = esp_timer_get_time();
                    int64_t conductor_ts_us = (int64_t)msg.timestamp;
                    // Convert conductor_ts to local clock using offset: local_start = conductor_ts - offset
                    int64_t local_start_us = conductor_ts_us - s_clock_offset_us;
                    int64_t wait_us = local_start_us - local_now_us;
                    if (wait_us > 0) {
                        ESP_LOGI(TAG, "Performer: scheduling START song %u in %lld us", (unsigned)msg.song_id, (long long)wait_us);
                        /* Use esp_rom_delay_us instead of ets_delay_us to avoid implicit declaration
                           and use the public ROM API for microsecond delays. */
                        esp_rom_delay_us((uint32_t)wait_us);
                    } else {
                        ESP_LOGI(TAG, "Performer: START song %u immediately (late by %lld us)", (unsigned)msg.song_id, (long long)(-wait_us));
                    }
                    orchestra_play_song(msg.song_id);
                }
                break;

            case MSG_SYNC_STOP:
                if (role == ROLE_CONDUCTOR) {
                    ESP_LOGI(TAG, "Conductor: STOP received, stopping visuals");
                    orchestra_stop();  // Stop visuals even though conductor has no audio
                } else {
                    ESP_LOGI(TAG, "Performer: STOP");
                    orchestra_stop();
                }
                break;

            case MSG_SONG_SELECT:
                // Optional: preload/update UI
                ESP_LOGI(TAG, "Song SELECT %u", (unsigned)msg.song_id);
                break;

            case MSG_HEARTBEAT: {
                // Heartbeat carries the conductor's esp_timer_get_time() (us). Use it to compute offset.
                if (role != ROLE_CONDUCTOR) {
                    uint64_t conductor_now = (uint64_t)msg.timestamp;
                    uint64_t local_now = (uint64_t)esp_timer_get_time();
                    int64_t offset = (int64_t)conductor_now - (int64_t)local_now;
                    // Simple low-pass filter for jitter
                    s_clock_offset_us = (s_clock_offset_us == 0) ? offset : (s_clock_offset_us * 7 + offset) / 8;
                    ESP_LOGI(TAG, "Clock offset updated: %lld us", (long long)s_clock_offset_us);
                }
                break;
            }

            default:
                ESP_LOGW(TAG, "Unknown msg type %d", (int)msg.type);
                break;
            }
        }
    }
}

// ------------------- Public API -------------------

esp_err_t espnow_init(uint8_t id)
{
    // Use the id provided by the caller (caller should pass role-based id).
    // Two-mode behavior when id == 0:
    //  - If the device already has a configured role (device_config_get_role() != ROLE_UNKNOWN),
    //    use that role value as the device id (keeps deterministic role-based mapping).
    //  - If role is UNKNOWN, fall back to deriving a stable performer id from the STA MAC.
    //    We reserve 0 for conductor; derived ids are 1..4.
    if (id == 0) {
        device_role_t cfg_role = device_config_get_role();
        if (cfg_role != ROLE_UNKNOWN) {
            s_device_id = (uint8_t)cfg_role;
            ESP_LOGI(TAG, "espnow_init: id==0, using configured role as device id=%u (role=%d)", (unsigned)s_device_id, (int)cfg_role);
        } else {
            uint8_t mac[6];
            esp_read_mac(mac, ESP_MAC_WIFI_STA);
            // Use last MAC byte to derive a stable but simple id in 1..4
            uint8_t derived = (mac[5] % 4) + 1u;
            s_device_id = derived;
            ESP_LOGI(TAG, "espnow_init: id==0 and role UNKNOWN, derived device id=%u from MAC %02X:%02X:%02X:%02X:%02X:%02X",
                     (unsigned)s_device_id, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    } else {
        s_device_id = id;
    }

    // NVS must be ready before WiFi
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // WiFi (STA), no AP connection needed
    ESP_ERROR_CHECK(esp_netif_init());
    // If the default loop already exists, this will return INVALID_STATE — ignore it.
    if (esp_event_loop_create_default() != ESP_OK) {
        // already created, fine
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // ESP-NOW core
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    // Add broadcast peer
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    peer.ifidx   = WIFI_IF_STA;
    peer.channel = 0;
    peer.encrypt = false;
    esp_err_t add_peer_res = esp_now_add_peer(&peer);
    if (add_peer_res != ESP_OK && add_peer_res != ESP_ERR_ESPNOW_EXIST) {
        ESP_ERROR_CHECK(add_peer_res);
    }

    // Control queue + task
    s_espnow_queue = xQueueCreate(10, sizeof(espnow_msg_t));
    if (!s_espnow_queue) {
        ESP_LOGE(TAG, "Failed to create ESP-NOW control queue");
        return ESP_FAIL;
    }
    xTaskCreate(espnow_task, "espnow_task", 4096, NULL, 10, NULL);

    // If this device is the conductor, start a heartbeat task to broadcast clock
    if (device_config_get_role() == ROLE_CONDUCTOR) {
        // Heartbeat task: send conductor timestamp every 500 ms
        extern void espnow_heartbeat_task(void *pv);
        xTaskCreate(espnow_heartbeat_task, "esp_heartbeat", 2048, NULL, 5, &s_heartbeat_task);
    }

    ESP_LOGI(TAG, "ESP-NOW ready (device_id=%u)", (unsigned)s_device_id);
    return ESP_OK;
}

esp_err_t espnow_broadcast(msg_type_t type, uint8_t song_id)
{
    // Use microsecond timestamps (esp_timer) for higher precision sync.
    uint64_t ts_us = esp_timer_get_time();
    if (type == MSG_SYNC_START) {
        const uint64_t SYNC_LEAD_US = 200000; // 200 ms lead
        ts_us += SYNC_LEAD_US;
    }

    espnow_msg_t msg = {
        .type      = type,
        .song_id   = song_id,
        .timestamp = ts_us,
        .sender_id = s_device_id
    };

    esp_err_t res = esp_now_send(s_broadcast_mac, (const uint8_t *)&msg, sizeof(msg));
    if (res == ESP_OK) {
        ESP_LOGI(TAG, "Broadcasted msg type=%d song=%u (sender=%u) OK", (int)type, (unsigned)song_id, (unsigned)msg.sender_id);
    } else {
        ESP_LOGW(TAG, "Broadcast failed msg type=%d song=%u (err=%d)", (int)type, (unsigned)song_id, (int)res);
    }
    return res;
}

// Heartbeat task implementation (runs only on conductor)
void espnow_heartbeat_task(void *pv)
{
    (void)pv;
    const TickType_t interval = pdMS_TO_TICKS(500);
    while (1) {
        espnow_msg_t hb = {0};
        hb.type = MSG_HEARTBEAT;
        hb.song_id = 0;
        hb.timestamp = (uint64_t)esp_timer_get_time();
        hb.sender_id = s_device_id;
        esp_now_send(s_broadcast_mac, (const uint8_t *)&hb, sizeof(hb));
        vTaskDelay(interval);
    }
}
