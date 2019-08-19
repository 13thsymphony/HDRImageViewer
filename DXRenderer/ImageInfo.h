#pragma once

namespace HDRImageViewer
{
    public value struct ImageInfo
    {
        unsigned int                                    bitsPerPixel;
        unsigned int                                    bitsPerChannel;
        bool                                            isFloat;
        Windows::Foundation::Size                       size;
        unsigned int                                    numProfiles;
        Windows::Graphics::Display::AdvancedColorKind   imageKind;
        bool                                            isXboxHdrScreenshot;
        bool                                            isValid;
    };

    public value struct ImageCLL
    {
        float   maxNits;
        float   medNits;
    };
}