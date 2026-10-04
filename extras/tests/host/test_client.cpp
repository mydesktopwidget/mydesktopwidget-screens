// Off-device tests for the protocol core, mdwlib::Client. Built and run on a PC by run.ps1 (Windows,
// MSVC) or by any C++17 compiler with ArduinoJson 7 on the include path:
//
//   g++ -std=c++17 -I src -I <ArduinoJson>/src extras/tests/host/test_client.cpp src/mdw/Client.cpp
//
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "mdw/Client.h"

namespace {

int failures = 0;
int checks = 0;

#define CHECK(condition)                                                        \
  do {                                                                          \
    ++checks;                                                                   \
    if (!(condition)) {                                                         \
      ++failures;                                                               \
      std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition);      \
    }                                                                           \
  } while (0)

class FakePort : public mdwlib::Port {
 public:
  std::deque<uint8_t> inbound;
  std::vector<std::string> sent;  // payloads, header stripped

  size_t read(uint8_t* buffer, size_t capacity) override {
    size_t n = 0;
    while (n < capacity && !inbound.empty()) {
      buffer[n++] = inbound.front();
      inbound.pop_front();
    }
    return n;
  }

  void write(const uint8_t* data, size_t length) override {
    if (length < 4) return;
    const uint32_t declared = (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) |
                              (uint32_t(data[2]) << 8) | uint32_t(data[3]);
    CHECK(declared == length - 4);  // one write per frame, header included
    sent.emplace_back(reinterpret_cast<const char*>(data + 4), length - 4);
  }

  void frame(const std::string& json) { raw(header(json.size()) + json); }

  void raw(const std::string& bytes) {
    for (char c : bytes) inbound.push_back(uint8_t(c));
  }

  static std::string header(size_t length) {
    std::string h(4, '\0');
    h[0] = char((length >> 24) & 0xFF);
    h[1] = char((length >> 16) & 0xFF);
    h[2] = char((length >> 8) & 0xFF);
    h[3] = char(length & 0xFF);
    return h;
  }

  int count(const char* type) const {
    const std::string needle = std::string("\"type\":\"") + type + "\"";
    int n = 0;
    for (const auto& s : sent) n += s.find(needle) != std::string::npos ? 1 : 0;
    return n;
  }

  const std::string* last(const char* type) const {
    const std::string needle = std::string("\"type\":\"") + type + "\"";
    for (auto it = sent.rbegin(); it != sent.rend(); ++it) {
      if (it->find(needle) != std::string::npos) return &*it;
    }
    return nullptr;
  }
};

const char* kReady = R"({"v":1,"type":"ready","machine":"DESK-PC","commands":[],"modes":["values"]})";

std::string telemetry(const char* readings, const char* text = "{}") {
  return std::string(R"({"v":1,"type":"telemetry","at":1,"readings":)") + readings + R"(,"text":)" + text + "}";
}

struct Rig {
  FakePort port;
  mdwlib::Client client{port};
  uint32_t now = 1000;

  Rig() { client.setIdentity("3C61053ED814", "Desk gauge", "cyd-2432s028r", "test"); }

  void step(uint32_t ms = 0) {
    now += ms;
    client.loop(now);
  }

  void attach() {
    step();
    port.frame(kReady);
    step(10);
  }
};

void announcesUntilAnswered() {
  Rig rig;
  rig.step();
  CHECK(rig.port.count("attach") == 1);

  rig.step(500);
  CHECK(rig.port.count("attach") == 1);

  rig.step(600);
  CHECK(rig.port.count("attach") == 2);

  const std::string* attach = rig.port.last("attach");
  CHECK(attach != nullptr && attach->find(R"("id":"3C61053ED814")") != std::string::npos);
  CHECK(attach != nullptr && attach->find(R"("protocol":1)") != std::string::npos);
  CHECK(attach != nullptr && attach->find(R"("modes":["values"])") != std::string::npos);

  rig.port.frame(kReady);
  rig.step(10);
  rig.step(3000);
  CHECK(rig.port.count("attach") == 2);  // answered: no more announcing
}

void readyIsAnsweredWithTheSubscription() {
  Rig rig;
  bool connected = false;
  std::string machine;
  rig.client.subscribe("cpu.load.cpu_total", [](float) {});
  rig.client.subscribe("gpu.temperature.*", [](const char*, float) {});
  rig.client.onConnection([&](bool c, const char* m) { connected = c; machine = m; });

  rig.attach();

  CHECK(connected);
  CHECK(machine == "DESK-PC");
  const std::string* subscribe = rig.port.last("subscribe");
  CHECK(subscribe != nullptr && subscribe->find(R"(["cpu.load.cpu_total","gpu.temperature.*"])") != std::string::npos);
}

void aReadingArrivesAndAnAbsentOneIsReportedOnce() {
  Rig rig;
  float cpu = -1;
  int missing = 0;
  rig.client.subscribe("cpu.load.cpu_total", [&](float v) { cpu = v; });
  rig.client.onMissing("cpu.load.cpu_total", [&] { ++missing; });
  rig.attach();

  rig.port.frame(telemetry(R"({"cpu.load.cpu_total":42.5})"));
  rig.step(500);
  CHECK(cpu == 42.5f);
  CHECK(missing == 0);

  for (int i = 0; i < 3; ++i) {
    rig.port.frame(telemetry("{}"));
    rig.step(500);
  }
  CHECK(missing == 1);  // once, when it went missing - not every frame

  rig.port.frame(telemetry(R"({"cpu.load.cpu_total":0})"));
  rig.step(500);
  CHECK(cpu == 0.0f);  // a real zero is a value, not a missing reading
}

void aReadingAbsentFromTheFirstFrameIsReportedMissing() {
  Rig rig;
  int missing = 0;
  rig.client.onMissing("gpu.temperature.core", [&] { ++missing; });
  rig.attach();

  rig.port.frame(telemetry("{}"));
  rig.step(500);
  CHECK(missing == 1);
}

void aFamilyCallsBackForEachMember() {
  Rig rig;
  std::vector<std::string> seen;
  rig.client.subscribe("cpu.load.cpu_core_*", [&](const char* id, float) { seen.emplace_back(id); });
  rig.attach();

  rig.port.frame(telemetry(R"({"cpu.load.cpu_core_1":10,"cpu.load.cpu_core_2":20,"cpu.load.cpu_total":15})"));
  rig.step(500);

  CHECK(seen.size() == 2);
}

void textArrivesThroughOnText() {
  Rig rig;
  std::string title;
  rig.client.onText("media.title", [&](const char* t) { title = t; });
  rig.attach();

  rig.port.frame(telemetry("{}", R"({"media.title":"Clair de lune"})"));
  rig.step(500);

  CHECK(title == "Clair de lune");
}

void silenceMeansTheEngineHasGone() {
  Rig rig;
  bool connected = false;
  int missing = 0;
  rig.client.subscribe("cpu.load.cpu_total", [](float) {});
  rig.client.onMissing("cpu.load.cpu_total", [&] { ++missing; });
  rig.client.onConnection([&](bool c, const char*) { connected = c; });
  rig.attach();
  rig.port.frame(telemetry(R"({"cpu.load.cpu_total":5})"));
  rig.step(500);
  CHECK(connected);

  const int announced = rig.port.count("attach");
  rig.step(mdwlib::Client::kHostGoneMs + 100);

  CHECK(!connected);
  CHECK(!rig.client.connected());
  CHECK(missing == 1);                               // the reading it was showing is gone with it
  CHECK(rig.port.count("attach") == announced + 1);  // and it starts announcing again
}

void aPausedFrameKeepsTheLinkAlive() {
  Rig rig;
  std::string reason;
  rig.client.onPaused([&](const char* r, const char*) { reason = r; });
  rig.attach();

  for (int i = 0; i < 6; ++i) {
    rig.port.frame(R"({"v":1,"type":"paused","reason":"machine.sleeping","message":"x"})");
    rig.step(2000);
  }

  CHECK(rig.client.connected());
  CHECK(reason == "machine.sleeping");
}

void aBadHeaderIsDrainedUntilTheLineGoesQuiet() {
  Rig rig;
  float cpu = -1;
  rig.client.subscribe("cpu.load.cpu_total", [&](float v) { cpu = v; });
  rig.attach();

  rig.port.raw(std::string("\xFF\xFF\xFF\xFF", 4) + "garbage");
  rig.port.frame(telemetry(R"({"cpu.load.cpu_total":1})"));  // swallowed by the same burst
  rig.step(10);
  CHECK(cpu == -1);

  rig.step(mdwlib::Client::kQuietGapMs + 10);  // the line goes quiet
  rig.port.frame(telemetry(R"({"cpu.load.cpu_total":2})"));
  rig.step(10);
  CHECK(cpu == 2.0f);
  CHECK(rig.client.stats().skippedBytes > 0);
}

void aFrameTooBigToKeepIsReadPastAndTheNextOneArrives() {
  Rig rig;
  float cpu = -1;
  rig.client.subscribe("cpu.load.cpu_total", [&](float v) { cpu = v; });
  rig.attach();

  std::string big = R"({"v":1,"type":"telemetry","pad":")" + std::string(MDW_MAX_FRAME + 100, 'a') + "\"}";
  rig.port.frame(big);
  rig.port.frame(telemetry(R"({"cpu.load.cpu_total":7})"));
  rig.step(10);
  rig.step(10);
  rig.step(10);

  CHECK(rig.client.stats().oversized == 1);
  CHECK(cpu == 7.0f);
}

void aFrameThatStopsHalfWayIsDropped() {
  Rig rig;
  float cpu = -1;
  rig.client.subscribe("cpu.load.cpu_total", [&](float v) { cpu = v; });
  rig.attach();

  const std::string whole = telemetry(R"({"cpu.load.cpu_total":3})");
  rig.port.raw(FakePort::header(whole.size()) + whole.substr(0, 10));
  rig.step(10);
  rig.step(mdwlib::Client::kStallMs + 10);
  CHECK(rig.client.stats().stalls == 1);

  rig.port.frame(telemetry(R"({"cpu.load.cpu_total":4})"));
  rig.step(10);
  CHECK(cpu == 4.0f);
}

void aRefusalIsKeptAndAskedAboutSlowly() {
  Rig rig;
  rig.step();
  rig.port.frame(R"({"v":1,"type":"refused","reason":"protocol.version","message":"x"})");
  rig.step(10);

  CHECK(std::strcmp(rig.client.refusal(), "protocol.version") == 0);

  const int announced = rig.port.count("attach");
  rig.step(5000);
  CHECK(rig.port.count("attach") == announced);
  rig.step(5100);
  CHECK(rig.port.count("attach") == announced + 1);
}

void subscribingWhileConnectedSendsTheNewListAtOnce() {
  Rig rig;
  rig.attach();
  const int before = rig.port.count("subscribe");

  rig.client.subscribe("memory.load", [](float) {});

  CHECK(rig.port.count("subscribe") == before + 1);
}

void subscriptionsAreBounded() {
  Rig rig;
  for (int i = 0; i < MDW_MAX_SUBSCRIPTIONS; ++i) {
    char id[32];
    std::snprintf(id, sizeof(id), "sensor.%d", i);
    CHECK(rig.client.subscribe(id, [](float) {}));
  }
  CHECK(!rig.client.subscribe("one.too.many", [](float) {}));
  CHECK(!rig.client.subscribe(std::string(MDW_MAX_ID_LENGTH + 1, 'x').c_str(), [](float) {}));

  Rig other;
  CHECK(!other.client.subscribe("gpu.*", [](float) {}));                     // a family needs (id, value)
  CHECK(!other.client.subscribe("gpu.load", [](const char*, float) {}));     // and only a family takes it
}

}  // namespace

int main() {
  struct Test {
    const char* name;
    void (*run)();
  } tests[] = {
      {"announces until answered", announcesUntilAnswered},
      {"ready is answered with the subscription", readyIsAnsweredWithTheSubscription},
      {"a reading arrives, an absent one is reported once", aReadingArrivesAndAnAbsentOneIsReportedOnce},
      {"absent from the first frame is missing", aReadingAbsentFromTheFirstFrameIsReportedMissing},
      {"a family calls back for each member", aFamilyCallsBackForEachMember},
      {"text arrives through onText", textArrivesThroughOnText},
      {"silence means the engine has gone", silenceMeansTheEngineHasGone},
      {"paused keeps the link alive", aPausedFrameKeepsTheLinkAlive},
      {"a bad header is drained until quiet", aBadHeaderIsDrainedUntilTheLineGoesQuiet},
      {"a frame too big to keep is read past", aFrameTooBigToKeepIsReadPastAndTheNextOneArrives},
      {"a frame that stops half way is dropped", aFrameThatStopsHalfWayIsDropped},
      {"a refusal is kept and asked about slowly", aRefusalIsKeptAndAskedAboutSlowly},
      {"subscribing while connected sends at once", subscribingWhileConnectedSendsTheNewListAtOnce},
      {"subscriptions are bounded", subscriptionsAreBounded},
  };

  for (const Test& test : tests) {
    const int before = failures;
    test.run();
    std::printf("%s %s\n", failures == before ? "ok  " : "FAIL", test.name);
  }

  std::printf("\n%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
