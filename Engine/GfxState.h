// GfxState.h
#pragma once
#include <d3d11.h>
#include <CommonStates.h>

namespace Gfx {


    inline void SetOpaque(ID3D11DeviceContext* ctx, DirectX::CommonStates* s) {
        ctx->OMSetBlendState(s->Opaque(), nullptr, 0xFFFFFFFF);
        ctx->OMSetDepthStencilState(s->DepthDefault(), 0);
        ctx->RSSetState(s->CullNone());
    }


    inline void SetAlphaNonPremul(ID3D11DeviceContext* ctx, DirectX::CommonStates* s) {
        ctx->OMSetBlendState(s->NonPremultiplied(), nullptr, 0xFFFFFFFF);
        ctx->OMSetDepthStencilState(s->DepthRead(), 0);
        ctx->RSSetState(s->CullNone());
    }


    inline void SetAlphaPremul(ID3D11DeviceContext* ctx, DirectX::CommonStates* s) {
        ctx->OMSetBlendState(s->AlphaBlend(), nullptr, 0xFFFFFFFF);
        ctx->OMSetDepthStencilState(s->DepthRead(), 0);
        ctx->RSSetState(s->CullNone());
    }


    inline void SetAdditive(ID3D11DeviceContext* ctx, DirectX::CommonStates* s) {
        ctx->OMSetBlendState(s->Additive(), nullptr, 0xFFFFFFFF);
        ctx->OMSetDepthStencilState(s->DepthRead(), 0);
        ctx->RSSetState(s->CullNone());
    }

    inline void SetAlphaNonPremulWriteZ(ID3D11DeviceContext* ctx, DirectX::CommonStates* s) {
        ctx->OMSetBlendState(s->NonPremultiplied(), nullptr, 0xFFFFFFFF);
        ctx->OMSetDepthStencilState(s->DepthDefault(), 0);
        ctx->RSSetState(s->CullNone());
    }

} // namespace Gfx
