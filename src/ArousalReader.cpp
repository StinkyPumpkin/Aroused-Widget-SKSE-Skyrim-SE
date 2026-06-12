#include "ArousalReader.h"

#include <RE/F/FunctionArguments.h>
#include <RE/I/IStackCallbackFunctor.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESDataHandler.h>
#include <RE/V/Variable.h>
#include <RE/V/VirtualMachine.h>

#include <atomic>
#include <optional>

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

    // Async dispatch of OSLArousedNative.<fn>(player) -> float into dst.
    void DispatchFloatGet(const char* fn, std::atomic<int>& dst) {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) return;
        auto* pc = RE::PlayerCharacter::GetSingleton();
        if (!pc) return;

        auto args = RE::MakeFunctionArguments(static_cast<RE::Actor*>(pc));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb(new FloatToAtomicCallback(dst, fn));
        vm->DispatchStaticCall("OSLArousedNative"sv, fn, args, cb);
    }
}

namespace ArousalReader {

    void Detect() {
        if (HasPlugin("OAroused.esp") || HasPlugin("OSLAroused.esp")) {
            g_source = Source::OSLAroused;
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
        if (g_source == Source::None) return;
        // OSLArousedNative covers both OSL and SLA modes (it ships the slaUtil shim).
        DispatchFloatGet("GetArousalNoSideEffects", g_arousal);
        DispatchFloatGet("GetExposure",             g_exposure);
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
}
