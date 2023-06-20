#include "pch.h"
#include "HDRImageViewerRenderer.h"
#include "Common\DirectXHelper.h"
#include "DirectXTex.h"
#include "ImageExporter.h"
#include "MagicConstants.h"
#include "RenderEffects\SimpleTonemapEffect.h"
#include "DirectXTex\DirectXTexEXR.h"

using namespace DXRenderer;

using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Platform;
using namespace std;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Input;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

HDRImageViewerRenderer::HDRImageViewerRenderer(
    SwapChainPanel^ panel) :
    m_renderEffectKind(RenderEffectKind::None),
    m_zoom(1.0f),
    m_minZoom(1.0f), // Dynamically calculated on window size.
    m_imageOffset(),
    m_pointerPos(),
    m_imageCLL{ -1.0f, -1.0f, false },
    m_exposureAdjust(1.0f),
    m_dispMaxCLLOverride(0.0f),
    m_imageInfo{},
    m_isComputeSupported(false),
    m_enableTargetCpuReadback(false),
    m_constrainGamut(true)
{
    // DeviceResources must be initialized first.
    // TODO: Current architecture does not allow multiple Renderers to share DeviceResources.
    m_deviceResources = std::make_shared<DeviceResources>();
    m_deviceResources->SetSwapChainPanel(panel);

    // Register to be notified if the GPU device is lost or recreated.
    m_deviceResources->RegisterDeviceNotify(this);

    CreateDeviceIndependentResources();
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

HDRImageViewerRenderer::~HDRImageViewerRenderer()
{
    // Deregister device notification.
    m_deviceResources->RegisterDeviceNotify(nullptr);
}

void HDRImageViewerRenderer::CreateDeviceIndependentResources()
{
    auto fact = m_deviceResources->GetD2DFactory();

    // TODO: This instance never does anything as it gets overwritten upon image load.
    m_imageLoader = std::make_unique<ImageLoader>(m_deviceResources, ImageLoaderOptions{});

    // Register the custom render effects.
    IFT(SimpleTonemapEffect::Register(fact));
    IFT(SdrOverlayEffect::Register(fact));
    IFT(LuminanceHeatmapEffect::Register(fact));
    IFT(MaxLuminanceEffect::Register(fact));
    IFT(SphereMapEffect::Register(fact));
}

void HDRImageViewerRenderer::CreateDeviceDependentResources()
{
    // All this app's device-dependent resources also depend on
    // the loaded image, so they are all created in
    // CreateImageDependentResources.
}

void HDRImageViewerRenderer::ReleaseDeviceDependentResources()
{
    m_imageLoader->ReleaseDeviceDependentResources();
}

// Whenever the app window is resized or changes displays, this method is used
// to update the app's sizing and advanced color state.
void HDRImageViewerRenderer::CreateWindowSizeDependentResources()
{
    // Window size changes don't require recomputing image HDR metadata.
    FitImageToWindow(false);
}

/// <summary>
/// Updates rendering parameters, and draws. If CPU readback is enabled, updates the CPU-side render target cache.
/// Always calls Draw() to refresh visual output.
/// </summary>
/// <param name="effect"></param>
/// <param name="exposureAdjustment">
/// Multiplication factor for color values; allows the user to
/// adjust the brightness of the image on an HDR display.</param>
/// <param name="dispMaxCllOverride">0 indicates no override (use the display's actual MaxCLL).</param>
/// <param name="acInfo">If nullptr, assumes a default HDR display.</param>
void HDRImageViewerRenderer::SetRenderOptions(
    RenderEffectKind effect,
    float exposureAdjustment,
    float dispMaxCllOverride,
    AdvancedColorInfo^ acInfo,
    bool constrainGamut
    )
{
    // Renderer state must be restored by the caller if we are using acInfo == null.
    m_dispInfo = acInfo;
    m_renderEffectKind = effect;
    m_exposureAdjust = exposureAdjustment;
    m_dispMaxCLLOverride = dispMaxCllOverride;
    m_constrainGamut = constrainGamut;

    float lum = m_dispInfo ? m_dispInfo->MaxLuminanceInNits : 0.f;

    struct _colors
    {
        float redx, redy, greenx, greeny, bluex, bluey, whitex, whitey;
    };

    // TODO: If using a nullptr acInfo, handle gamut transforms.
    _colors color {};
    if (m_dispInfo)
    {
        color =
        {
            m_dispInfo->RedPrimary.X, m_dispInfo->RedPrimary.Y,
            m_dispInfo->GreenPrimary.X, m_dispInfo->GreenPrimary.Y,
            m_dispInfo->BluePrimary.X, m_dispInfo->BluePrimary.Y,
            m_dispInfo->WhitePoint.X, m_dispInfo->WhitePoint.Y
        };

        UpdateGamutTransforms();
    }

    auto sdrWhite = m_dispInfo ? m_dispInfo->SdrWhiteLevelInNits : D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL;
    auto acKind = m_dispInfo ? m_dispInfo->CurrentAdvancedColorKind : AdvancedColorKind::HighDynamicRange;

    UpdateWhiteLevelScale(m_exposureAdjust, sdrWhite);

    // Adjust the Direct2D effect graph based on RenderEffectKind.
    // Some RenderEffectKind values require us to apply brightness adjustment
    // after the effect as their numerical output is affected by any luminance boost.
    switch (m_renderEffectKind)
    {
    // Effect graph: ImageSource > ColorManagement > [Optional GainMapMerge] > WhiteScale > HDRTonemap > WhiteScale2*
    case RenderEffectKind::HdrTonemap:
        if (acKind != AdvancedColorKind::HighDynamicRange)
        {
            // *Second white scale is needed as an integral part of using the Direct2D HDR
            // tonemapper on SDR/WCG displays to stay within [0, 1] numeric range.
            m_finalOutput = m_sdrWhiteScaleEffect.Get();
        }
        else
        {
            m_finalOutput = m_hdrTonemapEffect.Get();
        }

        m_sdrWhiteScaleEffect->SetInputEffect(0, m_hdrTonemapEffect.Get());
        m_whiteScaleEffect->SetInputEffect(0, m_gainMapMergeEffect.Get());
        break;

    // Effect graph: ImageSource > ColorManagement > [Optional GainMapMerge] > WhiteScale
    case RenderEffectKind::None:
        m_finalOutput = m_whiteScaleEffect.Get();
        m_whiteScaleEffect->SetInputEffect(0, m_gainMapMergeEffect.Get());
        break;

    // Effect graph: ImageSource > ColorManagement > [Optional GainMapMerge] > Heatmap > WhiteScale
    case RenderEffectKind::LuminanceHeatmap:
        m_finalOutput = m_whiteScaleEffect.Get();
        m_whiteScaleEffect->SetInputEffect(0, m_heatmapEffect.Get());
        break;

    // Effect graph: ImageSource > ColorManagement > [Optional GainMapMerge] > MaxLuminance > WhiteScale
    case RenderEffectKind::MaxLuminance:
        m_finalOutput = m_whiteScaleEffect.Get();
        m_whiteScaleEffect->SetInputEffect(0, m_maxLuminanceEffect.Get());

        lum = Clamp(lum, 80.0f, 10000.0f);
        m_maxLuminanceEffect->SetValueByName(L"MaxLuminance", lum);
        break;

    // Effect graph: ImageSource > ColorManagement > [Optional GainMapMerge] > SdrOverlay > WhiteScale
    case RenderEffectKind::SdrOverlay:
        m_finalOutput = m_whiteScaleEffect.Get();
        m_whiteScaleEffect->SetInputEffect(0, m_sdrOverlayEffect.Get());
        break;

    // Effect graph: ImageSource > ColorManagement > [Optional GainMapMerge] > WhiteScale > SphereMap
    case RenderEffectKind::SphereMap:
        m_finalOutput = m_sphereMapEffect.Get();
        m_whiteScaleEffect->SetInputEffect(0, m_gainMapMergeEffect.Get());
        break;

    default:
        throw ref new NotImplementedException();
        break;
    }

    float targetMaxNits = GetBestDispMaxLuminance();

    // Update HDR tonemappers with display information.
    // The custom tonemapper uses mostly the same property definitions as the 1809 Direct2D tonemapper, for simplicity.
    IFT(m_hdrTonemapEffect->SetValue(D2D1_HDRTONEMAP_PROP_OUTPUT_MAX_LUMINANCE, targetMaxNits));

    float maxCLL = m_imageCLL.maxNits != -1.0f ? m_imageCLL.maxNits : sc_DefaultImageMaxCLL;
    maxCLL *= m_exposureAdjust;

    // Very low input max luminance can produce unexpected rendering behavior. Restrict to
    // a reasonable level - the Direct2D tonemapper performs nearly a no-op if input < output max nits.
    maxCLL = max(maxCLL, sc_DefaultSdrDispMaxNits);

    IFT(m_hdrTonemapEffect->SetValue(D2D1_HDRTONEMAP_PROP_INPUT_MAX_LUMINANCE, maxCLL));

    // Don't use the SDR display tone mapper mode as it raises midtones a lot.
    IFT(m_hdrTonemapEffect->SetValue(D2D1_HDRTONEMAP_PROP_DISPLAY_MODE, D2D1_HDRTONEMAP_DISPLAY_MODE_HDR));

    // If an HDR tonemapper is used on an SDR or WCG display, perform additional white level correction.
    if (acKind != AdvancedColorKind::HighDynamicRange)
    {
        // Both the D2D and custom HDR tonemappers output values in scRGB using scene-referred luminance - a typical SDR display will
        // be around numeric range [0.0, 3.0] corresponding to [0, 240 nits]. To encode correctly for an SDR/WCG display
        // output, we must reinterpret the scene-referred input content (80 nits) as display-referred (targetMaxNits).
        // Some HDR images are dimmer than targetMaxNits, in those cases the tonemapper basically passes through.
        IFT(m_sdrWhiteScaleEffect->SetValue(D2D1_WHITELEVELADJUSTMENT_PROP_INPUT_WHITE_LEVEL, D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL));
        IFT(m_sdrWhiteScaleEffect->SetValue(D2D1_WHITELEVELADJUSTMENT_PROP_OUTPUT_WHITE_LEVEL, min(targetMaxNits, maxCLL)));
    }

    // If the gamut map conversion is enabled, insert the 2 color matrix effects needed to perform that operation.
    // What we're doing here is transforming the colors from scRGB colors to panel-relative colors, and clamping 
    // that output to 0-1, then a second effect converts back to rec 709 colorimetry without clipping.
    if (m_constrainGamut)
    {
        m_mapGamutToPanel->SetInputEffect(0, m_finalOutput.Get());
        m_mapGamutToScRGB->SetInputEffect(0, m_mapGamutToPanel.Get());

        m_finalOutput = m_mapGamutToScRGB.Get();
    }

    Draw();

    if (m_enableTargetCpuReadback)
    {
        // Draw the final rendered output.
        ComPtr<ID2D1Image> image;
        IFT(m_finalOutput.As(&image));

        D2D1_SIZE_U size = m_deviceResources->GetD2DTargetBitmap()->GetPixelSize();
        m_renderTargetCpuPixels = ImageExporter::DumpImageToRGBFloat(m_deviceResources.get(), image.Get(), size);
    }
    else
    {
        m_renderTargetCpuPixels.clear();
    }
}

ImageInfo HDRImageViewerRenderer::LoadImageFromWic(_In_ IRandomAccessStream^ imageStream, ImageLoaderOptions options)
{
    ComPtr<IStream> iStream;
    IFT(CreateStreamOverRandomAccessStream(imageStream, IID_PPV_ARGS(&iStream)));

    m_imageLoader = std::make_unique<ImageLoader>(m_deviceResources, options);
    m_imageInfo = m_imageLoader->LoadImageFromWic(iStream.Get());
    return m_imageInfo;
}

ImageInfo HDRImageViewerRenderer::LoadImageFromDirectXTex(String ^ filename, String ^ extension, ImageLoaderOptions options)
{
    m_imageLoader = std::make_unique<ImageLoader>(m_deviceResources, options);
    m_imageInfo = m_imageLoader->LoadImageFromDirectXTex(filename, extension);
    return m_imageInfo;
}

void HDRImageViewerRenderer::ExportImageToSdr(_In_ IRandomAccessStream^ outputStream, Guid wicFormat)
{
    ComPtr<IStream> iStream;
    IFT(CreateStreamOverRandomAccessStream(outputStream, IID_PPV_ARGS(&iStream)));

    ImageExporter::ExportToSdr(m_imageLoader.get(), m_deviceResources.get(), iStream.Get(), wicFormat);
}

// Test only. Exports to DXGI encoded DDS.
void HDRImageViewerRenderer::ExportAsDdsTest(_In_ IRandomAccessStream^ outputStream)
{
    auto wicSource = m_imageLoader->GetWicSourceTest();
    ComPtr<IWICBitmap> bitmap;
    IFT(wicSource->QueryInterface(IID_PPV_ARGS(&bitmap)));

    ComPtr<IStream> iStream;
    IFT(CreateStreamOverRandomAccessStream(outputStream, IID_PPV_ARGS(&iStream)));

    ImageExporter::ExportToDds(bitmap.Get(), iStream.Get(), DXGI_FORMAT_R10G10B10A2_UNORM);
}

/// <summary>
/// Save any supported HDR format as HDR JPEG XR. Not guaranteed to be lossless since we run the
/// full render pipeline.
/// Note: Using new approach for image export where we leverage the renderer itself instead of replicating
/// the render pipeline in ImageExporter.
/// Note: Calls Begin/EndDraw on the context.
/// </summary>
/// <param name="outputStream"></param>
void HDRImageViewerRenderer::ExportImageToJxr(Windows::Storage::Streams::IRandomAccessStream^ outputStream)
{
    // TODO: Keep in sync with any render pipeline changes.

    // Apply temp render pipeline state.
    auto saved_renderEffectKind = m_renderEffectKind;
    auto saved_exposureAdjust = m_exposureAdjust;
    auto saved_dispMaxCLLOverride = m_dispMaxCLLOverride;
    auto saved_constrainGamut = m_constrainGamut;
    auto saved_dispInfo = m_dispInfo;

    // SetRenderOptions sets the member variables and calls Draw().
    // Note the nullptr acInfo to fake an HDR display.
    SetRenderOptions(RenderEffectKind::None, 1.0f, 0.0f, nullptr, false);

    // Apply temp spatial transform state after SetRenderOptions.
    auto saved_zoom = m_zoom;
    auto saved_imageOffset = m_imageOffset;
    auto ctx = m_deviceResources->GetD2DDeviceContext();

    m_zoom = 1.0f;
    m_imageOffset = { 0.0f, 0.0f };
    ctx->SetDpi(96.0f, 96.0f); // Image export always occurs without DPI scaling.

    UpdateImageTransformState();

    ComPtr<IStream> iStream;
    IFT(CreateStreamOverRandomAccessStream(outputStream, IID_PPV_ARGS(&iStream)));

    ComPtr<ID2D1Image> outputImage;
    m_finalOutput->GetOutput(&outputImage);

    ImageExporter::ExportToWic(outputImage.Get(),
        m_imageInfo.pixelSize,
        m_deviceResources.get(),
        iStream.Get(),
        GUID_ContainerFormatWmp);

    // Restore all state.
    m_zoom = saved_zoom;
    m_imageOffset = saved_imageOffset;
    ctx->SetDpi(m_deviceResources->GetDpi(), m_deviceResources->GetDpi());
    UpdateImageTransformState();

    // Call SetRenderOptions last as it calls Draw.
    SetRenderOptions(saved_renderEffectKind,
        saved_exposureAdjust,
        saved_dispMaxCLLOverride,
        saved_dispInfo,
        saved_constrainGamut);
}

// Configures a Direct2D image pipeline, including source, color management, 
// tonemapping, and white level, based on the loaded image. Also responsible for m_imageLoader.
void HDRImageViewerRenderer::CreateImageDependentResources()
{
    // If we just came from device lost/restored, we need to manually re-setup ImageLoader.
    if (m_imageLoader->GetState() == ImageLoaderState::NeedDeviceResources)
    {
        m_imageLoader->CreateDeviceDependentResources();
    }

    auto d2dFactory = m_deviceResources->GetD2DFactory();
    auto context = m_deviceResources->GetD2DDeviceContext();

    // Configure the app's effect pipeline, consisting of a color management effect
    // followed by a tone mapping effect.

    IFT(context->CreateEffect(CLSID_D2D1ColorManagement, &m_colorManagementEffect));
    // The pipeline input is set in UpdateImageTransformState().

    IFT(m_colorManagementEffect->SetValue(
            D2D1_COLORMANAGEMENT_PROP_QUALITY,
            D2D1_COLORMANAGEMENT_QUALITY_BEST));   // Required for floating point and DXGI color space support.

    // The color management effect takes a source color space and a destination color space,
    // and performs the appropriate math to convert images between them.
    IFT(m_colorManagementEffect->SetValue(
            D2D1_COLORMANAGEMENT_PROP_SOURCE_COLOR_CONTEXT,
            m_imageLoader->GetImageColorContext()));

    // Perceptual (default) intent can introduce gamut compression. This is undesirable at the start of an HDR/WCG
    // render pipeline, as any gamut mapping should occur at the output stage.
    IFT(m_colorManagementEffect->SetValue(
        D2D1_COLORMANAGEMENT_PROP_SOURCE_RENDERING_INTENT,
        D2D1_COLORMANAGEMENT_RENDERING_INTENT_RELATIVE_COLORIMETRIC));

    // The destination color space is the render target's (swap chain's) color space. This app uses an
    // FP16 swap chain, which requires the colorspace to be scRGB.
    ComPtr<ID2D1ColorContext1> destColorContext;
    IFT(context->CreateColorContextFromDxgiColorSpace(
            DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709, // scRGB
            &destColorContext));

    IFT(m_colorManagementEffect->SetValue(
            D2D1_COLORMANAGEMENT_PROP_DESTINATION_COLOR_CONTEXT,
            destColorContext.Get()));

    // Next, merge the Apple HDR gainmap with the main image to recover HDR highlights.
    // This occurs after color management to scRGB but before any further stages which rely on HDR pixel data.
    // The parameters of the gainmap are empirically determined:
    // * 50% of the main image resolution
    // * 8-bit grayscale linear gamma luminance data, but is not calibrated to any absolute scale
    // * A value of 0.5f (or 128) is approximately equal to diffuse white in the scene
    // * Naively multiplying the gainmap by the main image in linear RGB approximates the visual effect
    //   in the iOS Photos app.
    
    if (m_imageInfo.hasAppleHdrGainMap == true)
    {
        IFT(context->CreateEffect(CLSID_D2D1GammaTransfer, &m_gainmapLinearEffect));

        // Approximate the linearization step by applying gamma of 1/2.2.
        m_gainmapLinearEffect->SetValue(D2D1_GAMMATRANSFER_PROP_RED_EXPONENT, 1.f / 2.2f);
        m_gainmapLinearEffect->SetValue(D2D1_GAMMATRANSFER_PROP_GREEN_EXPONENT, 1.f / 2.2f);
        m_gainmapLinearEffect->SetValue(D2D1_GAMMATRANSFER_PROP_BLUE_EXPONENT, 1.f / 2.2f);

        // Gain map input is set in UpdateImageTransformState().

        // This is treated as an HDR image, so we must use scene-referred luminance and read the system SDR white level.
        IFT(context->CreateEffect(CLSID_D2D1WhiteLevelAdjustment, &m_gainmapRefWhiteEffect));
        m_gainmapRefWhiteEffect->SetInputEffect(0, m_gainmapLinearEffect.Get());

        auto sdrWhite = m_dispInfo ? m_dispInfo->SdrWhiteLevelInNits : D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL;
        IFT(m_gainmapRefWhiteEffect->SetValue(D2D1_WHITELEVELADJUSTMENT_PROP_INPUT_WHITE_LEVEL, sdrWhite));
        IFT(m_gainmapRefWhiteEffect->SetValue(D2D1_WHITELEVELADJUSTMENT_PROP_OUTPUT_WHITE_LEVEL, D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL));

        IFT(context->CreateEffect(CLSID_D2D1ArithmeticComposite, &m_gainMapMergeEffect));

        m_gainMapMergeEffect->SetInputEffect(0, m_colorManagementEffect.Get());
        m_gainMapMergeEffect->SetInputEffect(1, m_gainmapRefWhiteEffect.Get());

        // Coefficients A, B, C, D: Output = A*source*dest + B*source + C*dest + D.
        m_gainMapMergeEffect->SetValue(D2D1_ARITHMETICCOMPOSITE_PROP_COEFFICIENTS, D2D1::Vector4F(2.f, 0.0f, 0.0f, 0.0f));
    }
    else
    {
        IFT(m_colorManagementEffect.CopyTo(&m_gainMapMergeEffect)); // Pass-through.
    }

    // White level scale is used to multiply the color values in the image; this allows the user
    // to adjust the brightness of the image on an HDR display.
    IFT(context->CreateEffect(CLSID_D2D1ColorMatrix, &m_whiteScaleEffect));

    // Input to white level scale may be modified in SetRenderOptions.
    m_whiteScaleEffect->SetInputEffect(0, m_gainMapMergeEffect.Get());

    // Set the actual matrix in SetRenderOptions.

    // Instantiate and cache all of the tonemapping/render effects.
    // Some effects are implemented as Direct2D custom effects; see the RenderEffects filter in the
    // Solution Explorer.

    GUID tonemapper = {};
    if (CheckPlatformSupport(Win1809))
    {
        // HDR tonemapper and white level adjust are only available in 1809 and above.
        tonemapper = CLSID_D2D1HdrToneMap;
    }
    else
    {
        // TODO: The custom tonemapper is never used in product code, only for testing.
        tonemapper = CLSID_CustomSimpleTonemapEffect;
    }

    IFT(context->CreateEffect(tonemapper, &m_hdrTonemapEffect));
    IFT(context->CreateEffect(CLSID_D2D1WhiteLevelAdjustment, &m_sdrWhiteScaleEffect));
    IFT(context->CreateEffect(CLSID_CustomSdrOverlayEffect, &m_sdrOverlayEffect));
    IFT(context->CreateEffect(CLSID_CustomLuminanceHeatmapEffect, &m_heatmapEffect));
    IFT(context->CreateEffect(CLSID_CustomMaxLuminanceEffect, &m_maxLuminanceEffect));
    IFT(context->CreateEffect(CLSID_CustomSphereMapEffect, &m_sphereMapEffect));

    IFT(context->CreateEffect(CLSID_D2D1ColorMatrix, &m_mapGamutToPanel));
    IFT(context->CreateEffect(CLSID_D2D1ColorMatrix, &m_mapGamutToScRGB));

    // TEST: border effect to remove seam at the boundary of the image (subpixel sampling)
    // Unclear if we can force D2D_BORDER_MODE_HARD somewhere to avoid the seam.
    ComPtr<ID2D1Effect> border;
    IFT(context->CreateEffect(CLSID_D2D1Border, &border));

    border->SetValue(D2D1_BORDER_PROP_EDGE_MODE_X, D2D1_BORDER_EDGE_MODE_WRAP);
    border->SetValue(D2D1_BORDER_PROP_EDGE_MODE_Y, D2D1_BORDER_EDGE_MODE_WRAP);
    border->SetInputEffect(0, m_whiteScaleEffect.Get());

    m_hdrTonemapEffect->SetInputEffect(0, m_whiteScaleEffect.Get());
    m_sphereMapEffect->SetInputEffect(0, border.Get());

    // SphereMap needs to know the pixel size of the image.
    IFT(m_sphereMapEffect->SetValue(
            SPHEREMAP_PROP_SCENESIZE,
            D2D1::SizeF(m_imageInfo.pixelSize.Width, m_imageInfo.pixelSize.Height)));

    // For the following effects, we want white level scale to be applied after
    // tonemapping (otherwise brightness adjustments will affect numerical values).
    m_heatmapEffect->SetInputEffect(0, m_gainMapMergeEffect.Get());
    m_maxLuminanceEffect->SetInputEffect(0, m_gainMapMergeEffect.Get());
    m_sdrOverlayEffect->SetInputEffect(0, m_gainMapMergeEffect.Get());

    // The remainder of the Direct2D effect graph is constructed in SetRenderOptions based on the
    // selected RenderEffectKind.

    CreateHistogramResources();
}

// Perform histogram pipeline setup; this should occur as part of image resource creation.
// Histogram results in no visual output but is used to calculate HDR metadata for the image.
void HDRImageViewerRenderer::CreateHistogramResources()
{
    auto context = m_deviceResources->GetD2DDeviceContext();

    // We need to preprocess the image data before running the histogram.
    // 1. Spatial downscale to reduce the amount of processing and limit intermediate texture size.
    IFT(context->CreateEffect(CLSID_D2D1Scale, &m_histogramPrescale));

    // Cap histogram pixel size to 2048 along the larger dimension.
    float pixScale = min(0.5f, 2048.0f / max(m_imageInfo.pixelSize.Width, m_imageInfo.pixelSize.Height));

    IFT(m_histogramPrescale->SetValue(D2D1_SCALE_PROP_SCALE, D2D1::Vector2F(pixScale, pixScale)));
    IFT(m_histogramPrescale->SetValue(
        D2D1_SCALE_PROP_INTERPOLATION_MODE,
        D2D1_SCALE_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC));

    // The right place to compute HDR metadata is after color management to the
    // image's native colorspace but before any tonemapping or adjustments for the display.
    m_histogramPrescale->SetInputEffect(0, m_gainMapMergeEffect.Get());

    // 2. Convert scRGB data into luminance (nits).
    // 3. Normalize color values. Histogram operates on [0-1] numeric range,
    //    while FP16 can go up to 65504 (5+ million nits).
    // Both steps are performed in the same color matrix.
    ComPtr<ID2D1Effect> histogramMatrix;
    IFT(context->CreateEffect(CLSID_D2D1ColorMatrix, &histogramMatrix));

    histogramMatrix->SetInputEffect(0, m_histogramPrescale.Get());

    float scale = sc_histMaxNits / D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL;

    D2D1_MATRIX_5X4_F rgbtoYnorm = D2D1::Matrix5x4F(
        0.2126f / scale, 0, 0, 0,
        0.7152f / scale, 0, 0, 0,
        0.0722f / scale, 0, 0, 0,
        0              , 0, 0, 1,
        0              , 0, 0, 0);
    // 1st column: [R] output, contains normalized Y (CIEXYZ).
    // 2nd column: [G] output, unused.
    // 3rd column: [B] output, unused.
    // 4th column: [A] output, alpha passthrough.
    // We explicitly calculate Y; this deviates from the CEA 861.3 definition of MaxCLL
    // which approximates luminance with max(R, G, B).

    IFT(histogramMatrix->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, rgbtoYnorm));

    // 4. Apply a gamma to allocate more histogram bins to lower luminance levels.
    ComPtr<ID2D1Effect> histogramGamma;
    IFT(context->CreateEffect(CLSID_D2D1GammaTransfer, &histogramGamma));

    histogramGamma->SetInputEffect(0, histogramMatrix.Get());

    // Gamma function offers an acceptable tradeoff between simplicity and efficient bin allocation.
    // A more sophisticated pipeline would use a more perceptually linear function than gamma.
    IFT(histogramGamma->SetValue(D2D1_GAMMATRANSFER_PROP_RED_EXPONENT, sc_histGamma));
    // All other channels are passthrough.
    IFT(histogramGamma->SetValue(D2D1_GAMMATRANSFER_PROP_GREEN_DISABLE, TRUE));
    IFT(histogramGamma->SetValue(D2D1_GAMMATRANSFER_PROP_BLUE_DISABLE, TRUE));
    IFT(histogramGamma->SetValue(D2D1_GAMMATRANSFER_PROP_ALPHA_DISABLE, TRUE));

    // 5. Finally, the histogram itself.
    HRESULT hr = context->CreateEffect(CLSID_D2D1Histogram, &m_histogramEffect);
    
    if (hr == D2DERR_INSUFFICIENT_DEVICE_CAPABILITIES)
    {
        // The GPU doesn't support compute shaders and we can't run histogram on it.
        m_isComputeSupported = false;
    }
    else
    {
        IFT(hr);
        m_isComputeSupported = true;

        IFT(m_histogramEffect->SetValue(D2D1_HISTOGRAM_PROP_NUM_BINS, sc_histNumBins));

        m_histogramEffect->SetInputEffect(0, histogramGamma.Get());
    }
}

void HDRImageViewerRenderer::ReleaseImageDependentResources()
{
    // TODO: This method is only called during device lost. In that situation,
    // m_imageLoader should not be reset. Confirm this is the only case we want to call this.

    m_loadedImage.Reset();
    m_loadedGainMap.Reset();
    m_gainmapLinearEffect.Reset();
    m_gainmapRefWhiteEffect.Reset();
    m_gainMapMergeEffect.Reset();
    m_colorManagementEffect.Reset();
    m_whiteScaleEffect.Reset();
    m_sdrWhiteScaleEffect.Reset();
    m_hdrTonemapEffect.Reset();
    m_sdrOverlayEffect.Reset();
    m_heatmapEffect.Reset();
    m_maxLuminanceEffect.Reset();
    m_histogramPrescale.Reset();
    m_histogramEffect.Reset();
    m_sphereMapEffect.Reset();
    m_mapGamutToPanel.Reset();
    m_mapGamutToScRGB.Reset();
    m_finalOutput.Reset();
}

/// <summary>
/// Sets whether to enable features that are dependent on having a CPU-cached copy of the render target.
/// Currently mainly used to analyze color values of the rendered output. CPU-cached copy is updated
/// with each call to SetRenderOptions().
/// </summary>
/// <param name="value">Whether support should be enabled or disabled.</param>
void HDRImageViewerRenderer::SetTargetCpuReadbackSupport(bool value)
{
    m_enableTargetCpuReadback = value;

    SetRenderOptions(m_renderEffectKind, m_exposureAdjust, 0.0f, m_dispInfo, m_constrainGamut);
}

/// <summary>
/// Returns the scRGB color value at the position in the render target.
/// </summary>
/// <param name="point">Position in the render target in DIPs (device independent pixels).</param>
/// <returns>If unsupported, all values are set to -1.0f.</returns>
Windows::Foundation::Numerics::float4 HDRImageViewerRenderer::GetPixelColorValue(Point point)
{
    auto color = Windows::Foundation::Numerics::float4(-1.0f);

    if (m_enableTargetCpuReadback)
    {
        auto targetSize = m_deviceResources->GetOutputSize();
        int offset = static_cast<int>(targetSize.Width) * static_cast<int>(point.Y) + static_cast<int>(point.X) * 3; // Channels per pixel.

        color.x = m_renderTargetCpuPixels.at(offset);
        color.y = m_renderTargetCpuPixels.at(offset + 1);
        color.z = m_renderTargetCpuPixels.at(offset + 2);
        color.w = 1.0f;
    }

    return color;
}

void HDRImageViewerRenderer::UpdateManipulationState(_In_ ManipulationUpdatedEventArgs^ args)
{
    Point position = args->Position;
    Point positionDelta = args->Delta.Translation;
    float zoomDelta = args->Delta.Scale;

    if (m_renderEffectKind == RenderEffectKind::SphereMap)
    {
        // For sphere map, panning and zooming is implemented in the effect.
        m_pointerPos.x += positionDelta.X;
        m_pointerPos.y += positionDelta.Y;

        D2D1_SIZE_F targetSize = m_deviceResources->GetD2DDeviceContext()->GetSize();

        // Normalize panning position to pixel dimensions of render target.
        auto x = m_pointerPos.x / targetSize.width;
        auto y = m_pointerPos.y / targetSize.height;

        IFT(m_sphereMapEffect->SetValue(SPHEREMAP_PROP_CENTER, D2D1::Point2F(x, y)));

        m_zoom *= zoomDelta;
        m_zoom = Clamp(m_zoom, sc_MinZoomSphereMap, sc_MaxZoom);

        IFT(m_sphereMapEffect->SetValue(SPHEREMAP_PROP_ZOOM, m_zoom));
    }
    else
    {
        // Normal image pan/zoom for all other render effects.
        m_imageOffset.x += positionDelta.X;
        m_imageOffset.y += positionDelta.Y;

        // We want to have any zoom operation be "centered" around the pointer position, which
        // requires recalculating the view position based on the new zoom and pointer position.
        // Step 1: Calculate the absolute pointer position (image position).
        D2D1_POINT_2F pointerAbsolutePosition = D2D1::Point2F(
            (m_imageOffset.x - position.X) / m_zoom,
            (m_imageOffset.y - position.Y) / m_zoom);

        // Step 2: Apply the zoom; do not allow user to go beyond max zoom level.
        m_zoom *= zoomDelta;
        m_zoom = min(m_zoom, sc_MaxZoom);

        // Step 3: Adjust the view position based on the new m_zoom value.
        m_imageOffset.x = pointerAbsolutePosition.x * m_zoom + position.X;
        m_imageOffset.y = pointerAbsolutePosition.y * m_zoom + position.Y;

        // Step 4: Clamp the translation to the window bounds.
        Size panelSize = m_deviceResources->GetLogicalSize();
        m_imageOffset.x = Clamp(m_imageOffset.x, panelSize.Width - m_imageInfo.pixelSize.Width * m_zoom, 0);
        m_imageOffset.y = Clamp(m_imageOffset.y, panelSize.Height - m_imageInfo.pixelSize.Height * m_zoom, 0);

        UpdateImageTransformState();
    }

    Draw();
}

// Overrides any pan/zoom state set by the user to fit image to the window size.
// Returns the computed content light level (CLL) of the image in nits.
// Recomputing the HDR metadata is only needed when loading a new image.
ImageCLL HDRImageViewerRenderer::FitImageToWindow(bool computeMetadata)
{
    Size panelSize = m_deviceResources->GetLogicalSize();

    // TODO: Root cause why this method is sometimes called before the below prereqs are ready.
    if (m_imageLoader != nullptr && panelSize.Width != 0 && panelSize.Height != 0 &&
        m_imageLoader->GetState() == ImageLoaderState::LoadingSucceeded)
    {
        // Set image to be letterboxed in the window, up to the max allowed scale factor.
        float letterboxZoom = min(
            panelSize.Width / m_imageInfo.pixelSize.Width,
            panelSize.Height / m_imageInfo.pixelSize.Height);

        m_zoom = min(sc_MaxZoom, letterboxZoom);

        // SphereMap needs to know the pixel size of the image.
        IFT(m_sphereMapEffect->SetValue(
                SPHEREMAP_PROP_SCENESIZE,
                D2D1::SizeF(m_imageInfo.pixelSize.Width * m_zoom, m_imageInfo.pixelSize.Height * m_zoom)));

        // Center the image.
        m_imageOffset = D2D1::Point2F(
            (panelSize.Width - (m_imageInfo.pixelSize.Width * m_zoom)) / 2.0f,
            (panelSize.Height - (m_imageInfo.pixelSize.Height * m_zoom)) / 2.0f
        );

        UpdateImageTransformState();

        if (computeMetadata)
        {
            // HDR metadata is supposed to be independent of any rendering options, but
            // we can't compute it until the full effect graph is hooked up, which is here.
            ComputeHdrMetadata();
        }
    }

    return m_imageCLL;
}

// Scale the (linear gamma) brightness/luminance of the image. This is typically used for two reasons:
// 1) When connected to an HDR display, the OS renders SDR content (e.g. 8888 UNORM) at
// a user configurable white level; this typically is around 200-300 nits. It is the responsibility
// of an advanced color app (e.g. FP16 scRGB) to emulate the OS-implemented SDR white level adjustment,
// BUT only for non-HDR content (SDR or WCG).
// 2) Users may want to adjust the exposure of their image to personal preference, typically most useful
// when viewing HDR content on HDR displays.
void HDRImageViewerRenderer::UpdateWhiteLevelScale(float brightnessAdjustment, float sdrWhiteLevel)
{
    float scale = 1.0f;

    switch (m_imageInfo.imageKind)
    {
    case AdvancedColorKind::HighDynamicRange:
        // HDR gainmaps are output-referred and do need to be compensated by SdrWhiteLevel.
        if (m_imageInfo.hasAppleHdrGainMap == true)
        {
            scale = sdrWhiteLevel / D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL;
        }
        else
        {
            // Scene-referred luminance content should not be compensated by the SdrWhiteLevel parameter.
            // Most HDR images fall into this category.
            scale = 1.0f;
        }
        break;

    case AdvancedColorKind::StandardDynamicRange:
    case AdvancedColorKind::WideColorGamut:
    default:
        scale = sdrWhiteLevel / D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL;
        break;
    }

    // The user may want to manually adjust brightness specifically for this image, on top of any
    // white level adjustment for SDR/WCG content. Brightness adjustment using a linear gamma scale
    // is mainly useful for HDR displays, but can be useful for HDR content tonemapped to an SDR/WCG display.
    scale *= brightnessAdjustment;

    // SDR white level scaling is performing by multiplying RGB color values in linear gamma.
    // We implement this with a Direct2D matrix effect.
    D2D1_MATRIX_5X4_F matrix = D2D1::Matrix5x4F(
        scale, 0, 0, 0,  // [R] Multiply each color channel
        0, scale, 0, 0,  // [G] by the scale factor in 
        0, 0, scale, 0,  // [B] linear gamma space.
        0, 0, 0    , 1,  // [A] Preserve alpha values.
        0, 0, 0    , 0); //     No offset.

    IFT(m_whiteScaleEffect->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, matrix));
}

D2D1_MATRIX_5X4_F MatrixToD2D(Matrix m)
{
    return D2D1::Matrix5x4F(
        (float)m.M[0], (float)m.M[1], (float)m.M[2], 0,
        (float)m.M[3], (float)m.M[4], (float)m.M[5], 0,
        (float)m.M[6], (float)m.M[7], (float)m.M[8], 0,
                    0,             0,             0, 1,
                    0,             0,             0, 0 );

}

// If we need to constrain the gamut of the output to specified colorimetry, calculate and set the matrices
void HDRImageViewerRenderer::UpdateGamutTransforms()
{
    // Clamping the colors in panel space should have the effect of a colorimetric clip
    IFT(m_mapGamutToPanel->SetValue(D2D1_COLORMATRIX_PROP_CLAMP_OUTPUT, TRUE));
    IFT(m_mapGamutToScRGB->SetValue(D2D1_COLORMATRIX_PROP_CLAMP_OUTPUT, FALSE));

    auto M709 = Matrix(3, 3);
    auto XYZDisplay = Matrix(3, 3);
    auto WhiteDisplay = Matrix(1, 3);
    auto MDisplay = Matrix(3, 3);

    M709.M = {
        0.4124564, 0.3575761, 0.1804375,
        0.2126729, 0.7151522, 0.0721750,
        0.0193339, 0.1191920, 0.9503041
    };

    XYZDisplay.M[0] = m_dispInfo->RedPrimary.X / m_dispInfo->RedPrimary.Y;
    XYZDisplay.M[1] = m_dispInfo->GreenPrimary.X / m_dispInfo->GreenPrimary.Y;
    XYZDisplay.M[2] = m_dispInfo->BluePrimary.X / m_dispInfo->BluePrimary.Y;
    XYZDisplay.M[3] = 1.f;
    XYZDisplay.M[4] = 1.f;
    XYZDisplay.M[5] = 1.f;
    XYZDisplay.M[6] = (1.f - m_dispInfo->RedPrimary.X   - m_dispInfo->RedPrimary.Y)   / m_dispInfo->RedPrimary.Y;
    XYZDisplay.M[7] = (1.f - m_dispInfo->GreenPrimary.X - m_dispInfo->GreenPrimary.Y) / m_dispInfo->GreenPrimary.Y;
    XYZDisplay.M[8] = (1.f - m_dispInfo->BluePrimary.X  - m_dispInfo->BluePrimary.Y)  / m_dispInfo->BluePrimary.Y;

    WhiteDisplay.M[0] = m_dispInfo->WhitePoint.X / m_dispInfo->WhitePoint.Y;
    WhiteDisplay.M[1] = 1.f;
    WhiteDisplay.M[2] = (1.f - m_dispInfo->WhitePoint.X - m_dispInfo->WhitePoint.Y) / m_dispInfo->WhitePoint.Y;

    auto S = XYZDisplay.Invert() * WhiteDisplay;

    MDisplay.M[0] = S.M[0] * XYZDisplay.M[0];
    MDisplay.M[1] = S.M[1] * XYZDisplay.M[1];
    MDisplay.M[2] = S.M[2] * XYZDisplay.M[2];
    MDisplay.M[3] = S.M[0] * XYZDisplay.M[3];
    MDisplay.M[4] = S.M[1] * XYZDisplay.M[4];
    MDisplay.M[5] = S.M[2] * XYZDisplay.M[5];
    MDisplay.M[6] = S.M[0] * XYZDisplay.M[6];
    MDisplay.M[7] = S.M[1] * XYZDisplay.M[7];
    MDisplay.M[8] = S.M[2] * XYZDisplay.M[8];

    auto transform = MDisplay.Invert() * M709;

    auto gamutToPanel = MatrixToD2D(transform);
    m_mapGamutToPanel->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, gamutToPanel);

    auto panelToScRGB = MatrixToD2D(transform.Invert());
    m_mapGamutToScRGB->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, panelToScRGB);
}

// Call this after updating any spatial transform state to regenerate the effect graph.
void HDRImageViewerRenderer::UpdateImageTransformState()
{
    if (m_imageLoader->GetState() == ImageLoaderState::LoadingSucceeded)
    {
        // Set the new image as the new source to the effect pipeline.
        m_loadedImage = m_imageLoader->GetLoadedImage(m_zoom, false);
        m_colorManagementEffect->SetInput(0, m_loadedImage.Get());

        if (m_imageInfo.hasAppleHdrGainMap == true)
        {
            m_loadedGainMap = m_imageLoader->GetLoadedImage(m_zoom, true);
            m_gainmapLinearEffect->SetInput(0, m_loadedGainMap.Get());
        }
    }
}

// Uses a histogram to compute a modified version of maximum content light level/ST.2086 MaxCLL
// and average content light level.
// Performs Begin/EndDraw on the D2D context.
void HDRImageViewerRenderer::ComputeHdrMetadata()
{
    // Initialize with a sentinel value.
    m_imageCLL = { -1.0f, -1.0f, false };

    // HDR metadata is not meaningful for SDR or WCG images.
    if ((!m_isComputeSupported) ||
        (m_imageInfo.imageKind != AdvancedColorKind::HighDynamicRange))
    {
        return;
    }

    // MaxCLL is nominally calculated for the single brightest pixel in a frame.
    // But we take a slightly more conservative definition that takes the 99.9th percentile
    // to account for extreme outliers in the image.
    // BUG#58: Small images (possibly under 4K) appear to trigger a lot of spurious, tiny (~1E-5) histogram
    // buckets which can incorrectly trigger the MaxCLL detection. Lowering this threshold as a workaround.
    float maxCLLPercent = 0.999f;

    auto ctx = m_deviceResources->GetD2DDeviceContext();

    // Histogram rendering should always occur without DPI scaling
    ctx->SetDpi(96.0f, 96.0f);

    ctx->BeginDraw();

    ctx->DrawImage(m_histogramEffect.Get());

    // Depending on the resolution, DPI, Windows size etc,
    // EndDraw() might return the "D1225: Tile Too Small" error.
    // In that case, we want to continue, even if we won't have the histogram data.
    try
    {
        HRESULT hr = ctx->EndDraw();
        // We ignore D2DERR_RECREATE_TARGET here. This error indicates that the device
        // is lost. It will be handled during the next call to Present.
        if (FAILED(hr) && hr != D2DERR_RECREATE_TARGET)
        {
            throw Platform::Exception::CreateException(hr);
        }
    }
    catch (...) { }

    ctx->SetDpi(m_deviceResources->GetDpi(), m_deviceResources->GetDpi());

    float *histogramData = new float[sc_histNumBins];
    IFT(m_histogramEffect->GetValue(D2D1_HISTOGRAM_PROP_HISTOGRAM_OUTPUT,
            reinterpret_cast<BYTE*>(histogramData),
            sc_histNumBins * sizeof(float)
            )
        );

    unsigned int maxCLLbin = sc_histNumBins - 1;
    unsigned int medCLLbin = 0;
    float runningSum = 0.0f; // Cumulative sum of values in histogram is 1.0.

    // BUG#58: Nominally, we iterate starting from sc_histNumBins - 1. But sometimes
    // I see spurious (nondeterminsistic) histogram results where the last bucket (~1 million nits)
    // reports a large pixel count. Workaround by ignoring the last bucket.
    for (int i = sc_histNumBins - 2; i >= 0; i--)
    {
        runningSum += histogramData[i];

        // Note the inequality (<) is the opposite of the next if block.
        if (runningSum < 1.0f - maxCLLPercent)
        {
            maxCLLbin = i;
        }

        if (runningSum > 0.5f)
        {
            // Some test patterns have a majority of pixels = 0 nits, so med = 0 is allowed.
            medCLLbin = i;
            break;
        }
    }

    float binNormMax = static_cast<float>(maxCLLbin) / static_cast<float>(sc_histNumBins);
    m_imageCLL.maxNits = powf(binNormMax, 1 / sc_histGamma) * sc_histMaxNits;

    float binNormMed = static_cast<float>(medCLLbin) / static_cast<float>(sc_histNumBins);
    m_imageCLL.medianNits = powf(binNormMed, 1 / sc_histGamma) * sc_histMaxNits;

    // Some drivers have a bug where histogram will always return 0. Or some images are pure black.
    // Treat these cases as unknown.
    if (m_imageCLL.maxNits == 0.0f)
    {
        m_imageCLL = { -1.0f, -1.0f };
    }

    // Certain HDR image types use recovered luminance and therefore are display/output-referred.
    // You can't interpret the histogram for these images as physical nits; they are only useful
    // to understand relative intensity.
    if (m_imageInfo.hasAppleHdrGainMap == true)
    {
        m_imageCLL.isSceneReferred = false;
    }
    else
    {
        m_imageCLL.isSceneReferred = true;
    }

    // HDR metadata computation is completed before the app rendering options are known, so don't
    // attempt to draw yet.
}

// Set HDR10 metadata to allow HDR displays to optimize behavior based on our content.
void HDRImageViewerRenderer::EmitHdrMetadata()
{
    // PC apps generally should not use HDR ST.2086 metadata.
    return;
}

// If AdvancedColorInfo does not have valid data, picks an appropriate default value,
// or the manually overridden value.
float HDRImageViewerRenderer::GetBestDispMaxLuminance()
{
    float val = m_dispInfo ? m_dispInfo->MaxLuminanceInNits : 0.0f;
    auto acKind = m_dispInfo ? m_dispInfo->CurrentAdvancedColorKind : AdvancedColorKind::HighDynamicRange;

    if (m_dispMaxCLLOverride != 0.0f)
    {
        val = m_dispMaxCLLOverride;
    }

    if (val == 0.0f)
    {
        if (acKind == AdvancedColorKind::HighDynamicRange)
        {
            // HDR TVs generally don't report metadata, but monitors do.
            val = sc_DefaultHdrDispMaxNits;
        }
        else
        {
            // Almost no SDR displays report HDR metadata. WCG displays generally should report HDR metadata.
            // We assume both SDR and WCG displays have similar peak luminances and use the same constants.
            val = sc_DefaultSdrDispMaxNits;
        }
    }

    return val;
}

// Renders the loaded image with user-specified options.
void HDRImageViewerRenderer::Draw()
{
    auto d2dContext = m_deviceResources->GetD2DDeviceContext();

    d2dContext->BeginDraw();

    d2dContext->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    d2dContext->SetTransform(m_deviceResources->GetOrientationTransform2D());

    if (m_loadedImage)
    {
        d2dContext->DrawImage(m_finalOutput.Get(), m_imageOffset);

        EmitHdrMetadata();
    }

    // We ignore D2DERR_RECREATE_TARGET here. This error indicates that the device
    // is lost. It will be handled during the next call to Present.
    HRESULT hr = d2dContext->EndDraw();
    if (hr != D2DERR_RECREATE_TARGET)
    {
        IFT(hr);
    }

    m_deviceResources->Present();
}

// Notifies renderers that device resources need to be released.
void HDRImageViewerRenderer::OnDeviceLost()
{
    ReleaseImageDependentResources();
    ReleaseDeviceDependentResources();
}

// Notifies renderers that device resources may now be recreated.
void HDRImageViewerRenderer::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateImageDependentResources();
    CreateWindowSizeDependentResources();

    SetRenderOptions(m_renderEffectKind, m_exposureAdjust, m_dispMaxCLLOverride, m_dispInfo, m_constrainGamut);

    Draw();
}