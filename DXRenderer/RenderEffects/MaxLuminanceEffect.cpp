#include "pch.h"
#include <initguid.h>
#include "MaxLuminanceEffect.h"
#include "..\Common\BasicReaderWriter.h"

#define XML(X) TEXT(#X)

MaxLuminanceEffect::MaxLuminanceEffect() :
    m_refCount(1),
    m_constants{
        96.0f, // DPI
        80.0f  // Max nits
    }
{
}


float MaxLuminanceEffect::GetMaxLum() const
{
    return m_constants.maxLuminance;
}

HRESULT MaxLuminanceEffect::SetMaxLum(float nits)
{
    if (nits < 0.0f || nits > 10000.0f)
    {
        return E_INVALIDARG;
    }

    m_constants.maxLuminance = nits;

    return S_OK;
}

HRESULT __stdcall MaxLuminanceEffect::CreateMaxLuminanceImpl(_Outptr_ IUnknown** ppEffectImpl)
{
    // Since the object's refcount is initialized to 1, we don't need to AddRef here.
    *ppEffectImpl = static_cast<ID2D1EffectImpl*>(new (std::nothrow) MaxLuminanceEffect());

    if (*ppEffectImpl == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    else
    {
        return S_OK;
    }
}

HRESULT MaxLuminanceEffect::Register(_In_ ID2D1Factory1* pFactory)
{
    // The inspectable metadata of an effect is defined in XML. This can be passed in from an external source
    // as well, however for simplicity we just inline the XML.
    PCWSTR pszXml =
        XML(
            <?xml version='1.0'?>
            <Effect>
                <!-- System Properties -->
                <Property name='DisplayName' type='string' value='MaxLuminance'/>
                <Property name='Author' type='string' value='Wendy Ho'/>
                <Property name='Category' type='string' value='Source'/>
                <Property name='Description' type='string' value='Renders all colors above the max luminance of the display as black.'/>
                <Inputs>
                    <Input name = 'Source'/>
                </Inputs>
                <!-- Custom Properties go here -->
                <Property name='MaxLuminance' type ='float'>
                    <Property name='DisplayName' type='string' value='Max Luminance of the display in nits.'/>
                </Property>
            </Effect>
            );
    const D2D1_PROPERTY_BINDING bindings[] =
    {
        D2D1_VALUE_TYPE_BINDING(
            L"MaxLuminance",      // The name of property. Must match name attribute in XML.
            &SetMaxLum,     // The setter method that is called on "SetValue".
            &GetMaxLum      // The getter method that is called on "GetValue".
            )
    };
    // This registers the effect with the factory, which will make the effect
    // instantiatable.
    return pFactory->RegisterEffectFromString(
        CLSID_CustomMaxLuminanceEffect,
        pszXml,
        bindings,
        ARRAYSIZE(bindings),
        //nullptr,
        //0,
        CreateMaxLuminanceImpl
        );
}

IFACEMETHODIMP MaxLuminanceEffect::Initialize(
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
        // CSO files are stored in the project subfolder in the app install location.
        data = reader->ReadData("DXRenderer\\MaxLuminanceEffect.cso");
    }
    catch (Platform::Exception^ e)
    {
        // Return error if file can not be read.
        return e->HResult;
    }

    HRESULT hr = pEffectContext->LoadPixelShader(GUID_MaxLuminancePixelShader, data->Data, data->Length);

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

HRESULT MaxLuminanceEffect::UpdateConstants()
{
    // Update the DPI if it has changed. This allows the effect to scale across different DPIs automatically.
    m_effectContext->GetDpi(&m_dpi, &m_dpi);
    m_constants.dpi = m_dpi;

    return m_drawInfo->SetPixelShaderConstantBuffer(reinterpret_cast<BYTE*>(&m_constants), sizeof(m_constants));
}

IFACEMETHODIMP MaxLuminanceEffect::PrepareForRender(D2D1_CHANGE_TYPE changeType)
{
    return UpdateConstants();
}

// SetGraph is only called when the number of inputs changes. This never happens as we publish this effect
// as a single input effect.
IFACEMETHODIMP MaxLuminanceEffect::SetGraph(_In_ ID2D1TransformGraph* pGraph)
{
    return E_NOTIMPL;
}

// Called to assign a new render info class, which is used to inform D2D on
// how to set the state of the GPU.
IFACEMETHODIMP MaxLuminanceEffect::SetDrawInfo(_In_ ID2D1DrawInfo* pDrawInfo)
{
    m_drawInfo = pDrawInfo;

    return m_drawInfo->SetPixelShader(GUID_MaxLuminancePixelShader);
}

// Calculates the mapping between the output and input rects.
IFACEMETHODIMP MaxLuminanceEffect::MapOutputRectToInputRects(
    _In_ const D2D1_RECT_L* pOutputRect,
    _Out_writes_(inputRectCount) D2D1_RECT_L* pInputRects,
    UINT32 inputRectCount
    ) const
{
    // This effect has exactly 1 input.
    if (inputRectCount != 1)
    {
        return E_INVALIDARG;
    }

    // 1:1 mapping of output to input rect.
    pInputRects[0].left = pOutputRect->left;
    pInputRects[0].top = pOutputRect->top;
    pInputRects[0].right = pOutputRect->right;
    pInputRects[0].bottom = pOutputRect->bottom;

    return S_OK;
}

IFACEMETHODIMP MaxLuminanceEffect::MapInputRectsToOutputRect(
    _In_reads_(inputRectCount) CONST D2D1_RECT_L* pInputRects,
    _In_reads_(inputRectCount) CONST D2D1_RECT_L* pInputOpaqueSubRects,
    UINT32 inputRectCount,
    _Out_ D2D1_RECT_L* pOutputRect,
    _Out_ D2D1_RECT_L* pOutputOpaqueSubRect
    )
{
    // This effect has exactly 1 input.
    if (inputRectCount != 1)
    {
        return E_INVALIDARG;
    }

    // 1:1 mapping of input to output rect.
    *pOutputRect = pInputRects[0];
    m_inputRect = pInputRects[0];

    // This effect always outputs fully opaque pixels.
    *pOutputOpaqueSubRect = pInputRects[0];

    return S_OK;
}

IFACEMETHODIMP MaxLuminanceEffect::MapInvalidRect(
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

IFACEMETHODIMP_(UINT32) MaxLuminanceEffect::GetInputCount() const
{
    return 1;
}

// D2D ensures that that effects are only referenced from one thread at a time.
// To improve performance, we simply increment/decrement our reference count
// rather than use atomic InterlockedIncrement()/InterlockedDecrement() functions.
IFACEMETHODIMP_(ULONG) MaxLuminanceEffect::AddRef()
{
    m_refCount++;
    return m_refCount;
}

IFACEMETHODIMP_(ULONG) MaxLuminanceEffect::Release()
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
// of the MaxLuminance interface, this method simply enables the developer
// to cast a MaxLuminance instance to an ID2D1EffectImpl or IUnknown instance.
IFACEMETHODIMP MaxLuminanceEffect::QueryInterface(
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