#pragma once

namespace DXRenderer
{
    public value struct ImageInfo
    {
        unsigned int                                    bitsPerPixel;
        unsigned int                                    bitsPerChannel;
        bool                                            isFloat;
        Windows::Foundation::Size                       size;
        unsigned int                                    numProfiles;
        Windows::Graphics::Display::AdvancedColorKind   imageKind;
        bool                                            forceBT2100ColorSpace;
        bool                                            isValid;
        bool                                            isHeif;
    };

    public value struct ImageCLL
    {
        float   maxNits;
        float   medNits;
    };
}