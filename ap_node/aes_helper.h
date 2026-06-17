#pragma once

#include "esp_system.h"
#include "esp_log.h"
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <string_view>
#include <algorithm>
#include <mbedtls/gcm.h>

static constexpr uint8_t MAGIC1 = 0xBA;
static constexpr uint8_t MAGIC2 = 0xBE;
static constexpr uint8_t PROTOCOL_VERSION = 3;
static constexpr size_t NONCE_SIZE = 12;
static constexpr size_t TAG_SIZE = 16;
static constexpr size_t AAD_SIZE = 16;
static constexpr size_t MAX_PAYLOAD = 512;
static constexpr uint8_t MAX_FRAGMENTS = 64;
static constexpr uint16_t REPLAY_BITS = 64;

struct LoRaPacket {
    uint32_t boot_id;
    uint8_t node_id;
    uint8_t type;
    uint8_t flags;
    uint32_t counter;
    uint16_t frag_id;
    uint8_t frag_index;
    uint8_t frag_total;
    std::string payload;
};

class AESGCMHelperV6 {
public:
    static void init(const uint8_t key[16]) {
        if (initialized) return;
        memcpy(AES_KEY, key, 16);
        initialized = true;
    }
    // HEAP-MENTES ENCRYPT: Visszaadja a generált csomag teljes méretét bájtokban (0 = hiba)
    static size_t encrypt(
        uint32_t boot_id, uint8_t node_id, uint32_t counter, uint8_t type, uint8_t flags,
        uint16_t frag_id, uint8_t frag_index, uint8_t frag_total, 
        std::string_view payload,     // std::string HELYETT (0 heap)
        uint8_t* out_buf,             // Hívó által biztosított célpuffer a kész csomagnak
        size_t out_buf_len            // A célpuffer maximális mérete
    ) {
        if (!initialized || payload.size() > MAX_PAYLOAD) return 0;
        // Szükséges csomagméret kiszámítása: MAGIC(2) + AAD(12) + NONCE(12) + PAYLOAD + TAG(16)
        size_t required_len = 2 + AAD_SIZE + NONCE_SIZE + payload.size() + TAG_SIZE;
        if (out_buf_len < required_len) return 0; // Nem fér el a pufferben
        // Nonce összeállítása
        uint8_t nonce[NONCE_SIZE]{};
        nonce[0] = (boot_id >> 24) & 0xFF;
        nonce[1] = (boot_id >> 16) & 0xFF;
        nonce[2] = (boot_id >> 8) & 0xFF;
        nonce[3] = boot_id & 0xFF;
        nonce[4] = node_id;
        nonce[5] = (counter >> 24) & 0xFF;
        nonce[6] = (counter >> 16) & 0xFF;
        nonce[7] = (counter >> 8) & 0xFF;
        nonce[8] = counter & 0xFF;
        nonce[9]  = type;
        nonce[10] = frag_index;
        nonce[11] = frag_total;
        // AAD összeállítása
        uint8_t aad[AAD_SIZE]{};
        aad[0] = PROTOCOL_VERSION;
        aad[1] = node_id;
        aad[2] = type;
        aad[3] = flags;
        aad[4] = (counter >> 24) & 0xFF;
        aad[5] = (counter >> 16) & 0xFF;
        aad[6] = (counter >> 8) & 0xFF;
        aad[7] = counter & 0xFF;
        aad[8] = (frag_id >> 8) & 0xFF;
        aad[9] = frag_id & 0xFF;
        aad[10] = frag_index;
        aad[11] = frag_total;
        // A kimeneti csomagot közvetlenül a célpufferbe írjuk (Nulla köztes vector allokáció!)
        out_buf[0] = MAGIC1;
        out_buf[1] = MAGIC2;
        size_t offset = 2;
        memcpy(out_buf + offset, aad, AAD_SIZE);       offset += AAD_SIZE;
        memcpy(out_buf + offset, nonce, NONCE_SIZE);   offset += NONCE_SIZE;
        // Elmentjük a titkosított adat kezdőcímét a pufferen belül
        uint8_t* cipher_ptr = out_buf + offset;
        offset += payload.size(); // Helyet hagyunk a titkosított adatnak
        uint8_t* tag_ptr = out_buf + offset; // A TAG a titkosított adat után következik
        mbedtls_gcm_context ctx;
        mbedtls_gcm_init(&ctx);
        mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, AES_KEY, 128);
        // Közvetlenül a payload.data()-ból olvassuk és a cipher_ptr-be írjuk
        int ret = mbedtls_gcm_crypt_and_tag(
            &ctx, MBEDTLS_GCM_ENCRYPT, payload.size(),
            nonce, NONCE_SIZE, aad, AAD_SIZE,
            (const uint8_t*)payload.data(), cipher_ptr, TAG_SIZE, tag_ptr
        );
        mbedtls_gcm_free(&ctx);
        if (ret != 0) return 0;
        return required_len; // Visszaadjuk a kész csomag pontos méretét
    }
    // HEAP-MENTES DECRYPT: std::vector helyett nyers csomagtömböt fogad, kimenete nyers puffer
    static bool decrypt(
        const uint8_t* packet, size_t packet_len, // std::vector helyett cím és hossz
        uint8_t* out_plain, size_t &out_plain_len, // std::string helyett kimeneti RAM puffer
        uint32_t &boot_id, uint8_t &node_id, uint8_t &type, uint8_t &flags, 
        uint32_t &counter, uint16_t &frag_id, uint8_t &frag_index, uint8_t &frag_total
    ) {
        if (!initialized || packet_len < 2 + AAD_SIZE + NONCE_SIZE + TAG_SIZE) return false;
        if (packet[0] != MAGIC1 || packet[1] != MAGIC2) return false;
        const uint8_t *aad = packet + 2;
        const uint8_t *nonce = aad + AAD_SIZE;
        boot_id = (uint32_t(nonce[0]) << 24) | (uint32_t(nonce[1]) << 16) | (uint32_t(nonce[2]) << 8)  | uint32_t(nonce[3]);
        const uint8_t *cipher = nonce + NONCE_SIZE;
        const uint8_t *tag = packet + packet_len - TAG_SIZE;
        size_t cipher_len = packet_len - 2 - AAD_SIZE - NONCE_SIZE - TAG_SIZE;
        if (cipher_len == 0 || cipher_len > MAX_PAYLOAD || out_plain_len < cipher_len) return false;
        node_id = aad[1];
        type    = aad[2];
        flags   = aad[3];
        counter = (uint32_t(aad[4]) << 24) | (uint32_t(aad[5]) << 16) | (uint32_t(aad[6]) << 8) | uint32_t(aad[7]);
        frag_id = (uint16_t(aad[8]) << 8) | uint16_t(aad[9]);
        frag_index = aad[10];
        frag_total = aad[11];
        mbedtls_gcm_context ctx;
        mbedtls_gcm_init(&ctx);
        mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, AES_KEY, 128);
        // Közvetlenül az out_plain pufferbe dekódolunk (0 heap)
        int ret = mbedtls_gcm_auth_decrypt(
            &ctx, cipher_len, nonce, NONCE_SIZE, aad, AAD_SIZE,
            tag, TAG_SIZE, cipher, out_plain
        );
        mbedtls_gcm_free(&ctx);
        if (ret != 0) return false;
        out_plain_len = cipher_len; // Beállítjuk a dekódolt szöveg pontos hosszát
        // --- Anti-Replay ablak logika (Változatlan, szép megoldás) ---
        struct ReplayState {
            uint32_t boot_id = 0;
            uint32_t last_counter = 0;
            uint64_t bitmap = 0;
        };
        static ReplayState replay[256];
        static portMUX_TYPE replay_mux = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&replay_mux);
        auto &r = replay[node_id];
        if (r.boot_id != boot_id) {
            r.boot_id = boot_id;
            r.last_counter = 0;
            r.bitmap = 0;
        }
        uint32_t last = r.last_counter;
        uint64_t map  = r.bitmap;
        if (counter > last) {
            uint32_t shift = counter - last;
            if (shift >= REPLAY_BITS) {
                map = 1ULL;
            } else {
                map <<= shift;
                map |= 1ULL;
            }
            r.last_counter = counter;
            r.bitmap = map;
        } else {
            uint32_t diff = last - counter;
            if (diff >= REPLAY_BITS) {
                portEXIT_CRITICAL(&replay_mux);
                return false;
            }
            uint64_t mask = (1ULL << diff);
            if (map & mask) {
                portEXIT_CRITICAL(&replay_mux);
                return false;
            }
            r.bitmap |= mask;
        }
        portEXIT_CRITICAL(&replay_mux);
        return true;
    }

private:
    static inline uint8_t AES_KEY[16];
    static inline bool initialized = false;
    static constexpr size_t NONCE_SIZE = 12;
    static constexpr size_t AAD_SIZE = 12;
    static constexpr size_t TAG_SIZE = 16;
    static constexpr size_t MAX_PAYLOAD = 256; 
    static constexpr uint8_t PROTOCOL_VERSION = 6;
    static constexpr uint8_t MAGIC1 = 0x1A;
    static constexpr uint8_t MAGIC2 = 0x2B;
    static constexpr uint32_t REPLAY_BITS = 64;
};

// --- Töredékkezelő architektúra javítása ---
struct FragmentBufferV6 {
    uint16_t total = 0;
    uint16_t received = 0;
    uint32_t created = 0;
    uint16_t frag_id = 0;
    uint8_t owner = 0;
    bool used = false;
    std::vector<std::string> parts;
    std::vector<bool> filled;
};

static constexpr size_t FRAGMENT_POOL_SIZE = 64;
inline static FragmentBufferV6 fragments[FRAGMENT_POOL_SIZE];
inline static portMUX_TYPE frag_mux = portMUX_INITIALIZER_UNLOCKED;
static std::string handle_fragment_v6(
    uint8_t node_id, uint16_t frag_id, uint8_t index, uint8_t total,
    const std::string &payload, uint32_t now)
    {
    if (index >= MAX_FRAGMENTS || total == 0 || total > MAX_FRAGMENTS || index >= total) return "";
    // 1. Slot keresése ütközéskezeléssel (Linear Probing)
    uint16_t base_slot = ((uint16_t(node_id) << 8) ^ frag_id) % FRAGMENT_POOL_SIZE;
    int target_slot = -1;
    bool force_reset = false;
    portENTER_CRITICAL(&frag_mux);
    for (size_t i = 0; i < FRAGMENT_POOL_SIZE; i++) {
        size_t current_slot = (base_slot + i) % FRAGMENT_POOL_SIZE;
        auto &buf = fragments[current_slot];
        if (buf.used && buf.frag_id == frag_id && buf.owner == node_id) {
            target_slot = current_slot; // Megtaláltuk a meglévő puffert
            break;
        }
        if (!buf.used && target_slot == -1) {
            target_slot = current_slot; // Megtaláltuk az első szabad helyet
        }
    }
    if (target_slot == -1) {
        force_reset = true;
        // Minden slot foglalt (DoS vagy túlterhelés), ürítsük a legrégebbit kényszerítve
        uint32_t oldest_time = 0xFFFFFFFF;
        for (size_t i = 0; i < FRAGMENT_POOL_SIZE; i++) {
            if (fragments[i].created < oldest_time) {
                oldest_time = fragments[i].created;
                target_slot = i;
            }
        }
    }
    auto &buf = fragments[target_slot];
    // Új puffer inicializálása, ha szükséges
    if (!buf.used || force_reset) {
        buf.total = total;
        buf.received = 0;
        buf.created = now;
        buf.frag_id = frag_id;
        buf.owner = node_id;
        buf.used = true;
        buf.parts.assign(total, std::string());
        buf.filled.assign(total, false);
    }
    if (index >= buf.parts.size() || buf.total != total) {
        portEXIT_CRITICAL(&frag_mux);
        return "";
    }
    if (!buf.filled[index]) {
        buf.received++;
        buf.filled[index] = true;
    }
    buf.parts[index] = payload;
    // Ha még nem jött meg az összes, korán kilépünk
    if (buf.received != buf.total) {
        portEXIT_CRITICAL(&frag_mux);
        return "";
    }
    auto parts = buf.parts;
    buf.used = false;
    portEXIT_CRITICAL(&frag_mux);
    std::string out;
    for (const auto &p : parts) {
        if (out.size() + p.size() > MAX_PAYLOAD) {
            return "";
        }
        out += p;
    }
    return out;
}

static inline void cleanup_fragments(uint32_t now) {
    portENTER_CRITICAL(&frag_mux);
    for (auto &f : fragments) {
        if (!f.used) continue;
        if ((uint32_t)(now - f.created) > 15000) {
            f.used = false;
            f.parts.clear();
            f.filled.clear();
        }
    }
    portEXIT_CRITICAL(&frag_mux);
}

#include <string_view>
#include <vector>

// Visszatérési érték helyett egy kimeneti pufferbe (out_buf) írunk, és visszaadjuk a titkosított adat hosszát.
inline size_t encrypt_packet(
    uint8_t node_id,
    uint8_t type,
    uint8_t flags,
    uint32_t counter,
    uint16_t frag_id,
    uint8_t frag_index,
    uint8_t frag_total,
    std::string_view payload,    // std::string HELYETT string_view (0 heap allokáció)
    uint8_t* out_buf,            // Kimeneti puffer címe (ide írja a titkosított adatot)
    size_t out_buf_len           // Kimeneti puffer maximális mérete
    ) {
    static uint32_t boot_id = esp_random();
    // Ha az AESGCMHelperV6-ot is át tudod írni, hogy std::string_view-t és nyers puffert fogadjon:
    return AESGCMHelperV6::encrypt(
        boot_id,
        node_id,
        counter,
        type,
        flags,
        frag_id,
        frag_index,
        frag_total,
        payload,
        out_buf,
        out_buf_len
    );
}

bool decrypt_packet(const std::vector<uint8_t>& temp_vector, LoRaPacket& decoded) {
    uint8_t decrypted_payload[256];
    size_t decrypted_len = sizeof(decrypted_payload);
    // Kicsomagoljuk a lokális változókba a 12 paraméteres új hívással
    bool success = AESGCMHelperV6::decrypt(
        temp_vector.data(), temp_vector.size(), // Nyers pointer és méret
        decrypted_payload, decrypted_len,       // Kimeneti puffer
        decoded.boot_id, decoded.node_id, decoded.type, decoded.flags,
        decoded.counter, decoded.frag_id, decoded.frag_index, decoded.frag_total
    );
    if (success) {
        // Ha sikeres, a kicsomagolt bájtokat visszaírjuk a decoded.payload-ba
        // Ha a decoded.payload std::string, akkor így:
        decoded.payload.assign((const char*)decrypted_payload, decrypted_len);
    }
    return success;
}
