// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/PartialData.h"

#include <cstdint>
#include <vector>

class Project;

/**
 * Amplitude scaling operations on the partial set.
 *
 * computePeak() is thread-safe (read-only on its inputs) and can run on any
 * thread. scale() must be called on the message thread; it registers an
 * undoable action with the project's EditHistory.
 */
namespace AmplitudeOps {

    /** Message-thread snapshot of one partial's breakpoints, safe to hand to a
     *  background thread for offline synthesis. */
    struct SynthData {
        std::vector<Breakpoint> breakpoints;
    };

    /** Offline synthesis at 8 820 Hz. Returns the peak absolute sample value
     *  of the mixed signal (all supplied partials summed). Returns 0 if the
     *  input is empty or silent. */
    float computePeak(const std::vector<SynthData>& partials);

    /** Multiply every breakpoint amplitude of the specified partials by
     *  `factor`. Registers an undoable ScaleAmplitudesAction with the
     *  project's EditHistory. No-op if factor <= 0 or ids is empty. */
    void scale(Project&                       project,
               const std::vector<uint32_t>&  partialIds,
               float                         factor);

} // namespace AmplitudeOps