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
#include "SphereMapEffect.h"
#include "..\Common\BasicReaderWriter.h"

#define XML(X) TEXT(#X)

SphereMapEffect::SphereMapEffect() :
    m_refCount(1)
{
    m_constants.center = D2D1::Point2F(0, 0);
    m_constants.sceneSize = D2D1::Point2F(0, 0);
    m_constants.zoom = 0.5f;
}

HRESULT __stdcall SphereMapEffect::CreateRippleImpl(_Outptr_ IUnknown** ppEffectImpl)
{
    // Since the object's refcount is initialized to 1, we don't need to AddRef here.
    *ppEffectImpl = static_cast<ID2D1EffectImpl*>(new (std::nothrow) SphereMapEffect());

    if (*ppEffectImpl == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    else
    {
        return S_OK;
    }
}

HRESULT SphereMapEffect::Register(_In_ ID2D1Factory1* pFactory)
{
    // The inspectable metadata of an effect is defined in XML. This can be passed in from an external source
    // as well, however for simplicity we just inline the XML.
    PCWSTR pszXml =
        XML(
            <?xml version='1.0'?>
            <Effect>
                <!-- System Properties -->
                <Property name='DisplayName' type='string' value='SphereMap'/>
                <Property name='Author' type='string' value='Microsoft Corporation'/>
                <Property name='Category' type='string' value='Stylize'/>
                <Property name='Description' type='string' value='Renders image using spherical projection'/>
                <Inputs>
                    <Input name='Source'/>
                </Inputs>
                <!-- Custom Properties go here -->
                <Property name='Center' type='vector2'>
                    <Property name='DisplayName' type='string' value='Center'/>
                    <Property name='Default' type='vector2' value='(0.0, 0.0)' />
                </Property>
                <Property name='SceneSize' type='vector2'>
                    <Property name='DisplayName' type='string' value='SceneSize'/>
                    <Property name='Default' type='vector2' value='(0.0, 0.0)' />
                </Property>
                <Property name='Zoom' type='float'>
                    <Property name='DisplayName' type='string' value='Zoom'/>
                    <Property name='Default' type='float' value='0.5' />
                </Property>
            </Effect>
            );

    // This defines the bindings from specific properties to the callback functions
    // on the class that ID2D1Effect::SetValue() & GetValue() will call.
    const D2D1_PROPERTY_BINDING bindings[] =
    {
        D2D1_VALUE_TYPE_BINDING(L"Center", &SetCenter, &GetCenter),
        D2D1_VALUE_TYPE_BINDING(L"SceneSize", &SetSceneSize, &GetSceneSize),
        D2D1_VALUE_TYPE_BINDING(L"Zoom", &SetZoom, &GetZoom),
    };

    // This registers the effect with the factory, which will make the effect
    // instantiatable.
    return pFactory->RegisterEffectFromString(
        CLSID_CustomSphereMapEffect,
        pszXml,
        bindings,
        ARRAYSIZE(bindings),
        CreateRippleImpl
        );
}

IFACEMETHODIMP SphereMapEffect::Initialize(
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
        data = reader->ReadData("SphereMapEffect.cso");
    }
    catch (Platform::Exception^ e)
    {
        // Return error if file can not be read.
        return e->HResult;
    }

    HRESULT hr = pEffectContext->LoadPixelShader(GUID_SphereMapPixelShader, data->Data, data->Length);

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

HRESULT SphereMapEffect::SetCenter(D2D1_POINT_2F center)
{
    // The valid range is all possible point positions, so no clamping is needed.
    m_constants.center = center;
    return S_OK;
}

D2D1_POINT_2F SphereMapEffect::GetCenter() const
{
    return m_constants.center;
}

HRESULT SphereMapEffect::SetSceneSize(D2D1_POINT_2F size)
{
    if (size.x <= 0 || size.y <= 0)
    {
        return E_INVALIDARG;
    }
    else
    {
        m_constants.sceneSize = size;
        return S_OK;
    }
}

D2D1_POINT_2F SphereMapEffect::GetSceneSize() const
{
    return m_constants.sceneSize;
}

HRESULT SphereMapEffect::SetZoom(float zoom)
{
    if (zoom <= 0.0f)
    {
        return E_INVALIDARG;
    }
    else
    {
        m_constants.zoom = zoom;
        return S_OK;
    }
}

float SphereMapEffect::GetZoom() const
{
    return m_constants.zoom;
}

HRESULT SphereMapEffect::UpdateConstants()
{
    // Update the DPI if it has changed. This allows the effect to scale across different DPIs automatically.
    m_effectContext->GetDpi(&m_dpi, &m_dpi);
    m_constants.dpi = m_dpi;

    return m_drawInfo->SetPixelShaderConstantBuffer(reinterpret_cast<BYTE*>(&m_constants), sizeof(m_constants));
}

IFACEMETHODIMP SphereMapEffect::PrepareForRender(D2D1_CHANGE_TYPE changeType)
{
    return UpdateConstants();
}

// SetGraph is only called when the number of inputs changes. This never happens as we publish this effect
// as a single input effect.
IFACEMETHODIMP SphereMapEffect::SetGraph(_In_ ID2D1TransformGraph* pGraph)
{
    return E_NOTIMPL;
}

// Called to assign a new render info class, which is used to inform D2D on
// how to set the state of the GPU.
IFACEMETHODIMP SphereMapEffect::SetDrawInfo(_In_ ID2D1DrawInfo* pDrawInfo)
{
    m_drawInfo = pDrawInfo;

    return m_drawInfo->SetPixelShader(GUID_SphereMapPixelShader);
}

// Calculates the mapping between the output and input rects.
// Depending on the 3D view transform, the spherical map could sample from
// pixels anywhere in the original image.
IFACEMETHODIMP SphereMapEffect::MapOutputRectToInputRects(
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

    // TODO: For now, use the image's pixel bounds. But there probably is a way to calculate
    // the maximum displacement that may be sampled. This also doesn't handle cases
    // where the image has been transformed before this effect.
    pInputRects[0].left    = -1;
    pInputRects[0].top     = -1;
    pInputRects[0].right   = static_cast<long>(m_constants.sceneSize.x) + 1;
    pInputRects[0].bottom  = static_cast<long>(m_constants.sceneSize.y) + 1;

    return S_OK;
}

IFACEMETHODIMP SphereMapEffect::MapInputRectsToOutputRect(
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

    // TEST - spheremap rendering just wraps around, so it can fill infinite space.
    *pOutputRect = { -LONG_MAX, -LONG_MAX, LONG_MAX, LONG_MAX };
    //*pOutputRect = pInputRects[0];
    m_inputRect = pInputRects[0];

    // Indicate that entire output might contain transparency.
    ZeroMemory(pOutputOpaqueSubRect, sizeof(*pOutputOpaqueSubRect));

    return S_OK;
}

IFACEMETHODIMP SphereMapEffect::MapInvalidRect(
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

IFACEMETHODIMP_(UINT32) SphereMapEffect::GetInputCount() const
{
    return 1;
}

// D2D ensures that that effects are only referenced from one thread at a time.
// To improve performance, we simply increment/decrement our reference count
// rather than use atomic InterlockedIncrement()/InterlockedDecrement() functions.
IFACEMETHODIMP_(ULONG) SphereMapEffect::AddRef()
{
    m_refCount++;
    return m_refCount;
}

IFACEMETHODIMP_(ULONG) SphereMapEffect::Release()
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

// This enables the stack of parent interfaces to be queried. In the instance
// of the SphereMap interface, this method simply enables the developer
// to cast a SphereMap instance to an ID2D1EffectImpl or IUnknown instance.
IFACEMETHODIMP SphereMapEffect::QueryInterface(
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
