#include "market_ws/market_ws.hpp"

#include "market_store/market_store.hpp"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "wifi_manager/wifi_manager.hpp"

#include <atomic>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

const char *TAG = "MWS";

/** Bitvavo public WebSocket v2 (geen auth in deze stap). */
constexpr const char *kBitvavoWsUri = "wss://ws.bitvavo.com/v2/";

/** Vaste backoff vóór reconnect; TODO: jitter, exponential backoff, max attempts. */
constexpr uint32_t kReconnectBackoffMs = 3000;
/** IDF websocket_task stack; verhoogd ivm zwaardere callback-/logpaden. */
constexpr uint32_t kWsTaskStack = 8192;

/** Log elke N-de text-frame compact (rx_count, len, snippet). */
constexpr uint32_t kLogSampleEveryN = 25;

constexpr size_t kSampleCap = 120;

/** Max wachten op WiFi GotIp (ms) vóór WS-connect: 120 s. */
constexpr int kMaxWaitGotIpTicks = 480;  // 480 * 250 ms

enum class WorkCmd : uint8_t {
    kConnectNow,
    kReconnectAfterBackoff,
};

QueueHandle_t s_work_q = nullptr;
TaskHandle_t s_worker = nullptr;

std::atomic<bool> s_run_requested{false};
/** Voorkomt dubbele reconnect-jobs bij DISCONNECTED + ERROR kort na elkaar. */
std::atomic<bool> s_reconnect_scheduled{false};
bool s_module_inited = false;

esp_websocket_client_handle_t s_client = nullptr;

market_ws::MarketWsState s_mws_state = market_ws::MarketWsState::kUninitialized;
portMUX_TYPE s_state_spin = portMUX_INITIALIZER_UNLOCKED;

uint64_t s_rx_total = 0;
uint64_t s_data_events = 0;
uint32_t s_error_events = 0;
uint64_t s_reconnect_cycles = 0;
int64_t s_last_rx_us = 0;
uint32_t s_last_payload_len = 0;

void snapshot_stats(uint64_t *rx, uint64_t *data_ev, uint64_t *reconnect, uint32_t *err, uint32_t *last_len,
                    int64_t *last_rx_us) {
    portENTER_CRITICAL(&s_state_spin);
    *rx = s_rx_total;
    *data_ev = s_data_events;
    *reconnect = s_reconnect_cycles;
    *err = s_error_events;
    *last_len = s_last_payload_len;
    *last_rx_us = s_last_rx_us;
    portEXIT_CRITICAL(&s_state_spin);
}

void set_state(market_ws::MarketWsState next) {
    portENTER_CRITICAL(&s_state_spin);
    const market_ws::MarketWsState prev = s_mws_state;
    s_mws_state = next;
    portEXIT_CRITICAL(&s_state_spin);
    if (prev == next) {
        return;
    }

    ESP_LOGD(TAG, "State -> %s", market_ws::state_to_string(next));
}

void teardown_client() {
    if (s_client == nullptr) {
        return;
    }
    esp_websocket_client_stop(s_client);
    esp_websocket_client_destroy(s_client);
    s_client = nullptr;
}

bool post_work(WorkCmd cmd) {
    if (s_work_q == nullptr) {
        return false;
    }
    const WorkCmd c = cmd;
    return xQueueSend(s_work_q, &c, pdMS_TO_TICKS(200)) == pdTRUE;
}

void on_ws_event(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

/** Wacht tot STA een IP heeft (of run stopt / WiFi niet geconfigureerd). False = geen connect proberen. */
bool wait_for_got_ip_before_connect() {
    int n = 0;
    while (s_run_requested.load() && n < kMaxWaitGotIpTicks) {
        const auto st = wifi_manager::get_state();
        if (st == wifi_manager::WifiState::kGotIp) {
            if (n > 0) {
                ESP_LOGI(TAG, "WiFi GotIp terug na ~%d ms wachten", n * 250);
            }
            return true;
        }
        if (st == wifi_manager::WifiState::kNotConfigured) {
            ESP_LOGW(TAG, "WS connect afgebroken: WiFi NotConfigured");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
        ++n;
    }
    if (!s_run_requested.load()) {
        return false;
    }
    ESP_LOGW(TAG, "WS connect timeout: geen GotIp na %d s", (kMaxWaitGotIpTicks * 250) / 1000);
    return false;
}

esp_err_t open_and_start_client() {
    if (!s_run_requested.load()) {
        return ESP_OK;
    }
    if (!wait_for_got_ip_before_connect()) {
        set_state(market_ws::MarketWsState::kIdle);
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_run_requested.load()) {
        return ESP_OK;
    }
    if (s_client != nullptr) {
        ESP_LOGW(TAG, "open_and_start: client bestond al — teardown + opnieuw");
        teardown_client();
    }

    set_state(market_ws::MarketWsState::kConnecting);
    ESP_LOGI(TAG, "connect poging -> %s", kBitvavoWsUri);

    esp_websocket_client_config_t cfg{};
    cfg.uri = kBitvavoWsUri;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.network_timeout_ms = 15000;
    cfg.disable_auto_reconnect = true;
    cfg.task_stack = kWsTaskStack;

    s_client = esp_websocket_client_init(&cfg);
    if (s_client == nullptr) {
        ESP_LOGE(TAG, "esp_websocket_client_init failed");
        set_state(market_ws::MarketWsState::kError);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, &on_ws_event, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_websocket_register_events: %s", esp_err_to_name(err));
        teardown_client();
        set_state(market_ws::MarketWsState::kError);
        return err;
    }

    err = esp_websocket_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_websocket_client_start: %s", esp_err_to_name(err));
        teardown_client();
        set_state(market_ws::MarketWsState::kError);
        {
            bool expected = false;
            if (s_reconnect_scheduled.compare_exchange_strong(expected, true)) {
                post_work(WorkCmd::kReconnectAfterBackoff);
            }
        }
        return err;
    }

    return ESP_OK;
}

void send_ticker_subscribe(esp_websocket_client_handle_t h) {
    static const char payload[] =
        "{\"action\":\"subscribe\",\"channels\":[{\"name\":\"ticker\",\"markets\":[\"BTC-EUR\"]}]}";
    const int sent =
        esp_websocket_client_send_text(h, payload, static_cast<size_t>(strlen(payload)), pdMS_TO_TICKS(5000));
    if (sent < 0) {
        ESP_LOGW(TAG, "subscribe send_text failed");
    } else {
        ESP_LOGI(TAG, "Subscribe sent for BTC-EUR");
    }
}

/** Minimale heuristiek voor transport-“live” (geen domeinparser). */
bool looks_like_ticker_update(const char *p, size_t len) {
    if (p == nullptr || len < 12) {
        return false;
    }
    char buf[256];
    const size_t n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    memcpy(buf, p, n);
    buf[n] = '\0';
    if (strstr(buf, "\"event\":\"subscribed\"") != nullptr || strstr(buf, "\"event\": \"subscribed\"") != nullptr) {
        return false;
    }
    if (strstr(buf, "\"event\":\"ticker\"") != nullptr || strstr(buf, "\"event\": \"ticker\"") != nullptr) {
        return true;
    }
    if (strstr(buf, "\"lastPrice\"") != nullptr) {
        return true;
    }
    if (strstr(buf, "BTC-EUR") != nullptr) {
        return true;
    }
    return false;
}

void log_rx_sample(const char *data, size_t len) {
    char sample[kSampleCap + 4]{};
    size_t n = len < kSampleCap ? len : kSampleCap;
    if (data != nullptr && n > 0) {
        memcpy(sample, data, n);
    }
    sample[n] = '\0';
    for (size_t i = 0; i < n; ++i) {
        if (sample[i] == '\n' || sample[i] == '\r') {
            sample[i] = ' ';
        }
    }
    uint64_t rx = 0;
    uint64_t data_ev = 0;
    uint64_t recon = 0;
    uint32_t err = 0;
    uint32_t last_len = 0;
    int64_t last_rx_us = 0;
    snapshot_stats(&rx, &data_ev, &recon, &err, &last_len, &last_rx_us);
    const market_store::IngestStats ist = market_store::get_ingest_stats();
    ESP_LOGI(TAG,
             "rx_count=%" PRIu64 " data_events=%" PRIu64 " parsed=%" PRIu64 " parse_err=%" PRIu64 " last_len=%u sample=%s",
             rx, data_ev, static_cast<unsigned long long>(ist.ticker_ok),
             static_cast<unsigned long long>(ist.failed), static_cast<unsigned>(static_cast<uint32_t>(len)), sample);
}

void on_ws_event(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    (void)handler_args;
    (void)base;

    auto *data = static_cast<esp_websocket_event_data_t *>(event_data);

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        s_reconnect_scheduled.store(false);
        ESP_LOGI(TAG, "Connected");
        set_state(market_ws::MarketWsState::kConnected);
        if (s_client != nullptr) {
            send_ticker_subscribe(s_client);
        }
        set_state(market_ws::MarketWsState::kSubscribed);
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected");
        set_state(market_ws::MarketWsState::kDisconnected);
        if (s_run_requested.load()) {
            bool expected = false;
            if (s_reconnect_scheduled.compare_exchange_strong(expected, true)) {
                post_work(WorkCmd::kReconnectAfterBackoff);
            }
        }
        break;

    case WEBSOCKET_EVENT_DATA: {
        if (data->op_code != 1 || data->data_ptr == nullptr || data->data_len <= 0) {
            break;
        }

        portENTER_CRITICAL(&s_state_spin);
        const market_ws::MarketWsState before = s_mws_state;
        ++s_rx_total;
        ++s_data_events;
        s_last_rx_us = esp_timer_get_time();
        s_last_payload_len = static_cast<uint32_t>(data->data_len);
        const uint64_t rx = s_rx_total;
        portEXIT_CRITICAL(&s_state_spin);

        if (before == market_ws::MarketWsState::kSubscribed &&
            looks_like_ticker_update(data->data_ptr, static_cast<size_t>(data->data_len))) {
            set_state(market_ws::MarketWsState::kLive);
        }

        market_store::ingest_ws_text(data->data_ptr, static_cast<size_t>(data->data_len), rx);

        if (rx == 1 || (kLogSampleEveryN > 0 && (rx % kLogSampleEveryN) == 0)) {
            log_rx_sample(data->data_ptr, static_cast<size_t>(data->data_len));
        }
        break;
    }

    case WEBSOCKET_EVENT_ERROR: {
        uint32_t ec = 0;
        portENTER_CRITICAL(&s_state_spin);
        ++s_error_events;
        ec = s_error_events;
        portEXIT_CRITICAL(&s_state_spin);
        ESP_LOGW(TAG, "Error (count=%" PRIu32 ")", ec);
        set_state(market_ws::MarketWsState::kError);
        if (s_run_requested.load()) {
            bool expected = false;
            if (s_reconnect_scheduled.compare_exchange_strong(expected, true)) {
                post_work(WorkCmd::kReconnectAfterBackoff);
            }
        }
        break;
    }

    default:
        break;
    }
}

void mws_worker_main(void *arg) {
    (void)arg;
    WorkCmd cmd = WorkCmd::kConnectNow;

    for (;;) {
        if (xQueueReceive(s_work_q, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (!s_run_requested.load() && cmd == WorkCmd::kReconnectAfterBackoff) {
            continue;
        }

        switch (cmd) {
        case WorkCmd::kConnectNow:
            if (!s_run_requested.load()) {
                break;
            }
            teardown_client();
            open_and_start_client();
            break;

        case WorkCmd::kReconnectAfterBackoff:
            if (!s_run_requested.load()) {
                break;
            }
            teardown_client();
            portENTER_CRITICAL(&s_state_spin);
            ++s_reconnect_cycles;
            portEXIT_CRITICAL(&s_state_spin);
            set_state(market_ws::MarketWsState::kReconnectBackoff);
            ESP_LOGI(TAG, "Reconnecting in %lu ms", static_cast<unsigned long>(kReconnectBackoffMs));
            vTaskDelay(pdMS_TO_TICKS(kReconnectBackoffMs));
            if (!s_run_requested.load()) {
                s_reconnect_scheduled.store(false);
                break;
            }
            open_and_start_client();
            s_reconnect_scheduled.store(false);
            break;
        }
    }
}

}  // namespace

namespace market_ws {

esp_err_t init() {
    if (s_module_inited) {
        return ESP_OK;
    }

    s_work_q = xQueueCreate(8, sizeof(WorkCmd));
    if (s_work_q == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    const BaseType_t ok =
        xTaskCreatePinnedToCore(mws_worker_main, "mws_work", 4096, nullptr, 5, &s_worker, tskNO_AFFINITY);
    if (ok != pdPASS) {
        vQueueDelete(s_work_q);
        s_work_q = nullptr;
        return ESP_ERR_NO_MEM;
    }

    s_module_inited = true;
    set_state(MarketWsState::kIdle);
    ESP_LOGI(TAG, "init klaar (Bitvavo WS transport, worker task, IDF 6 esp_websocket_client)");
    return ESP_OK;
}

esp_err_t start() {
    if (!s_module_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    s_reconnect_scheduled.store(false);
    s_run_requested.store(true);
    set_state(MarketWsState::kStarting);
    if (!post_work(WorkCmd::kConnectNow)) {
        ESP_LOGW(TAG, "start: work queue vol");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t stop() {
    s_run_requested.store(false);
    s_reconnect_scheduled.store(false);
    if (s_work_q != nullptr) {
        xQueueReset(s_work_q);
    }
    teardown_client();
    set_state(MarketWsState::kStopping);
    set_state(MarketWsState::kIdle);
    ESP_LOGI(TAG, "stop: transport gestopt");
    return ESP_OK;
}

bool is_live() {
    portENTER_CRITICAL(&s_state_spin);
    const bool live = (s_mws_state == MarketWsState::kLive);
    portEXIT_CRITICAL(&s_state_spin);
    return live;
}

MarketWsState get_state() {
    portENTER_CRITICAL(&s_state_spin);
    const MarketWsState s = s_mws_state;
    portEXIT_CRITICAL(&s_state_spin);
    return s;
}

const char *state_to_string(MarketWsState s) {
    switch (s) {
        case MarketWsState::kUninitialized:
            return "Uninitialized";
        case MarketWsState::kIdle:
            return "Idle";
        case MarketWsState::kStarting:
            return "Starting";
        case MarketWsState::kConnecting:
            return "Connecting";
        case MarketWsState::kConnected:
            return "Connected";
        case MarketWsState::kSubscribed:
            return "Subscribed";
        case MarketWsState::kLive:
            return "Live";
        case MarketWsState::kDisconnected:
            return "Disconnected";
        case MarketWsState::kReconnectBackoff:
            return "ReconnectBackoff";
        case MarketWsState::kStopping:
            return "Stopping";
        case MarketWsState::kError:
            return "Error";
    }
    return "?";
}

const char *state_to_string() {
    return state_to_string(get_state());
}

uint64_t rx_count() {
    portENTER_CRITICAL(&s_state_spin);
    const uint64_t v = s_rx_total;
    portEXIT_CRITICAL(&s_state_spin);
    return v;
}

uint64_t data_events_count() {
    portENTER_CRITICAL(&s_state_spin);
    const uint64_t v = s_data_events;
    portEXIT_CRITICAL(&s_state_spin);
    return v;
}

uint64_t reconnect_count() {
    portENTER_CRITICAL(&s_state_spin);
    const uint64_t v = s_reconnect_cycles;
    portEXIT_CRITICAL(&s_state_spin);
    return v;
}

uint32_t error_count() {
    portENTER_CRITICAL(&s_state_spin);
    const uint32_t v = s_error_events;
    portEXIT_CRITICAL(&s_state_spin);
    return v;
}

uint32_t last_payload_len() {
    portENTER_CRITICAL(&s_state_spin);
    const uint32_t v = s_last_payload_len;
    portEXIT_CRITICAL(&s_state_spin);
    return v;
}

uint32_t idle_since_last_rx_ms() {
    portENTER_CRITICAL(&s_state_spin);
    const int64_t last_rx_us = s_last_rx_us;
    portEXIT_CRITICAL(&s_state_spin);
    if (last_rx_us <= 0) {
        return 0;
    }
    const int64_t now = esp_timer_get_time();
    if (now <= last_rx_us) {
        return 0;
    }
    const uint64_t ms = static_cast<uint64_t>((now - last_rx_us) / 1000);
    return ms > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(ms);
}

}  // namespace market_ws
