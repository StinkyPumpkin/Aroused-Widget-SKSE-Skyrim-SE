#pragma once

// Drives the three widgets: arousal/exposure on a 5s tick, bounty on a 2s
// tick. Reads sources, pushes values via InfinityUIBridge.
namespace WidgetController {
    void Start(); // call from kDataLoaded
    void Stop();  // best-effort on shutdown
}
