#include "pch.h"
#include "CppUnitTest.h"

#include "..\D2DAdvancedColorImages\ImageLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using namespace D2DAdvancedColorImages;

using Windows::Graphics::Display::AdvancedColorKind;
using Windows::Foundation::Size;

namespace UnitTests
{
    struct TestInputDefinition
    {
        std::wstring                        filename;
        bool                                useWic; // Either WIC or DirectXTex to decode.
        D2DAdvancedColorImages::ImageInfo   info;
        D2DAdvancedColorImages::ImageCLL    cllInfo;
    };

    TEST_CLASS(ImageLoaderTests)
    {
    public:
        std::shared_ptr<DX::DeviceResources> m_devRes;

        TEST_METHOD_INITIALIZE(methodName)
        {
            m_devRes = std::make_shared<DX::DeviceResources>();
        }

        TEST_METHOD(LoadJpeg)
        {
            TestInputDefinition definitions[] = {
                // Filename                     |useWIC|  bpp|bpc|isfloat|pixelsize|numProfs|ACKind                             |isXbox|valid| maxCLL|medCLL
                { L"Png_BasicSrgbColors_5x5.png", true,  { 24,  8, false, Size(5, 5), 0, AdvancedColorKind::StandardDynamicRange, false, true }, { 0, 0 } },
                { L"Png_BasicSrgbColors_5x5.png", true,  { 24,  8, false, Size(5, 5), 0, AdvancedColorKind::StandardDynamicRange, false, true }, { 0, 0 } },
            };

            auto loader = std::make_unique<ImageLoader>(m_devRes);
        }
    };
}