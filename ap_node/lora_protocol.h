#pragma once

struct LoRaStats {
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t ack_rx;
    uint32_t ack_tx;
    uint32_t duplicate_packets;
    uint32_t decrypt_errors;
    uint32_t json_errors;
    uint32_t fragmented_rx;
    uint32_t fragmented_tx;
    uint32_t retransmissions;
    uint32_t dropped_packets;
    uint32_t radio_read_errors;
    uint32_t oversized_packets;
};

extern volatile LoRaStats lora_stats;

#include "aes_helper.h"
#include <RadioLib.h> // RadioLib osztályok elérése
#include <deque>
#include <map>
#include <vector>
#include <string>
#include <string_view>
#include <cstdint>
#include <charconv> // A leggyorsabb, heap-mentes string->szám konverter C++17-ben

#define PACKET_TYPE_DATA       1
#define PACKET_TYPE_ACK        2
#define PACKET_TYPE_DISCOVERY  3

#define FLAG_ACK        0x02
#define FLAG_FRAGMENT   0x04

#define MAX_PAYLOAD_SIZE 180
#define MAX_RETRIES      4

#define MAX_FRAGMENTS 64
#define FRAG_MAX_SIZE 128 // Állítsd be a LoRa MTU-d darabka méretére (pl. 100 vagy 128)

extern volatile bool radioActionInterrupt;

template <typename T, size_t MaxSize>
class StaticRingBuffer {
private:
    T data[MaxSize];
    size_t head = 0;
    size_t tail = 0;
    size_t count = 0;

public:
    inline bool push_back(const T& item) {
        if (count >= MaxSize) return false; // A puffer megtelt, elvetjük a csomagot
        data[tail] = item;
        tail = (tail + 1) % MaxSize;
        count++;
        return true;
    }

    inline bool pop_front(T& out_item) {
        if (count == 0) return false; // A puffer üres
        out_item = data[head];
        head = (head + 1) % MaxSize;
        count--;
        return true;
    }
    inline bool peek_front(T& out_item) const {
        if (count == 0) return false;
        out_item = data[head];
        return true;
    }
    inline bool update_front(const T& updated_item) {
        if (count == 0) return false;
        data[head] = updated_item;
        return true;
    }
    inline T& operator[](size_t index) {
        // Kiszámítja a valós memóriacímet a körkörös pufferben az index alapján
        return data[(head + index) % MaxSize];
    }
    inline void erase_at(size_t index) {
        if (index >= count) return; // Hibás index esetén nem csinálunk semmit
        for (size_t i = index; i < count - 1; i++) {
            size_t current_idx = (head + i) % MaxSize;
            size_t next_idx = (head + i + 1) % MaxSize;
            data[current_idx] = data[next_idx];
        }
        if (tail == 0) tail = MaxSize - 1;
        else tail--;
        count--;
    }
    inline size_t size() const { return count; }
    inline bool empty() const { return count == 0; }
    inline void clear() { head = 0; tail = 0; count = 0; }
};

struct PendingPacket {
    uint32_t counter;
    uint8_t retries;
    uint32_t deadline;
    uint8_t data[256]; // Fix méretű puffer std::vector vagy std::string helyett!
    size_t data_len;   // A ténylegesen felhasznált bájtok száma
};

struct FragmentBuffer {
    uint32_t boot_id = 0;
    uint32_t msg_id = 0;
    uint32_t created = 0;
    uint16_t total = 0;
    uint16_t received = 0;
    
    // Fix kétdimenziós tömb: 64 darab, egyenként FRAG_MAX_SIZE méretű slot
    uint8_t data[MAX_FRAGMENTS][FRAG_MAX_SIZE];
    uint16_t lens[MAX_FRAGMENTS]; // Elmentjük, melyik darabka hány bájtos
    bool present[MAX_FRAGMENTS];  // Jelzi, hogy az adott indexű darab megérkezett-e már
};

struct NodeFragContext {
    uint8_t active_node_id = 0; 
    FragmentBuffer buffers[2]; // Node-onként 2 párhuzamos üzenet slot
};

inline static NodeFragContext fragment_ctx[4]; 

struct NodeTxQueue {
    uint8_t node_id = 0; // 0 = üres slot, egyébként a LoRa Node azonosítója
    StaticRingBuffer<PendingPacket, 4> queue;
};

// 256 helyett maximum 32 különböző node-nak engedünk egy időben aktív adási sort fenntartani!
// Ez 276 KB helyet mindössze ~34 KB RAM-ot fog fogyasztani (Azonnali 242 KB RAM megtakarítás!)
inline static NodeTxQueue tx_queue[32];

static portMUX_TYPE tx_mux = portMUX_INITIALIZER_UNLOCKED;
extern portMUX_TYPE frag_mux;

struct FragmentContext {
    std::map<uint32_t, FragmentBuffer> buffers;
};

// ============================================================
// ENQUEUE
// ============================================================
inline void enqueue_packet(
    uint8_t node,
    uint8_t type,
    uint8_t flags,
    uint32_t counter,
    std::string_view payload
) {
    PendingPacket pkt;
    pkt.counter = counter;
    pkt.retries = 0;
    pkt.deadline = millis() + 2000;

    // Titkosítás közvetlenül a PendingPacket fix struktúrájába (0 heap)
    pkt.data_len = encrypt_packet(
        node, type, flags, counter, 0, 0, 1, 
        payload, 
        pkt.data, 
        sizeof(pkt.data)
    );
    portENTER_CRITICAL(&tx_mux);
    bool success = false;
    int free_idx = -1;
    int target_idx = -1;
    // Megkeressük, van-e már aktív sora ennek a node-nak a 32-ből
    for (int i = 0; i < 32; i++) {
        if (tx_queue[i].node_id == node) {
            target_idx = i;
            break;
        }
        if (tx_queue[i].node_id == 0 && free_idx == -1) {
            free_idx = i;
        }
    }
    // Ha még nincs, de van szabad hely, lefoglaljuk neki
    if (target_idx == -1 && free_idx != -1) {
        tx_queue[free_idx].node_id = node;
        tx_queue[free_idx].queue.clear();
        target_idx = free_idx;
    }
    // Ha találtunk/kaptunk slotot, betesszük a csomagot a Ring Bufferbe
    if (target_idx != -1) {
        success = tx_queue[target_idx].queue.push_back(pkt);
    }
    portEXIT_CRITICAL(&tx_mux);
    #if DEBUG
    if (!success) {
        // Serial.println("HIBA: A TX sor megtelt a node-hoz, a csomag elvetve!");
    }
    #endif
}

// ============================================================
// FRAGMENTATION
// ============================================================
inline void enqueue_fragmented_packet(
    uint8_t node,
    uint32_t counter,
    std::string_view payload // std::string HELYETT string_view
) {
    const size_t FRAG_SIZE = 100;
    uint16_t total = (payload.size() + FRAG_SIZE - 1) / FRAG_SIZE;
    uint32_t msg_id = ((uint32_t)node << 24) | counter;

    // Egyetlen fix méretű puffer a stack-en a darabka összeállításához.
    // 32 bájt a fejlécnek (msg_id|i|total|) + 100 bájt a fragmensnek = 140 elegendő.
    //char frag_buffer[140]; // berakva a 200-as sorba...
    for(uint16_t i = 0; i < total; i++) {
        size_t start = i * FRAG_SIZE;
        size_t len = std::min(FRAG_SIZE, payload.size() - start);
        // std::string_view-val "kivágjuk" a darabot másolás nélkül
        std::string_view frag = payload.substr(start, len);
        char frag_buffer[140];
        // std::to_string és out += HEAVY HEAP ALLOKÁCIÓK HELYETT:
        // Először beírjuk a fejlécet, és elmentjük, hány karaktert írtunk ki (header_len)
        int header_len = snprintf(frag_buffer, sizeof(frag_buffer), "%u|%u|%u|", msg_id, i, total);
        if (header_len > 0 && (header_len + len) < sizeof(frag_buffer)) {
            // Közvetlenül a fejléc mögé másoljuk a fragmens bájtolt tartalmát (heap nélkül)
            memcpy(frag_buffer + header_len, frag.data(), len);
            // Létrehozzuk a kész csomag ablakát
            std::string_view out_view(frag_buffer, header_len + len);
            enqueue_packet(
                node,
                PACKET_TYPE_DATA,
                FLAG_FRAGMENT,
                counter + i,
                out_view
            );
        }
    }
}

// ============================================================
// ACK RECEIVED
// ============================================================
inline void tx_ack_received(uint8_t node, uint32_t counter) {
    portENTER_CRITICAL(&tx_mux);

    for (int i = 0; i < 32; i++) {
        if (tx_queue[i].node_id == node) {
            auto &q = tx_queue[i].queue;
            size_t current_size = q.size();
            for (size_t j = 0; j < current_size; j++) {
                if (q[j].counter == counter) {
                    q.erase_at(j);
                    
                    // Ha a törlés után kiürült a sor, felszabadítjuk a slotot
                    if (q.empty()) {
                        tx_queue[i].node_id = 0;
                    }
                    break;
                }
            }
            break;
        }
    }
    portEXIT_CRITICAL(&tx_mux);
}

// ============================================================
// TX PROCESSOR (RadioLib-re portolt és optimalizált változat)
// ============================================================
inline void process_tx_queues(SX1262 &radio) {
    uint32_t now = millis();
    static uint32_t last_send_node[256] = {0}; // Ez maradhat 256 az időzítések miatt
    // 256 helyett csak a 32 aktív slotot nézzük át
    for(int i = 0; i < 32; i++) {
        if (tx_queue[i].node_id == 0 || tx_queue[i].queue.empty()) {
            continue; 
        }
        uint8_t node = tx_queue[i].node_id;
        PendingPacket pkt;
        bool has_packet = false;
        portENTER_CRITICAL(&tx_mux);
        has_packet = tx_queue[i].queue.peek_front(pkt);
        portEXIT_CRITICAL(&tx_mux);
        if (!has_packet) continue;
        // --- KÜLDÉSI LOGIKA (Változatlan, csak a tx_queue[node] helyett tx_queue[i].queue-t használunk!) ---
        if(pkt.retries == 0) {
            if ((uint32_t)(now - last_send_node[node]) < 50) continue;
            int state = radio.startTransmit(pkt.data, pkt.data_len);
            if (state == RADIOLIB_ERR_NONE) {
                last_send_node[node] = now;
                pkt.retries++;
                pkt.deadline = now + 2000;
                portENTER_CRITICAL(&tx_mux);
                tx_queue[i].queue.update_front(pkt);
                portEXIT_CRITICAL(&tx_mux);
                break; 
            }
            continue;
        }
        if((int32_t)(now - pkt.deadline) >= 0) {
            if(pkt.retries >= MAX_RETRIES) {
                lora_stats.dropped_packets++;
                PendingPacket dropped;
                portENTER_CRITICAL(&tx_mux);
                tx_queue[i].queue.pop_front(dropped);
                // Ha a sor teljesen kiürült, felszabadítjuk a globális slotot
                if (tx_queue[i].queue.empty()) {
                    tx_queue[i].node_id = 0;
                }
                portEXIT_CRITICAL(&tx_mux);
                continue;
            }
            if ((uint32_t)(now - last_send_node[node]) < 50) continue;
            int state = radio.startTransmit(pkt.data, pkt.data_len);
            if (state == RADIOLIB_ERR_NONE) {
                lora_stats.retransmissions++;
                last_send_node[node] = now;
                pkt.retries++;
                pkt.deadline = now + 2000 + (esp_random() % 500);
                portENTER_CRITICAL(&tx_mux);
                tx_queue[i].queue.update_front(pkt);
                portEXIT_CRITICAL(&tx_mux);
                break;
            }
        }
    }
}

// ============================================================
// REASSEMBLY
// ============================================================
size_t handle_fragment(const LoRaPacket &decoded, uint8_t* out_buf, size_t out_buf_len) {
    if (!(decoded.flags & FLAG_FRAGMENT)) {
        if (decoded.payload.size() > out_buf_len) return 0;
        memcpy(out_buf, decoded.payload.data(), decoded.payload.size());
        return decoded.payload.size();
    }
    std::string_view p(decoded.payload.data(), decoded.payload.size());
    size_t p1 = p.find('|'); if (p1 == std::string_view::npos) return 0;
    size_t p2 = p.find('|', p1 + 1); if (p2 == std::string_view::npos) return 0;
    size_t p3 = p.find('|', p2 + 1); if (p3 == std::string_view::npos) return 0;
    // Számok beolvasása string_view-ból teljesen heap-mentesen
    uint32_t msg_id = 0;
    auto res1 = std::from_chars(p.data(), p.data() + p1, msg_id);
    if (res1.ec != std::errc()) return 0;
    long idx_l = 0;
    auto res2 = std::from_chars(p.data() + p1 + 1, p.data() + p2, idx_l);
    if (res2.ec != std::errc() || idx_l < 0 || idx_l >= MAX_FRAGMENTS) return 0;
    long total_l = 0;
    auto res3 = std::from_chars(p.data() + p2 + 1, p.data() + p3, total_l);
    if (res3.ec != std::errc() || total_l <= 0 || total_l > MAX_FRAGMENTS) return 0;
    uint16_t idx = idx_l;
    uint16_t total = total_l;
    if (idx >= total) return 0;
    // Kivágjuk a tényleges adatot másolás nélkül
    std::string_view part = p.substr(p3 + 1);
    if (part.empty() || part.size() > FRAG_MAX_SIZE) return 0;
    portENTER_CRITICAL(&frag_mux);
    FragmentBuffer* buf = nullptr;
    int target_node_idx = -1; // Megjegyezzük, melyik globális slotban (0-3) van ez a node
    int free_slot_idx = -1;
    for (int i = 0; i < 4; i++) {
        if (fragment_ctx[i].active_node_id == decoded.node_id) {
            target_node_idx = i;
            for (int j = 0; j < 2; j++) {
                if (fragment_ctx[i].buffers[j].msg_id == msg_id && 
                    fragment_ctx[i].buffers[j].boot_id == decoded.boot_id) {
                    buf = &fragment_ctx[i].buffers[j];
                    break;
                }
            }
            if (!buf) {
                int oldest_j = (fragment_ctx[i].buffers[0].created < fragment_ctx[i].buffers[1].created) ? 0 : 1;
                buf = &fragment_ctx[i].buffers[oldest_j];
                buf->total = 0; 
            }
            break;
        }
        if (fragment_ctx[i].active_node_id == 0 && free_slot_idx == -1) {
            free_slot_idx = i; 
        }
    }
    if (!buf) {
        if (free_slot_idx == -1) {
            free_slot_idx = 0; 
        }
        target_node_idx = free_slot_idx;
        fragment_ctx[target_node_idx].active_node_id = decoded.node_id;
        buf = &fragment_ctx[target_node_idx].buffers[0]; // Első belső slot inicializálása
        buf->total = 0;
    }
    // Ha teljesen új üzenet-azonosító (msg_id), alaphelyzetbe hozzuk a slotot
    if (buf->total == 0) {
        buf->boot_id = decoded.boot_id;
        buf->msg_id = msg_id;
        buf->total = total;
        buf->received = 0;
        memset(buf->present, 0, sizeof(buf->present));
    }
    // Ha ezt a darabot még nem kaptuk meg, regisztráljuk
    if (!buf->present[idx]) {
        buf->present[idx] = true;
        buf->received++;
    }
    // Bemásoljuk a darabka tartalmát a fix bájttömbbe
    memcpy(buf->data[idx], part.data(), part.size());
    buf->lens[idx] = part.size();
    buf->created = millis();
    // Ha még hiányzik darab, kilépünk (0 hosszt ad vissza, de nem hiba)
    if (buf->received != buf->total) {
        portEXIT_CRITICAL(&frag_mux);
        return 0;
    }
    // --- ÖSSZEFŰZÉS (Minden darab megérkezett!) ---
    size_t actual_out_len = 0;
    bool success = true;
    for (uint16_t i = 0; i < total; i++) {
        if (!buf->present[i]) { success = false; break; }
        if (actual_out_len + buf->lens[i] > out_buf_len) { success = false; break; }
        memcpy(out_buf + actual_out_len, buf->data[i], buf->lens[i]);
        actual_out_len += buf->lens[i];
    }
    // Felszabadítjuk ezt az üzenet-slotot
    buf->total = 0;
    buf->msg_id = 0;
    if (target_node_idx != -1) {
        if (fragment_ctx[target_node_idx].buffers[0].total == 0 && 
            fragment_ctx[target_node_idx].buffers[1].total == 0) {
            fragment_ctx[target_node_idx].active_node_id = 0; 
        }
    }
    portEXIT_CRITICAL(&frag_mux);
    return success ? actual_out_len : 0;
}

struct DecodedLoRaPacket {
    uint8_t node_id;
    uint32_t boot_id;
    uint8_t flags;
    uint8_t type;
    uint32_t counter;
    uint16_t frag_id;
    uint8_t frag_index;
    uint8_t frag_total;
};

void process_received_packet(SX1262 &radio) {
    // 1. Csomaghossz ellenőrzése a rádióban
    size_t packet_len = radio.getPacketLength();
    if (packet_len == 0 || packet_len > 256) {
        radio.startReceive(); // Hibás méretnél azonnal visszaállunk vételre
        return;
    }
    // Fix puffer a stack-en a rádióból érkező NYERS (titkosított) adatnak
    uint8_t raw_packet[256];
    // Nyers bájtok beolvasása a rádióból (0 heap allokáció)
    int state = radio.readData(raw_packet, packet_len);
    if (state != RADIOLIB_ERR_NONE) {
        radio.startReceive();
        return;
    }
    // 2. Dekódolás és AES-GCM feloldás + Anti-Replay ellenőrzés
    LoRaPacket decoded;
    uint8_t decrypted_payload[256];
    size_t decrypted_len = sizeof(decrypted_payload);
    bool decrypt_ok = AESGCMHelperV6::decrypt(
        raw_packet, packet_len,
        decrypted_payload, decrypted_len, // Ide írja a tiszta szöveget, és frissíti a hosszt
        decoded.boot_id, decoded.node_id,
        decoded.type, decoded.flags, decoded.counter,
        decoded.frag_id, decoded.frag_index, decoded.frag_total
    );
    if (!decrypt_ok) {
        // Sikertelen dekódolás, rossz kulcs, hibás adat vagy ismétléses (Replay) támadás esetén elvetjük
        radio.startReceive();
        return;
    }
    // 3. Fragmentáció (Darabkák kezelése és összefűzése)
    // Létrehozunk egy nagyobb fix puffert a stack-en az összefűzött teljes üzenetnek (pl. 512 bájt)
    uint8_t assembled_payload[512]; 
    size_t full_len = handle_fragment(decoded, assembled_payload, sizeof(assembled_payload));
    // Ha full_len == 0, az azt jelenti, hogy a csomag fragmentált volt, de még NEM érkezett meg az összes darab.
    // Ilyenkor csendben kilépünk, a háttérben a fix puffer tárolja a részletet, és várjuk a többi részt.
    if (full_len == 0) {
        radio.startReceive();
        return;
    }
    // 4. JSON FELDOLGOZÁS (Ha az összes darab összegyűlt, vagy nem is volt fragmentált)
    // Biztosítjuk a lezáró nullát (\0), hogy a stringkezelő/JSON könyvtárak biztonságosan olvashassák szövegként
    if (full_len < sizeof(assembled_payload)) {
        assembled_payload[full_len] = '\0';
    }
    // ArduinoJson használata heap allokáció nélkül
    #if ARDUINOJSON_VERSION_MAJOR >= 7
        JsonDocument doc; // v7 automatikusan optimalizált belső memóriakezeléssel dolgozik
    #else
        StaticJsonDocument<512> doc; // v6 rögzített stack-méret
    #endif
    // deserializeJson közvetlenül a fix stack pufferből olvas, 0 bájt heap-et pazarolva
    DeserializationError error = deserializeJson(doc, assembled_payload, full_len);
    if (!error) {
        // --- SIKER! A JSON ÉS A CSOMAG TÖKÉLETESEN FELDOLGOZVA ---
        // Itt már biztonságosan kiolvashatod az adatokat a fix stack memóriából:
        const char* topic = doc["t"];
        const char* value = doc["v"];
        #if DEBUG
        Serial.printf("Sikeres csomag a Node %d felől! Topic: %s, Val: %s\n", decoded.node_id, topic, value);
        #endif
        // TODO: Itt tudod meghívni az MQTT továbbítást vagy egyéb logikát nyers char* mutatókkal
    } else {
        #if DEBUG
        Serial.printf("JSON parszolasi hiba: %s\n", error.c_str());
        #endif
    }
    // 5. Mindenképpen visszaállítjuk a rádiót folyamatos vételre
    radio.startReceive();
}

// ============================================================
// CLEANUP & FREE RTOS TASK
// ============================================================
void cleanup_fragment_buffers() {
    uint32_t now = millis();
    portENTER_CRITICAL(&frag_mux);
    
    // JAVÍTVA: 256 helyett csak a 4 aktív slotot ciklusozzuk
    for (int i = 0; i < 4; i++) {
        if (fragment_ctx[i].active_node_id != 0) {
            // Átnézzük a node mindkét belső slotját
            for (int j = 0; j < 2; j++) {
                if (fragment_ctx[i].buffers[j].total > 0) {
                    // Ha a fragmentum régebbi mint 5 másodperc, eldobjuk (timeout)
                    if (now - fragment_ctx[i].buffers[j].created > 5000) {
                        fragment_ctx[i].buffers[j].total = 0;
                        fragment_ctx[i].buffers[j].msg_id = 0;
                        fragment_ctx[i].buffers[j].received = 0;
                    }
                }
            }
            // Ha mindkét belső slot kiürült, magát a kliens slotot is felszabadítjuk
            if (fragment_ctx[i].buffers[0].total == 0 && fragment_ctx[i].buffers[1].total == 0) {
                fragment_ctx[i].active_node_id = 0;
            }
        }
    }
    portEXIT_CRITICAL(&frag_mux);
}

inline void fragment_cleanup_task(void *param) {
    while(true) {
        cleanup_fragment_buffers();
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}