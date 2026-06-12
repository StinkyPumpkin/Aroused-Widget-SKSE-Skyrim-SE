#pragma once
#include <optional>

// Reads OSL Aroused / SLA arousal + exposure for the player via the OSLArousedNative
// global Papyrus surface (DispatchStaticCall). Detection is by plugin presence;
// reads are async — the controller calls Refresh() each tick to fire a dispatch,
// and reads the cached value via GetArousalCached() / GetExposureCached().
namespace ArousalReader {

    enum class Source : std::uint8_t { None, OSLAroused, LegacySLA };

    void   Detect();          // call from kDataLoaded
    Source Active();

    // Fire async DispatchStaticCalls for arousal + exposure on the player.
    // Results land via callback into the cached values below.
    void Refresh();

    // Most recent value seen, or nullopt if never received.
    std::optional<int> GetArousalCached();   // 0..100
    std::optional<int> GetExposureCached();  // 0..100
}
