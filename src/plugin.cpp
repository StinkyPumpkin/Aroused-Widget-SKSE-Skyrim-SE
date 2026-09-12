#include "ArousalReader.h"
#include "HotkeyHandler.h"
#include "HudUI.h"
#include "Settings.h"
#include "SettingsUI.h"
#include "WidgetController.h"
#include "iHUDBridge.h"

#include <REL/Relocation.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <format>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <thread>

#include <ShlObj.h>
#include <KnownFolders.h>

namespace {
    std::atomic<bool> g_saveRunning{false};
    std::thread       g_saveThread;

    // Drops a marker file unconditionally at the start of SKSEPluginLoad so we
    // can prove the entry point ran even if spdlog blows up later.
    void WriteStartupMarker(const char* phase, const std::string& detail = {}) {
        try {
            namespace fs = std::filesystem;
            fs::path p = fs::current_path() / "Data" / "SKSE" / "Plugins";
            std::error_code ec;
            fs::create_directories(p, ec);
            p /= "ArousedWidget-startup.log";
            std::ofstream f(p, std::ios::app);
            const auto t = std::time(nullptr);
            f << t << " [" << phase << "]";
            if (!detail.empty()) f << " " << detail;
            f << "\n";
        } catch (...) {}
    }

    // Build the canonical SKSE log path manually. We don't use
    // SKSE::log::log_directory() because in our environment it resolves to
    // Documents\My Games\Skyrim.INI\SKSE — the INI filename instead of the
    // runtime directory name. Other plugins (PapyrusUtilDev, mfgfix, etc.)
    // log to Documents\My Games\Skyrim Special Edition\SKSE so we target
    // the same place ourselves.
    std::filesystem::path ResolveLogDirectory() {
        wchar_t* docs = nullptr;
        std::filesystem::path p;
        if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &docs))) {
            p = docs;
            ::CoTaskMemFree(docs);
        } else {
            // Last-ditch: USERPROFILE\Documents
            const wchar_t* up = _wgetenv(L"USERPROFILE");
            if (up) p = std::filesystem::path(up) / "Documents";
        }
        p /= "My Games";
        p /= "Skyrim Special Edition";
        p /= "SKSE";
        return p;
    }

    void InitializeLogging() {
        WriteStartupMarker("InitializeLogging-enter");

        std::filesystem::path logPath = ResolveLogDirectory();
        WriteStartupMarker("log_directory-resolved", logPath.string());

        std::error_code ec;
        std::filesystem::create_directories(logPath, ec);

        logPath /= "ArousedWidget.log";

        try {
            auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
            auto log  = std::make_shared<spdlog::logger>("global log", std::move(sink));
            log->set_level(spdlog::level::info);
            log->flush_on(spdlog::level::info);
            spdlog::set_default_logger(std::move(log));
            spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");
            SKSE::log::info("ArousedWidget v0.3.8 - logging initialized at {}", logPath.string());
            WriteStartupMarker("spdlog-init-ok", logPath.string());
        } catch (const std::exception& e) {
            WriteStartupMarker("spdlog-init-FAILED", std::string{e.what()} + " | path=" + logPath.string());
        } catch (...) {
            WriteStartupMarker("spdlog-init-FAILED-unknown");
        }
    }

    void SaveLoop() {
        using namespace std::chrono;
        while (g_saveRunning.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(1s);
            if (Settings::TakeDirty()) {
                Settings::Save();
            }
        }
    }

    bool g_addressLibraryMissing = false;

    // v0.3.4 (1.5.97 "game will not start" report): CommonLib fatally terminates the
    // game with a cryptic popup when the Address Library .bin for the RUNNING runtime
    // is absent (e.g. 1.5.97 users who installed only the AE edition). Check for the
    // file ourselves before any REL-dependent call; if missing, disable the widget and
    // tell the user exactly what to install instead of taking the game down.
    bool CheckAddressLibrary() {
        const auto ver = REL::Module::get().version();
        // Same file CommonLib's IDDatabase::load() opens: VR reads a .csv, AE a versionlib .bin,
        // SE a version .bin. (0.3.4-0.3.5 looked for a .bin on VR too, so every VR user got the
        // "Address Library missing" popup and a disabled widget - 0.3.6 fix.)
        std::string file;
        if (REL::Module::IsVR()) {
            file = std::format("Data/SKSE/Plugins/version-{}.csv", ver.string());
        } else if (REL::Module::IsAE()) {
            file = std::format("Data/SKSE/Plugins/versionlib-{}.bin", ver.string());
        } else {
            file = std::format("Data/SKSE/Plugins/version-{}.bin", ver.string());
        }
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::current_path() / file, ec)) {
            return true;
        }
        g_addressLibraryMissing = true;
        SKSE::log::error("Address Library file missing for runtime {}.{}.{}.{} ({}) - widget disabled",
                         ver.major(), ver.minor(), ver.patch(), ver.build(), file);
        const std::string text = std::format(
            "Aroused Widget: the Address Library file for your game version "
            "({}.{}.{}.{}) is not installed, so the widget has been disabled.\n\n"
            "Install \"Address Library for SKSE Plugins\" and pick the edition that "
            "matches your game (1.5.x = SE, 1.6.x / 1.7.x = AE, VR = the VR Address Library).\n\n"
            "The game will continue to run normally.",
            ver.major(), ver.minor(), ver.patch(), ver.build());
        ::MessageBoxA(nullptr, text.c_str(), "Aroused Widget", MB_OK | MB_ICONWARNING);
        return false;
    }

    void MessageCallback(SKSE::MessagingInterface::Message* msg) {
        if (g_addressLibraryMissing) return;
        switch (msg->type) {
        case SKSE::MessagingInterface::kPostLoad:
            SKSE::log::info("kPostLoad - registering MCP sections + iHUD bridge");
            if (!CheckAddressLibrary()) return;
            Settings::Load();
            SettingsUI::Register();
            iHUDBridge::Register();
            break;

        case SKSE::MessagingInterface::kDataLoaded:
            SKSE::log::info("kDataLoaded - detecting arousal source + registering HUD elements + starting loops");
            ArousalReader::Detect();
            HudUI::Register();
            HotkeyHandler::Register();
            WidgetController::Start();
            g_saveRunning.store(true);
            g_saveThread = std::thread(SaveLoop);
            break;

        // v0.3.3 (Nexus report "sometimes missing at start"): the kDataLoaded refresh runs at
        // the MAIN MENU where arousal frameworks have no player data, and the next poll could
        // be a full cadence away — leaving the widget blank for seconds after loading in.
        // Refresh immediately whenever a save is loaded or a new game starts.
        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            ArousalReader::Refresh();
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    WriteStartupMarker("SKSEPluginLoad-enter");
    InitializeLogging();
    SKSE::log::info("ArousedWidget loading...");
    SKSE::Init(skse);

    auto* mi = SKSE::GetMessagingInterface();
    if (!mi || !mi->RegisterListener(MessageCallback)) {
        SKSE::log::error("Failed to register SKSE messaging listener");
        WriteStartupMarker("listener-register-FAILED");
        return false;
    }
    SKSE::log::info("ArousedWidget loaded");
    WriteStartupMarker("SKSEPluginLoad-return-true");
    return true;
}
