#pragma once

namespace DXRenderer
{
    public value struct ImageInfo
    {
        unsigned int                                    bitsPerPixel;
        unsigned int                                    bitsPerChannel;
        bool                                            isFloat;
        Windows::Foundation::Size                       pixelSize;
        unsigned int                                    numProfiles;
        Windows::Graphics::Display::AdvancedColorKind   imageKind;
        bool                                            forceBT2100ColorSpace;
        bool                                            isValid;
        bool                                            isHeif;
        bool                                            hasAppleHdrGainMap;
        Windows::Foundation::Size                       gainMapPixelSize;
        bool                                            overridenColorProfile;
    };

    public value struct ImageCLL
    {
        float   maxNits;
        float   medianNits;
        bool    isSceneReferred; // If False, the CLL values are not calibrated to actual nits
                                 // should only be used to understand relative intensity of the image.
    };
}