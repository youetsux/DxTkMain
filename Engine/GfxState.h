// GfxState.h
#pragma once
#include <d3d11.h>
#include <CommonStates.h>

namespace Gfx {

    // 不透明(デフォルト)描画セット
    inline void SetOpaque(ID3D11DeviceContext* ctx, DirectX::CommonStates* s) {
        ctx->OMSetBlendState(s->Opaque(), nullptr, 0xFFFFFFFF);
        ctx->OMSetDepthStencilState(s->DepthDefault(), 0);
        ctx->RSSetState(s->CullNone());
    }

    // 透過PNGなどストレートα（Non-premultiplied）用
    inline void SetAlphaNonPremul(ID3D11DeviceContext* ctx, DirectX::CommonStates* s) {
        ctx->OMSetBlendState(s->NonPremultiplied(), nullptr, 0xFFFFFFFF);
        ctx->OMSetDepthStencilState(s->DepthRead(), 0);
        ctx->RSSetState(s->CullNone());
    }

    // プリマルチα用（PSで rgb*=a している/プリマルチ画像を使う時）
    inline void SetAlphaPremul(ID3D11DeviceContext* ctx, DirectX::CommonStates* s) {
        ctx->OMSetBlendState(s->AlphaBlend(), nullptr, 0xFFFFFFFF);
        ctx->OMSetDepthStencilState(s->DepthRead(), 0);
        ctx->RSSetState(s->CullNone());
    }

    // 追加：加算合成など（必要になったら）
    inline void SetAdditive(ID3D11DeviceContext* ctx, DirectX::CommonStates* s) {
        ctx->OMSetBlendState(s->Additive(), nullptr, 0xFFFFFFFF);
        ctx->OMSetDepthStencilState(s->DepthRead(), 0);
        ctx->RSSetState(s->CullNone());
    }

    inline void SetAlphaNonPremulWriteZ(ID3D11DeviceContext* ctx, DirectX::CommonStates* s) {
        ctx->OMSetBlendState(s->NonPremultiplied(), nullptr, 0xFFFFFFFF);
        ctx->OMSetDepthStencilState(s->DepthDefault(), 0); // ★ Z書き込みあり
        ctx->RSSetState(s->CullNone());
    }

} // namespace Gfx
