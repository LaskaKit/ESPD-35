// =============================================================================
//  ESPD35_MeteoPlaneRadar
//  Viz NetSink.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "NetSink.h"
#include "Config.h"      // NET_BODY_BUDGET_MS
#include <HTTPClient.h>
#include <Stream.h>
#include <string.h>      // memcpy / memmove

// -----------------------------------------------------------------------------
//  NetBufSink - prilepuje telo do bufferu volajiciho a sam nikdy nealokuje.
// -----------------------------------------------------------------------------
class NetBufSink : public Stream {
public:
  // buf/cap: cil, ktery vlastni volajici. poll: yield + reset watchdogu, vola
  // se pri kazdem prichozim bloku. budgetMs: 0 casovy limit vypina.
  NetBufSink(uint8_t* buf, size_t cap, void (*poll)(), uint32_t budgetMs)
    : _buf(buf), _cap(buf ? cap : 0), _poll(poll), _budget(budgetMs),
      _start(millis()) {}

  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* data, size_t len) override;

  // Sink je jen pro zapis, tyhle se nikdy nepouziji.
  int  available() override { return 0; }
  int  read() override { return -1; }
  int  peek() override { return -1; }
  void flush() override {}

  size_t length()     const { return _len; }
  bool   overflowed() const { return _over; }
  bool   timedOut()   const { return _timeout; }

  // Ukonci telo nulou, aby se dalo brat jako C retezec. False, kdyz na
  // ukoncovaci bajt uz neni misto.
  bool terminate() {
    if (!_buf || _len >= _cap) return false;
    _buf[_len] = '\0';
    return true;
  }

private:
  uint8_t*  _buf;
  size_t    _cap;
  size_t    _len     = 0;
  void    (*_poll)();
  uint32_t  _budget;
  uint32_t  _start;
  bool      _over    = false;
  bool      _timeout = false;
};

size_t NetBufSink::write(const uint8_t* data, size_t len) {
  if (_poll) _poll();          // yield + reset watchdogu pri kazdem bloku

  // Kratky zapis je jedina paka, kterou writeToStreamDataBlock() rozumi: jednou
  // to zopakuje a pak skonci s HTTPC_ERROR_STREAM_WRITE. Nic jineho se nas
  // uvnitr te smycky nezepta.
  if (_budget && (millis() - _start) > _budget) {
    _timeout = true;
    return 0;
  }
  if (!_buf || len == 0) return 0;

  size_t room = (_len < _cap) ? (_cap - _len) : 0;
  if (len > room) {
    _over = true;              // zaznamenat - telo uz se nesmi pouzit
    len   = room;
  }
  if (len) {
    memcpy(_buf + _len, data, len);
    _len += len;
  }
  return len;
}

long Net_ReadBody(HTTPClient& http, uint8_t* buf, size_t cap, const char* tag,
                  void (*poll)()) {
  if (!buf || cap == 0) return -1;

  // Jeden bajt se drzi stranou, aby slo telo vzdy ukoncit nulou.
  NetBufSink sink(buf, cap - 1, poll, NET_BODY_BUDGET_MS);
  int ret = http.writeToStream(&sink);

  if (sink.timedOut()) {
    Serial.printf("%s: prenos prekrocil %lu ms, preruseno\n",
                  tag, (unsigned long)NET_BODY_BUDGET_MS);
    return -1;
  }
  if (sink.overflowed()) {
    // Zamerne fatalni. Telo, ktere preroste buffer, je uriznute telo, a pulka
    // JSONu nebo pulka PNG je horsi nez zadna aktualizace.
    Serial.printf("%s: odpoved presahla %u B, zahozeno\n", tag, (unsigned)cap);
    return -1;
  }
  if (ret < 0) {
    Serial.printf("%s: writeToStream chyba %d (%s)\n", tag, ret,
                  HTTPClient::errorToString(ret).c_str());
    return -1;
  }
  if (!sink.terminate()) {
    Serial.printf("%s: neni misto pro ukonceni retezce\n", tag);
    return -1;
  }
  return (long)sink.length();
}

// -----------------------------------------------------------------------------
//  NetScanSink - prohleda telo za behu a neulozi z nej nic.
// -----------------------------------------------------------------------------
class NetScanSink : public Stream {
public:
  NetScanSink(NetScanFn cb, void* user, void (*poll)(), uint32_t budgetMs)
    : _cb(cb), _user(user), _poll(poll), _budget(budgetMs), _start(millis()) {}

  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* data, size_t len) override;

  int  available() override { return 0; }
  int  read() override { return -1; }
  int  peek() override { return -1; }
  void flush() override {}

  void finish();               // proskenuj, co v okne zbylo (po konci prenosu)
  size_t length()   const { return _total; }
  bool   timedOut() const { return _timeout; }

private:
  void scanWindow();
  static const size_t WIN     = 512;
  static const size_t OVERLAP = NET_SCAN_MAX_TOKEN;

  NetScanFn _cb;
  void*     _user;
  void    (*_poll)();
  uint32_t  _budget;
  uint32_t  _start;
  char      _win[WIN + 1];
  size_t    _fill    = 0;
  size_t    _total   = 0;
  bool      _timeout = false;
};

size_t NetScanSink::write(const uint8_t* data, size_t len) {
  if (_poll) _poll();

  if (_budget && (millis() - _start) > _budget) {
    _timeout = true;
    return 0;                  // kratky zapis - jedina cesta, jak smycku zastavit
  }
  if (!data || len == 0) return 0;

  // Prijme vzdycky vsechno: okno se vyprazdnuje, jak se plni, takze telo muze
  // byt libovolne velke.
  size_t done = 0;
  while (done < len) {
    size_t room = WIN - _fill;
    size_t take = (len - done < room) ? (len - done) : room;
    memcpy(_win + _fill, data + done, take);
    _fill += take;
    done  += take;
    if (_fill == WIN) scanWindow();
  }
  _total += len;
  return len;
}

void NetScanSink::scanWindow() {
  _win[_fill] = '\0';
  if (_cb) _cb(_win, _user);
  // Konec okna se prenese dopredu, aby se retezec rozdeleny mezi dve okna
  // nasel cely.
  if (_fill > OVERLAP) {
    memmove(_win, _win + _fill - OVERLAP, OVERLAP);
    _fill = OVERLAP;
  }
}

void NetScanSink::finish() {
  if (!_fill) return;
  _win[_fill] = '\0';
  if (_cb) _cb(_win, _user);
  _fill = 0;
}

long Net_ScanBody(HTTPClient& http, NetScanFn cb, void* user, const char* tag,
                  void (*poll)()) {
  if (!cb) return -1;

  NetScanSink sink(cb, user, poll, NET_BODY_BUDGET_MS);
  int ret = http.writeToStream(&sink);
  sink.finish();               // posledni, necele okno

  if (sink.timedOut()) {
    Serial.printf("%s: prenos prekrocil %lu ms, preruseno\n",
                  tag, (unsigned long)NET_BODY_BUDGET_MS);
    return -1;
  }
  if (ret < 0) {
    Serial.printf("%s: writeToStream chyba %d (%s)\n", tag, ret,
                  HTTPClient::errorToString(ret).c_str());
    return -1;
  }
  return (long)sink.length();
}
