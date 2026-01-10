#pragma once

#include <string>

struct BakedRig;
struct BakedAnimClip;

namespace BakedDataValidate
{
    bool ValidateRig(const BakedRig& rig, std::string& out_error);
    bool ValidateRigAndClip(const BakedRig& rig, const BakedAnimClip* clip, std::string& out_error);
}
