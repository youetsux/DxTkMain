#pragma once

#include "BakedRig.h"
#include "BakedAnim.h"
#include "BakedPose.h"

#include <DirectXMath.h>

#include <vector>

struct BakedPoseEval
{
    static float NormalizeTime(const BakedAnimClip* clip, float time, bool loop);

    static void EvaluateNodeLocal(const BakedRig& rig,
                                 const BakedAnimClip* clip,
                                 float time,
                                 std::vector<DirectX::XMMATRIX>& out_node_local);

    static void EvaluateNodeWorld(const BakedRig& rig,
                                 const BakedAnimClip* clip,
                                 float time,
                                 std::vector<DirectX::XMMATRIX>& out_node_world);

    static void BuildSkinPaletteStub(const BakedPoseWorld& world,
                                    BakedSkinPalette& out_palette);

};

struct BakedPosePlayer
{
    BakedPosePlayer();

    void SetRig(const BakedRig* rig);
    void SetClip(const BakedAnimClip* clip);
    void SetLoop(bool loop);
    void SetBuildPaletteOnEvaluate(bool enable);

    void Evaluate(float time);


    void EvaluateWithPalette(float time);
    const BakedPoseLocal& GetLocalPose() const;
    const BakedPoseWorld& GetWorldPose() const;


    void BuildSkinPalette();

    const BakedSkinPalette& GetSkinPalette() const;
private:
    const BakedRig* rig_;
    const BakedAnimClip* clip_;
    bool loop_;
    bool build_palette_on_evaluate_;

    BakedPoseLocal local_;
    BakedPoseWorld world_;

    BakedSkinPalette palette_;
};
