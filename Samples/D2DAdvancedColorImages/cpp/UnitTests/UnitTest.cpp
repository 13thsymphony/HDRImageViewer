#include "pch.h"
#include "CppUnitTest.h"

#include "..\D2DAdvancedColorImages\ImageLoader.cpp"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using namespace D2DAdvancedColorImages;

namespace UnitTests
{
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
            auto loader = std::make_unique<ImageLoader>(m_devRes);
        }
    };
}