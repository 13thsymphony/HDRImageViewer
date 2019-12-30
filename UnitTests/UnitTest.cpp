#include "pch.h"
#include "CppUnitTest.h"

#include "..\HDRImageViewer\ImageLoader.h"
using namespace HDRImageViewer;

using namespace concurrency;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Microsoft::WRL;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;

using Windows::Graphics::Display::AdvancedColorKind;
using Windows::Foundation::Size;

namespace UnitTests
{
#define TESTHR(x) Assert::IsTrue(SUCCEEDED(x), L"Failed HRESULT")

    struct TestInputDefinition
    {
        std::wstring                        filename;
        bool                                useWic; // Either WIC or DirectXTex to decode.
        HDRImageViewer::ImageInfo   info;
        HDRImageViewer::ImageCLL    cllInfo;
    };

    TEST_CLASS(ImageLoaderTests)
    {
    public:
        std::shared_ptr<DX::DeviceResources> m_devRes;

        TEST_METHOD_INITIALIZE(methodName)
        {
			// TODO: Move device resource initialization to test method initialize.
            // m_devRes = std::make_shared<DX::DeviceResources>();
        }

        TEST_METHOD(LoadValidWicImages)
        {
            HRESULT hr = S_OK;

            m_devRes = std::make_shared<DX::DeviceResources>();

            TestInputDefinition definitions[] = {
                // Filename                     |useWIC | bpp |bpc|isfloat|pixelsize    |numICC| ACKind                               |isXbox|valid| maxCLL|medCLL
                { L"Png_BasicSrgbColors_5x5.png", true,  { 24 , 8 , false, Size(5, 5)     , 0, AdvancedColorKind::StandardDynamicRange, false, true }, { 0, 0 } },
                { L"Jpg_ProPhotoIcc.jpg"        , true,  { 24 , 8 , false, Size(102, 68)  , 1, AdvancedColorKind::WideColorGamut      , false, true }, { 0, 0 } },
                { L"Jxr_HdrRuler.jxr"           , true,  { 64 , 16, true , Size(192, 108) , 0, AdvancedColorKind::HighDynamicRange    , false, true }, { 0, 0 } },
                { L"Jxr_HdrScene.jxr"           , true,  { 64 , 16, true , Size(192, 108) , 0, AdvancedColorKind::HighDynamicRange    , false, true }, { 0, 0 } },
                { L"Jxr_HdrWithIcc.jxr"         , true,  { 128, 32, true , Size(51, 34)   , 1, AdvancedColorKind::HighDynamicRange    , false, true }, { 0, 0 } },
                { L"Jxr_HdrXboxOne_1025px.jxr"  , true,  { 64 , 16, true , Size(1025, 576), 0, AdvancedColorKind::HighDynamicRange    , true , true }, { 0, 0 } },
                { L"Tif_16bpcArgbIcc.tif"       , true,  { 48 , 16, false, Size(224, 149) , 1, AdvancedColorKind::HighDynamicRange    , false, true }, { 0, 0 } },
            };

            for (int i = 0; i < ARRAYSIZE(definitions); i++)
            {
                std::wstring path = L"ms-appx:///TestInputs/" + definitions[i].filename;
                auto uri = ref new Windows::Foundation::Uri(ref new Platform::String(path.c_str()));

                create_task(StorageFile::GetFileFromApplicationUriAsync(uri)).then([=](StorageFile^ imageFile) {

                    return create_task(imageFile->OpenAsync(FileAccessMode::Read));

                }).then([=](IRandomAccessStream^ stream) {

                    ComPtr<IStream> iStream;
                    CreateStreamOverRandomAccessStream(stream, IID_PPV_ARGS(&iStream));

                    auto loader = std::make_unique<ImageLoader>(m_devRes);
                    Assert::IsTrue(loader->GetState() == ImageLoaderState::NotInitialized);

                    ImageInfo info = loader->LoadImageFromWic(iStream.Get());
                    Assert::IsTrue(loader->GetState() == ImageLoaderState::LoadingSucceeded);

                    auto imageSource = loader->GetLoadedImage(1.0f);
                    auto imageSource2 = loader->GetLoadedImage(0.5f);

                    Assert::AreEqual(info.bitsPerPixel, definitions[i].info.bitsPerPixel);
                    Assert::AreEqual(info.bitsPerChannel, definitions[i].info.bitsPerChannel);
                    Assert::AreEqual(info.isFloat, definitions[i].info.isFloat);
                    Assert::AreEqual(info.size.Width, definitions[i].info.size.Width);
                    Assert::AreEqual(info.size.Height, definitions[i].info.size.Height);
                    Assert::AreEqual(info.numProfiles, definitions[i].info.numProfiles);
                    Assert::IsTrue(info.imageKind == definitions[i].info.imageKind);
                    Assert::AreEqual(info.forceBT2100ColorSpace, definitions[i].info.forceBT2100ColorSpace);
                    Assert::AreEqual(info.isValid, definitions[i].info.isValid);
                }).then([=](task<void> previousTask) {
                    try
                    {
                        previousTask.get();
                    }
                    catch (Platform::COMException^ e)
                    {
                        Assert::AreEqual(static_cast<int>(S_OK), e->HResult);
                    }
                });
            }
        }
    };
}