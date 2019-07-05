#pragma once

namespace HDRImageViewer
{
    struct ImageInfo
    {
        unsigned int                                    bitsPerPixel;
        unsigned int                                    bitsPerChannel;
        bool                                            isFloat;
        Windows::Foundation::Size                       size;
        unsigned int                                    numProfiles;
        Windows::Graphics::Display::AdvancedColorKind   imageKind;
        bool                                            isXboxHdrScreenshot;
        bool                                            isValid;
        bool                                            isHeif;
    };

    struct ImageCLL
    {
        float   maxNits;
        float   medNits;
    };
}