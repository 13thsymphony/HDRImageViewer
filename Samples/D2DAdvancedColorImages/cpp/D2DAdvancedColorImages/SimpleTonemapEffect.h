//********************************************************* 
// 
// Copyright (c) Microsoft. All rights reserved. 
// This code is licensed under the MIT License (MIT). 
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY 
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR 
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT. 
// 
//*********************************************************
// {4587A15E-425A-4311-A591-0B2E89D02E75}
DEFINE_GUID(GUID_SimpleTonemapEffectPixelShader, 
0x4587a15e, 0x425a, 0x4311, 0xa5, 0x91, 0xb, 0x2e, 0x89, 0xd0, 0x2e, 0x75);

// {0273FF16-6CA6-4AC4-BD14-77704DAD5391}
DEFINE_GUID(CLSID_CustomSimpleTonemapEffect, 0x273ff16, 0x6ca6, 0x4ac4, 0xbd, 0x14, 0x77, 0x70, 0x4d, 0xad, 0x53, 0x91);

// Our effect contains one transform, which is simply a wrapper around a pixel shader. As such,
// we can simply make the effect itself act as the transform.
class SimpleTonemapEffect : public ID2D1EffectImpl, public ID2D1DrawTransform
{
public:
    // Declare effect registration methods.
    static HRESULT Register(_In_ ID2D1Factory1* pFactory);

    static HRESULT __stdcall CreateSimpleTonemapImpl(_Outptr_ IUnknown** ppEffectImpl);

    // Declare property getter/setters
    HRESULT SetInputMaxLuminance(float nits);
    float GetInputMaxLuminance() const;

    HRESULT SetOutputMaxLuminance(float nits);
    float GetOutputMaxLuminance() const;

    HRESULT SetDisplayMode(D2D1_HDRTONEMAP_DISPLAY_MODE mode);
    D2D1_HDRTONEMAP_DISPLAY_MODE GetDisplayMode() const;

    // Declare ID2D1EffectImpl implementation methods.
    IFACEMETHODIMP Initialize(
        _In_ ID2D1EffectContext* pContextInternal,
        _In_ ID2D1TransformGraph* pTransformGraph
        );

    IFACEMETHODIMP PrepareForRender(D2D1_CHANGE_TYPE changeType);

    IFACEMETHODIMP SetGraph(_In_ ID2D1TransformGraph* pGraph);

    // Declare ID2D1DrawTransform implementation methods.
    IFACEMETHODIMP SetDrawInfo(_In_ ID2D1DrawInfo* pRenderInfo);

    // Declare ID2D1Transform implementation methods.
    IFACEMETHODIMP MapOutputRectToInputRects(
        _In_ const D2D1_RECT_L* pOutputRect,
        _Out_writes_(inputRectCount) D2D1_RECT_L* pInputRects,
        UINT32 inputRectCount
        ) const;

    IFACEMETHODIMP MapInputRectsToOutputRect(
        _In_reads_(inputRectCount) CONST D2D1_RECT_L* pInputRects,
        _In_reads_(inputRectCount) CONST D2D1_RECT_L* pInputOpaqueSubRects,
        UINT32 inputRectCount,
        _Out_ D2D1_RECT_L* pOutputRect,
        _Out_ D2D1_RECT_L* pOutputOpaqueSubRect
        );

    IFACEMETHODIMP MapInvalidRect(
        UINT32 inputIndex,
        D2D1_RECT_L invalidInputRect,
        _Out_ D2D1_RECT_L* pInvalidOutputRect
        ) const;

    // Declare ID2D1TransformNode implementation methods.
    IFACEMETHODIMP_(UINT32) GetInputCount() const;

    // Declare IUnknown implementation methods.
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();
    IFACEMETHODIMP QueryInterface(_In_ REFIID riid, _Outptr_ void** ppOutput);

private:
    SimpleTonemapEffect();
    HRESULT UpdateConstants();

    inline static float Clamp(float v, float low, float high)
    {
        return (v < low) ? low : (v > high) ? high : v;
    }

    inline static float Round(float v)
    {
        return floor(v + 0.5f);
    }

    // Prevents over/underflows when adding longs.
    inline static long SafeAdd(long base, long valueToAdd)
    {
        if (valueToAdd >= 0)
        {
            return ((base + valueToAdd) >= base) ? (base + valueToAdd) : LONG_MAX;
        }
        else
        {
            return ((base + valueToAdd) <= base) ? (base + valueToAdd) : LONG_MIN;
        }
    }

    // This struct defines the constant buffer of our pixel shader.
    struct
    {
        float inputMaxLum;  // In scRGB values.
        float outputMaxLum; // In scRGB values.
    } m_constants;

    Microsoft::WRL::ComPtr<ID2D1DrawInfo>      m_drawInfo;
    Microsoft::WRL::ComPtr<ID2D1EffectContext> m_effectContext;
    LONG                                       m_refCount;
    D2D1_RECT_L                                m_inputRect;
    float                                      m_dpi;
    D2D1_HDRTONEMAP_DISPLAY_MODE               m_ignored;
};
