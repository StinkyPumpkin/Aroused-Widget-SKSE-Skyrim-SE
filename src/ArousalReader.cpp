#include "ArousalReader.h"

#include <RE/F/FunctionArguments.h>
#include <RE/I/IStackCallbackFunctor.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESDataHandler.h>
#include <RE/V/Variable.h>
#include <RE/V/VirtualMachine.h>

#include <atomic>
#include <optional>

#include <Windows.h>

namespace {
    ArousalReader::Source g_source = ArousalReader::Source::None;

    // OSL Aroused arousal range is 0..100 (float). We cache as int.
    std::atomic<int>  g_arousal{-1};
    std::atomic<int>  g_exposure{-1};

    bool HasPlugin(const char* name) {
        auto* dh = RE::TESDataHandler::GetSingleton();
        if (!dh) return false;
        return dh->LookupModByName(name) != nullptr;
    }

    // Stack callback that writes a clamped int into a target std::atomic.
    class FloatToAtomicCallback : public RE::BSScript::IStackCallbackFunctor {
    public:
        explicit FloatToAtomicCallback(std::atomic<int>& dst, const char* tag)
            : _dst(dst), _tag(tag) {}

        void operator()(RE::BSScript::Variable a_result) override {
            if (!a_result.IsFloat()) return;
            float f = a_result.GetFloat();
            if (f < 0.0f) f = 0.0f;
            if (f > 100.0f) f = 100.0f;
            _dst.store(static_cast<int>(f + 0.5f), std::memory_order_relaxed);
        }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        std::atomic<int>& _dst;
        const char*       _tag;
    };

    // Async dispatch of <cls>.<fn>(actor) -> float into dst.
    void DispatchFloatGetActor(std::string_view cls, const char* fn, RE::Actor* actor, std::atomic<int>& dst) {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm || !actor) return;
        auto args = RE::MakeFunctionArguments(std::move(actor));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb(new FloatToAtomicCallback(dst, fn));
        vm->DispatchStaticCall(RE::BSFixedString(cls), fn, args, cb);
    }

    // Player convenience.
    void DispatchFloatGet(std::string_view cls, const char* fn, std::atomic<int>& dst) {
        DispatchFloatGetActor(cls, fn, RE::PlayerCharacter::GetSingleton(), dst);
    }

    std::atomic<int> g_npcArousal{ -1 };
}

namespace ArousalReader {

    void Detect() {
        if (HasPlugin("OAroused.esp") || HasPlugin("OSLAroused.esp")) {
            g_source = Source::OSLAroused;
        } else if (::GetModuleHandleA("SexlabArousedNG.dll")) {
            // SexLab Aroused NG (crajjjj) — native SLA rewrite. Ships the same
            // SexLabAroused.esm as legacy SLA, so probe its DLL before the esm
            // check. Its global natives live on slaInternalModules.
            g_source = Source::SLANG;
        } else if (HasPlugin("SexLabAroused.esm") || HasPlugin("SexLabArousedRedux.esp")) {
            // OSL Aroused 2.x ships SexLabAroused.esm too as the SLA-mode shim.
            // If only the .esm is present without OAroused/OSLAroused, we're on legacy SLA.
            g_source = Source::LegacySLA;
        } else {
            g_source = Source::None;
        }
        SKSE::log::info("ArousalReader::Detect -> source={}", static_cast<int>(g_source));
    }

    Source Active() { return g_source; }

    void Refresh() {
        switch (g_source) {
        case Source::OSLAroused:
        case Source::LegacySLA:
            // OSLArousedNative covers both OSL and SLA modes (it ships the slaUtil shim).
            DispatchFloatGet("OSLArousedNative", "GetArousalNoSideEffects", g_arousal);
            DispatchFloatGet("OSLArousedNative", "GetExposure",             g_exposure);
            break;
        case Source::SLANG:
            // SLA NG global native: float slaInternalModules.GetArousal(Actor).
            // No exposure getter exists in its API — exposure stays absent.
            DispatchFloatGet("slaInternalModules", "GetArousal", g_arousal);
            break;
        default:
            break;
        }
    }

    std::optional<int> GetArousalCached() {
        int v = g_arousal.load(std::memory_order_relaxed);
        if (v < 0) return std::nullopt;
        return v;
    }

    std::optional<int> GetExposureCached() {
        int v = g_exposure.load(std::memory_order_relaxed);
        if (v < 0) return std::nullopt;
        return v;
    }

    void ReadActorArousal(RE::Actor* a_actor) {
        if (g_source == Source::None || !a_actor) return;
        if (g_source == Source::SLANG) {
            DispatchFloatGetActor("slaInternalModules", "GetArousal", a_actor, g_npcArousal);
        } else {
            DispatchFloatGetActor("OSLArousedNative", "GetArousalNoSideEffects", a_actor, g_npcArousal);
        }
    }

    std::optional<int> GetNpcArousalCached() {
        int v = g_npcArousal.load(std::memory_order_relaxed);
        if (v < 0) return std::nullopt;
        return v;
    }
}
