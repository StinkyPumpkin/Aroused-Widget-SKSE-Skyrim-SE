#include "Settings.h"
#include "JsonStore.h"

#include <SKSE/SKSE.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>

namespace {
    std::mutex   g_mutex;
    Settings::Config g_config{};
    std::atomic<bool> g_dirty{false};

    void AppendArousal(std::string& out, const Settings::ArousalConfig& a) {
        char buf[400];
        std::snprintf(buf, sizeof(buf),
            "  \"arousal\": { \"enabled\": %s, \"x\": %.2f, \"y\": %.2f, "
            "\"iconHeightPx\": %.2f, \"textSizePx\": %.2f, \"showText\": %s, "
            "\"imageSet\": %d, \"glowPulse\": %s, \"anOverlays\": %s },\n",
            a.enabled  ? "true" : "false",
            a.x, a.y, a.iconHeightPx, a.textSizePx,
            a.showText ? "true" : "false",
            a.imageSet,
            a.glowPulse  ? "true" : "false",
            a.anOverlays ? "true" : "false",
            a.npcCrosshair ? "true" : "false");
        out += buf;
    }

    std::string Serialize(const Settings::Config& c) {
        std::string out;
        out.reserve(640);
        out += "{\n";
        AppendArousal(out, c.arousal);
        char buf[200];
        std::snprintf(buf, sizeof(buf),
            "  \"arousalCadenceSec\": %d,\n  \"hideHotkeyDX\": %d,\n  \"followCompassHide\": %s\n}\n",
            c.arousalCadenceSec, c.hideHotkeyDX, c.followCompassHide ? "true" : "false");
        out += buf;
        return out;
    }

    std::string FindValue(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\"";
        size_t p = json.find(needle);
        if (p == std::string::npos) return "";
        p += needle.size();
        while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == ':')) ++p;
        size_t end = p;
        while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != '\n') ++end;
        return json.substr(p, end - p);
    }

    void ParseArousal(const std::string& json, Settings::ArousalConfig& out) {
        size_t s = json.find("\"arousal\"");
        if (s == std::string::npos) return;
        size_t open  = json.find('{', s);
        size_t close = json.find('}', open);
        if (open == std::string::npos || close == std::string::npos) return;
        std::string body = json.substr(open + 1, close - open - 1);
        std::string v;
        if (!(v = FindValue(body, "enabled")).empty())      out.enabled      = (v.find("true") != std::string::npos);
        if (!(v = FindValue(body, "x")).empty())            out.x            = std::strtof(v.c_str(), nullptr);
        if (!(v = FindValue(body, "y")).empty())            out.y            = std::strtof(v.c_str(), nullptr);
        if (!(v = FindValue(body, "iconHeightPx")).empty()) out.iconHeightPx = std::strtof(v.c_str(), nullptr);
        if (!(v = FindValue(body, "textSizePx")).empty())   out.textSizePx   = std::strtof(v.c_str(), nullptr);
        if (!(v = FindValue(body, "showText")).empty())     out.showText     = (v.find("true") != std::string::npos);
        if (!(v = FindValue(body, "imageSet")).empty())     out.imageSet     = std::atoi(v.c_str());
        if (!(v = FindValue(body, "glowPulse")).empty())    out.glowPulse    = (v.find("true") != std::string::npos);
        if (!(v = FindValue(body, "anOverlays")).empty())   out.anOverlays   = (v.find("true") != std::string::npos);
        if (!(v = FindValue(body, "npcCrosshair")).empty()) out.npcCrosshair = (v.find("true") != std::string::npos);
    }
}

namespace Settings {

    std::unique_lock<std::mutex> Lock() { return std::unique_lock<std::mutex>(g_mutex); }
    Config& Get() { return g_config; }

    void Load() {
        std::string txt = JsonStore::Load();
        if (txt.empty()) {
            SKSE::log::info("Settings::Load - no config.json yet, using defaults");
            return;
        }
        auto lk = Lock();
        ParseArousal(txt, g_config.arousal);
        std::string v;
        if (!(v = FindValue(txt, "arousalCadenceSec")).empty())
            g_config.arousalCadenceSec = std::atoi(v.c_str());
        if (!(v = FindValue(txt, "hideHotkeyDX")).empty())
            g_config.hideHotkeyDX      = std::atoi(v.c_str());
        if (!(v = FindValue(txt, "followCompassHide")).empty())
            g_config.followCompassHide = (v.find("true") != std::string::npos);
        SKSE::log::info("Settings::Load - applied config from disk");
    }

    void Save() {
        std::string txt;
        { auto lk = Lock(); txt = Serialize(g_config); }
        if (JsonStore::Save(txt)) {
            SKSE::log::info("Settings::Save - wrote config.json ({} bytes)", txt.size());
        }
    }

    void MarkDirty() { g_dirty.store(true, std::memory_order_relaxed); }
    bool TakeDirty() { return g_dirty.exchange(false, std::memory_order_relaxed); }
}
