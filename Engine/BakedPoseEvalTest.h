#pragma once

#include "BakedRig.h"
#include "BakedAnim.h"

namespace BakedPoseEvalTest
{
    void RunOnce(const BakedRig& rig, const BakedAnimClip* clip, float time);

    // Debug-only self test.
    // Verifies that world evaluation does not depend on node array order.
    // Not called by default.
    void SelfTest_WorldOrderIndependent();

    // Debug-only self test.
    // Verifies that missing TRS key streams fall back to rig rest TRS.
    // Not called by default.
    void SelfTest_PartialChannelTRS();
}
