#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/twai.h"
#include <string.h>
#include "esp_mac.h"
#include "esp_timer.h"

#define WIFI_SSID "ESP32_CAN_AP"
#define WIFI_PASS ""
#define MAX_STA_CONN 4

static const char *TAG = "WebServer";
static httpd_handle_t server = NULL;
static esp_timer_handle_t countdown_timer = NULL;
static int countdown_value = 30;
static bool countdown_active = false;

static esp_err_t send_can_frames(void) {
    twai_message_t messages[] = {
        {.identifier = 0x742, .data_length_code = 8, .data = {0x03, 0x14, 0xFF, 0x00, 0xB6, 0x01, 0xFF, 0xFF}},
        {.identifier = 0x742, .data_length_code = 8, .data = {0x03, 0x31, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {.identifier = 0x742, .data_length_code = 8, .data = {0x03, 0x31, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00}}
    };

    for (int i = 0; i < 3; i++) {
        esp_err_t result = twai_transmit(&messages[i], pdMS_TO_TICKS(1000));
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send CAN frame %d: %s", i, esp_err_to_name(result));
            return result;
        }
        vTaskDelay(pdMS_TO_TICKS(100));  // Esperar 100ms entre tramas
    }
    
    ESP_LOGI(TAG, "All CAN frames sent successfully");
    return ESP_OK;
}

static void countdown_timer_callback(void* arg) {
    countdown_value--;
    if (countdown_value <= 0) {
        esp_timer_stop(countdown_timer);
        countdown_active = false;
        send_can_frames();
    }
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");

    const char* response_part1 =
        "<!DOCTYPE html><html><head><title>ESP32 CAN Control</title>"
        "<script>"
        "var countdown = 30;"
        "var countdownActive = false;"
        "function updateButton() {"
        "  var btn = document.getElementById('ctrlBtn');"
        "  if (countdownActive) {"
        "    btn.innerHTML = 'Countdown: ' + countdown + 's';"
        "    btn.disabled = true;"
        "  } else {"
        "    btn.innerHTML = 'Start Countdown';"
        "    btn.disabled = false;"
        "  }"
        "}"
        "function startCountdown() {"
        "  countdownActive = true;"
        "  countdown = 30;"
        "  updateButton();";

    const char* response_part2 =
        "  document.getElementById('explanation').innerHTML = '1. Con el motor encencido, ponga el volante/ruedas en el centro.<br><br>"
        "2. Gire el volante a la izquierda hasta el tope.<br><br>"
        "3. Gire el volante a la derecha hasta el tope.<br><br>"
        "4. Vuelva a centrar el volante/ruedas y espere a que finalice la cuenta atrás.<br><br>"
        "5. Una vez finalizada este proceso, apague el coche y vuelva a encenderlo<br><br><br><br><br>';"
        "  var timer = setInterval(function() {"
        "    countdown--;"
        "    updateButton();"
        "    if (countdown <= 0) {"
        "      clearInterval(timer);"
        "      countdownActive = false;"
        "      sendFrames();"
        "    }"
        "  }, 1000);"
        "}"
        "function sendFrames() {"
        "  fetch('/button', {method: 'POST'})"
        "    .then(response => response.text())"
        "    .then(data => {"
        "      document.getElementById('result').innerHTML = data;"
        "    });"
        "}"
        "</script>"
        "</head>"
        "<body>"
        "<h1>ESP32 CAN Control</h1>"
        "<button id='ctrlBtn' onclick='startCountdown()'>Start Countdown</button>"
        "<div id='explanation'></div>"
        "<div id='result'></div>"
        "<script>updateButton();</script>"
        "</body></html>";

    httpd_resp_send_chunk(req, response_part1, strlen(response_part1));
    httpd_resp_send_chunk(req, response_part2, strlen(response_part2));
    httpd_resp_send_chunk(req, NULL, 0); // Sends the final chunk to end the response

    return ESP_OK;
}

static esp_err_t button_post_handler(httpd_req_t *req)
{
    esp_err_t result = send_can_frames();
    if (result == ESP_OK) {
        httpd_resp_sendstr(req, "CAN frames sent successfully");
    } else {
        httpd_resp_send_500(req);
    }
    return ESP_OK;
}

static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
};

static const httpd_uri_t button = {
    .uri       = "/button",
    .method    = HTTP_POST,
    .handler   = button_post_handler,
};

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &button);
    }

    // Inicializar el temporizador de cuenta regresiva
    esp_timer_create_args_t timer_args = {
        .callback = &countdown_timer_callback,
        .name = "countdown_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &countdown_timer));
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = 1,
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_OPEN
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             WIFI_SSID, WIFI_PASS, 1);

    start_webserver();
}