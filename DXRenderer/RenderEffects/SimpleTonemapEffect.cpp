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

#include "pch.h"
#include <initguid.h>
#include "SimpleTonemapEffect.h"
#include "..\Common\BasicReaderWriter.h"

#define XML(X) TEXT(#X)

SimpleTonemapEffect::SimpleTonemapEffect() :
    m_refCount(1)
{
}

HRESULT __stdcall SimpleTonemapEffect::CreateSimpleTonemapImpl(_Outptr_ IUnknown** ppEffectImpl)
{
    // Since the object's refcount is initialized to 1, we don't need to AddRef here.
    *ppEffectImpl = static_cast<ID2D1EffectImpl*>(new (std::nothrow) SimpleTonemapEffect());

    if (*ppEffectImpl == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    else
    {
        return S_OK;
    }
}

HRESULT SimpleTonemapEffect::SetInputMaxLuminance(float nits)
{
    if (nits < 0.0f)
    {
        return E_INVALIDARG;
    }

    m_constants.inputMaxLum = nits / 80.0f; // scRGB 1.0 == 80 nits.

    return S_OK;
}

float SimpleTonemapEffect::GetInputMaxLuminance() const
{
    return m_constants.inputMaxLum * 80.0f;
}

HRESULT SimpleTonemapEffect::SetOutputMaxLuminance(float nits)
{
    if (nits < 0.0f || nits > 10000.0f)
    {
        return E_INVALIDARG;
    }

    m_constants.outputMaxLum = nits / 80.0f; // scRGB 1.0 == 80 nits.

    return S_OK;
}

float SimpleTonemapEffect::GetOutputMaxLuminance() const
{
    return m_constants.outputMaxLum * 80.0f;
}

HRESULT SimpleTonemapEffect::SetDisplayMode(D2D1_HDRTONEMAP_DISPLAY_MODE mode)
{
    m_ignored = mode;

    return S_OK;
}

D2D1_HDRTONEMAP_DISPLAY_MODE SimpleTonemapEffect::GetDisplayMode() const
{
    return m_ignored;
}

HRESULT SimpleTonemapEffect::Register(_In_ ID2D1Factory1* pFactory)
{
    // The inspectable metadata of an effect is defined in XML. This can be passed in from an external source
    // as well, however for simplicity we just inline the XML.
    PCWSTR pszXml =
        XML(
            <?xml version='1.0'?>
            <Effect>
                <!-- System Properties -->
                <Property name='DisplayName' type='string' value='Simple HDR Tonemapper' />
                <Property name='Author' type='string' value='Microsoft Corporation' />
                <Property name='Category' type='string' value='Stylize' />
                <Property name='Description' type='string' value='Rudimentary HDR tonemapper using rational beziers' />
                <Inputs>
                    <Input name='Source' />
                </Inputs>
                <!-- Custom Properties go here -->
                <!-- For convenience use the same definitions as the RS5 Direct2D HDR Tonemap effect - See d2d1effects_2.h -->
                <Property name='InputMaxLuminance' type='float'>
                    <Property name='DisplayName' type='string' value='Input average luminance (nits)'/>
                    <Property name='Default' type='float' value='4000.0' />
                </Property>
                <Property name='OutputMaxLuminance' type='float'>
                    <Property name='DisplayName' type='string' value='Output max luminance (nits)'/>
                    <Property name='Default' type='float' value='270.0' />
                </Property>
                <Property name='DisplayMode' type='enum'>
                    <Property name='DisplayName' type='string' value='Not used'/>
                    <Property name="Default" type="enum" value="0" />
                    <Fields>
                        <Field name='SDR' displayname='SDR' index="0" />
                        <Field name='HDR' displayname='HDR' index="1" />
                    </Fields>
                </Property>
            </Effect>
            );

    // This defines the bindings from specific properties to the callback functions
    // on the class that ID2D1Effect::SetValue() & GetValue() will call.
    const D2D1_PROPERTY_BINDING bindings[] =
    {
        // When accessing by index, use D2D1_HDRTONEMAP_PROP_INPUT_MAX_LUMINANCE = 0.
        D2D1_VALUE_TYPE_BINDING(L"InputMaxLuminance", &SetInputMaxLuminance, &GetInputMaxLuminance),

        // When accessing by index, use D2D1_HDRTONEMAP_PROP_OUTPUT_MAX_LUMINANCE = 1.
        D2D1_VALUE_TYPE_BINDING(L"OutputMaxLuminance", &SetOutputMaxLuminance, &GetOutputMaxLuminance),

        // When accessing by index, use D2D1_HDRTONEMAP_DISPLAY_MODE = 2.
        // Note this property is IGNORED by this effect, the entry point is implemented as a convenience
        // to drop-in with the 1809 tonemapper.
        D2D1_VALUE_TYPE_BINDING(L"DisplayMode", &SetDisplayMode, &GetDisplayMode),
    };

    // This registers the effect with the factory, which will make the effect
    // instantiatable.
    return pFactory->RegisterEffectFromString(
        CLSID_CustomSimpleTonemapEffect,
        pszXml,
        bindings,
        ARRAYSIZE(bindings),
        CreateSimpleTonemapImpl
        );
}

IFACEMETHODIMP SimpleTonemapEffect::Initialize(
    _In_ ID2D1EffectContext* pEffectContext,
    _In_ ID2D1TransformGraph* pTransformGraph
    )
{
    // To maintain consistency across different DPIs, this effect needs to cover more pixels at
    // higher than normal DPIs. The context is saved here so the effect can later retrieve the DPI.
    m_effectContext = pEffectContext;
    BasicReaderWriter^ reader = ref new BasicReaderWriter();
    Platform::Array<unsigned char, 1U>^ data;

    try
    {
        data = reader->ReadData("SimpleTonemapEffect.cso");
    }
    catch (Platform::Exception^ e)
    {
        // Return error if file can not be read.
        return e->HResult;
    }

    HRESULT hr = pEffectContext->LoadPixelShader(GUID_SimpleTonemapEffectPixelShader, data->Data, data->Length);

    // This loads the shader into the Direct2D image effects system and associates it with the GUID passed in.
    // If this method is called more than once (say by other instances of the effect) with the same GUID,
    // the system will simply do nothing, ensuring that only one instance of a shader is stored regardless of how
    // many time it is used.
    if (SUCCEEDED(hr))
    {
        // The graph consists of a single transform. In fact, this class is the transform,
        // reducing the complexity of implementing an effect when all we need to
        // do is use a single pixel shader.
        hr = pTransformGraph->SetSingleTransformNode(this);
    }

    return hr;
}

HRESULT SimpleTonemapEffect::UpdateConstants()
{
    // Update the DPI if it has changed. This allows the effect to scale across different DPIs automatically.
    m_effectContext->GetDpi(&m_dpi, &m_dpi); // DPI is never used right now.

    return m_drawInfo->SetPixelShaderConstantBuffer(reinterpret_cast<BYTE*>(&m_constants), sizeof(m_constants));
}

IFACEMETHODIMP SimpleTonemapEffect::PrepareForRender(D2D1_CHANGE_TYPE changeType)
{
    return UpdateConstants();
}

// SetGraph is only called when the number of inputs changes. This never happens as we publish this effect
// as a single input effect.
IFACEMETHODIMP SimpleTonemapEffect::SetGraph(_In_ ID2D1TransformGraph* pGraph)
{
    return E_NOTIMPL;
}

// Called to assign a new render info class, which is used to inform D2D on
// how to set the state of the GPU.
IFACEMETHODIMP SimpleTonemapEffect::SetDrawInfo(_In_ ID2D1DrawInfo* pDrawInfo)
{
    m_drawInfo = pDrawInfo;

    return m_drawInfo->SetPixelShader(GUID_SimpleTonemapEffectPixelShader);
}

// Calculates the mapping between the output and input rects.
IFACEMETHODIMP SimpleTonemapEffect::MapOutputRectToInputRects(
    _In_ const D2D1_RECT_L* pOutputRect,
    _Out_writes_(inputRectCount) D2D1_RECT_L* pInputRects,
    UINT32 inputRectCount
    ) const
{
    // This effect has exactly one input, so if there is more than one input rect,
    // something is wrong.
    if (inputRectCount != 1)
    {
        return E_INVALIDARG;
    }

    pInputRects[0].left    = pOutputRect->left;
    pInputRects[0].top     = pOutputRect->top;
    pInputRects[0].right   = pOutputRect->right;
    pInputRects[0].bottom  = pOutputRect->bottom;

    return S_OK;
}

IFACEMETHODIMP SimpleTonemapEffect::MapInputRectsToOutputRect(
    _In_reads_(inputRectCount) CONST D2D1_RECT_L* pInputRects,
    _In_reads_(inputRectCount) CONST D2D1_RECT_L* pInputOpaqueSubRects,
    UINT32 inputRectCount,
    _Out_ D2D1_RECT_L* pOutputRect,
    _Out_ D2D1_RECT_L* pOutputOpaqueSubRect
    )
{
    // This effect has exactly one input, so if there is more than one input rect,
    // something is wrong.
    if (inputRectCount != 1)
    {
        return E_INVALIDARG;
    }

    *pOutputRect = pInputRects[0];
    m_inputRect = pInputRects[0];

    // Indicate that entire output might contain transparency.
    ZeroMemory(pOutputOpaqueSubRect, sizeof(*pOutputOpaqueSubRect));

    return S_OK;
}

IFACEMETHODIMP SimpleTonemapEffect::MapInvalidRect(
    UINT32 inputIndex,
    D2D1_RECT_L invalidInputRect,
    _Out_ D2D1_RECT_L* pInvalidOutputRect
    ) const
{
    HRESULT hr = S_OK;

    // Indicate that the entire output may be invalid.
    *pInvalidOutputRect = m_inputRect;

    return hr;
}

IFACEMETHODIMP_(UINT32) SimpleTonemapEffect::GetInputCount() const
{
    return 1;
}

// D2D ensures that that effects are only referenced from one thread at a time.
// To improve performance, we simply increment/decrement our reference count
// rather than use atomic InterlockedIncrement()/InterlockedDecrement() functions.
IFACEMETHODIMP_(ULONG) SimpleTonemapEffect::AddRef()
{
    m_refCount++;
    return m_refCount;
}

IFACEMETHODIMP_(ULONG) SimpleTonemapEffect::Release()
{
    m_refCount--;

    if (m_refCount == 0)
    {
        delete this;
        return 0;
    }
    else
    {
        return m_refCount;
    }
}

// This enables the stack of parent interfaces to be queried.
IFACEMETHODIMP SimpleTonemapEffect::QueryInterface(
    _In_ REFIID riid,
    _Outptr_ void** ppOutput
    )
{
    *ppOutput = nullptr;
    HRESULT hr = S_OK;

    if (riid == __uuidof(ID2D1EffectImpl))
    {
        *ppOutput = reinterpret_cast<ID2D1EffectImpl*>(this);
    }
    else if (riid == __uuidof(ID2D1DrawTransform))
    {
        *ppOutput = static_cast<ID2D1DrawTransform*>(this);
    }
    else if (riid == __uuidof(ID2D1Transform))
    {
        *ppOutput = static_cast<ID2D1Transform*>(this);
    }
    else if (riid == __uuidof(ID2D1TransformNode))
    {
        *ppOutput = static_cast<ID2D1TransformNode*>(this);
    }
    else if (riid == __uuidof(IUnknown))
    {
        *ppOutput = this;
    }
    else
    {
        hr = E_NOINTERFACE;
    }

    if (*ppOutput != nullptr)
    {
        AddRef();
    }

    return hr;
}
