#include "wifi_creds.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp_efuse.h>
#include <esp_hmac.h>
#include <esp_random.h>
#include <mbedtls/gcm.h>

static const char *const NAMESPACE = "wifi";
static const char *const KEY_SSID = "ssid";
static const char *const KEY_PASS = "pw";
// HMAC input for the AES key; change the version to make old ciphertext unreadable.
static const char HMAC_LABEL[] = "micromat wifi password v1";

struct PassBlob {
  uint8_t version = 1;
  uint8_t len = 0;  // plaintext length
  uint8_t iv[12];
  uint8_t tag[16];
  uint8_t data[WIFI_PASS_MAX];
};

static char ssid_[WIFI_SSID_MAX + 1] = "";

// --- eFuse key / AES key ------------------------------------------------------------
static bool findKey(hmac_key_id_t &id) {
  esp_efuse_block_t block;
  if (!esp_efuse_find_purpose(ESP_EFUSE_KEY_PURPOSE_HMAC_UP, &block)) return false;
  id = (hmac_key_id_t)(block - EFUSE_BLK_KEY0);
  return true;
}

bool credsKeyReady() {
  hmac_key_id_t id;
  return findKey(id);
}

bool credsCreateKey() {
  if (credsKeyReady()) return true;
  esp_efuse_block_t block = esp_efuse_find_unused_key_block();
  if (block == EFUSE_BLK_KEY_MAX) {
    Serial.println("keygen: no unused eFuse key block");
    return false;
  }
  uint8_t key[32];
  esp_fill_random(key, sizeof(key));
  // Also read- and write-protects the block (HMAC keys are never readable).
  esp_err_t err = esp_efuse_write_key(block, ESP_EFUSE_KEY_PURPOSE_HMAC_UP, key, sizeof(key));
  memset(key, 0, sizeof(key));
  Serial.printf("keygen: eFuse key block %d: %s\n", (int)(block - EFUSE_BLK_KEY0), esp_err_to_name(err));
  return err == ESP_OK && credsKeyReady();
}

static bool aesKey(uint8_t out[32]) {
  hmac_key_id_t id;
  return findKey(id) && esp_hmac_calculate(id, HMAC_LABEL, sizeof(HMAC_LABEL) - 1, out) == ESP_OK;
}

// --- Store ------------------------------------------------------------------------------
void credsBegin() {
  WiFi.persistent(false);  // the Wi-Fi driver must not keep its own (plain text) copy
  Preferences p;
  if (p.begin("nvs.net80211", false)) {  // erase anything it saved before
    if (p.isKey("sta.pswd") || p.isKey("sta.ssid")) {
      p.clear();
      Serial.println("wifi: erased the Wi-Fi driver's stored network");
    }
    p.end();
  }
  if (p.begin(NAMESPACE, true)) {
    p.getString(KEY_SSID, ssid_, sizeof(ssid_));
    p.end();
  }
}

bool credsHaveNetwork() { return ssid_[0] != '\0'; }
const char *credsSsid() { return ssid_; }

bool credsPassword(char *buf, size_t len) {
  buf[0] = '\0';
  if (!credsHaveNetwork()) return false;
  PassBlob b;
  Preferences p;
  bool ok = p.begin(NAMESPACE, true) && p.getBytes(KEY_PASS, &b, sizeof(b)) == sizeof(b);
  p.end();
  if (!ok || b.version != 1 || b.len > WIFI_PASS_MAX || b.len >= len) return false;
  if (b.len == 0) return true;  // open network

  uint8_t key[32];
  if (!aesKey(key)) return false;
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  ok = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
       mbedtls_gcm_auth_decrypt(&gcm, b.len, b.iv, sizeof(b.iv), (const uint8_t *)ssid_, strlen(ssid_), b.tag,
                                sizeof(b.tag), b.data, (uint8_t *)buf) == 0;
  mbedtls_gcm_free(&gcm);
  memset(key, 0, sizeof(key));
  if (!ok) {
    memset(buf, 0, len);
    return false;
  }
  buf[b.len] = '\0';
  return true;
}

bool credsSave(const char *ssid, const char *pass) {
  size_t n = strlen(pass);
  if (!ssid[0] || strlen(ssid) > WIFI_SSID_MAX || n > WIFI_PASS_MAX) return false;
  PassBlob b;
  b.len = n;
  esp_fill_random(b.iv, sizeof(b.iv));
  memset(b.data, 0, sizeof(b.data));
  memset(b.tag, 0, sizeof(b.tag));
  if (n > 0) {
    uint8_t key[32];
    if (!aesKey(key)) {
      Serial.println("wifi: no eFuse key, password NOT saved (serial: keygen)");
      return false;
    }
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    bool ok = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
              mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, n, b.iv, sizeof(b.iv), (const uint8_t *)ssid,
                                        strlen(ssid), (const uint8_t *)pass, b.data, sizeof(b.tag), b.tag) == 0;
    mbedtls_gcm_free(&gcm);
    memset(key, 0, sizeof(key));
    if (!ok) return false;
  }
  Preferences p;
  if (!p.begin(NAMESPACE, false)) return false;
  bool ok = p.putString(KEY_SSID, ssid) > 0 && p.putBytes(KEY_PASS, &b, sizeof(b)) == sizeof(b);
  p.end();
  if (ok) strlcpy(ssid_, ssid, sizeof(ssid_));
  Serial.printf("[%7lu] wifi: network \"%s\" %s\n", millis(), ssid, ok ? "saved (password encrypted)" : "SAVE FAILED");
  return ok;
}

void credsForget() {
  Preferences p;
  if (p.begin(NAMESPACE, false)) {
    p.clear();
    p.end();
  }
  ssid_[0] = '\0';
  Serial.printf("[%7lu] wifi: network forgotten\n", millis());
}
