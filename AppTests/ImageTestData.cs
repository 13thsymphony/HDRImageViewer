using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Storage;
using Windows.Graphics.Display;

using DXRenderer;


namespace AppTests
{
    public struct ImageTestDefinition
    {
        public string                  filename; // Relative to the root of the TestInputs directory.
        public bool                    useWic; // Either WIC or DirectXTex to decode.
        public ImageInfo    imageInfo;
        public ImageCLL     imageCLL;
    }

    public static class ImageTestStatics
    {
        public static List<ImageTestDefinition> TestDefinitions = new List<ImageTestDefinition>
        {
            // This is really verbose...
            new ImageTestDefinition { filename = "Png_BasicSrgbColors_5x5.png", useWic = true, imageInfo = new ImageInfo { bitsPerPixel = 24 , bitsPerChannel = 8 , isFloat = false, size = new Size(5, 5)     , numProfiles = 0, imageKind = AdvancedColorKind.StandardDynamicRange, forceBT2100ColorSpace = false, isValid = true, isHeif = false }, imageCLL = new ImageCLL { maxNits = 0, medNits = 0 } },
            new ImageTestDefinition { filename = "Jpg_ProPhotoIcc.jpg"        , useWic = true, imageInfo = new ImageInfo { bitsPerPixel = 24 , bitsPerChannel = 8 , isFloat = false, size = new Size(102, 68)  , numProfiles = 1, imageKind = AdvancedColorKind.WideColorGamut      , forceBT2100ColorSpace = false, isValid = true, isHeif = false }, imageCLL = new ImageCLL { maxNits = 0, medNits = 0 } },
            new ImageTestDefinition { filename = "Jxr_HdrRuler.jxr"           , useWic = true, imageInfo = new ImageInfo { bitsPerPixel = 64 , bitsPerChannel = 16, isFloat = true , size = new Size(192, 108) , numProfiles = 0, imageKind = AdvancedColorKind.HighDynamicRange    , forceBT2100ColorSpace = false, isValid = true, isHeif = false }, imageCLL = new ImageCLL { maxNits = 0, medNits = 0 } },
            new ImageTestDefinition { filename = "Jxr_HdrScene.jxr"           , useWic = true, imageInfo = new ImageInfo { bitsPerPixel = 64 , bitsPerChannel = 16, isFloat = true , size = new Size(192, 108) , numProfiles = 0, imageKind = AdvancedColorKind.HighDynamicRange    , forceBT2100ColorSpace = false, isValid = true, isHeif = false }, imageCLL = new ImageCLL { maxNits = 0, medNits = 0 } },
            new ImageTestDefinition { filename = "Jxr_HdrWithIcc.jxr"         , useWic = true, imageInfo = new ImageInfo { bitsPerPixel = 128, bitsPerChannel = 32, isFloat = true , size = new Size(51, 34)   , numProfiles = 1, imageKind = AdvancedColorKind.HighDynamicRange    , forceBT2100ColorSpace = false, isValid = true, isHeif = false }, imageCLL = new ImageCLL { maxNits = 0, medNits = 0 } },
            new ImageTestDefinition { filename = "Jxr_HdrXboxOne_1025px.jxr"  , useWic = true, imageInfo = new ImageInfo { bitsPerPixel = 64 , bitsPerChannel = 16, isFloat = true , size = new Size(1025, 576), numProfiles = 0, imageKind = AdvancedColorKind.HighDynamicRange    , forceBT2100ColorSpace = true , isValid = true, isHeif = false }, imageCLL = new ImageCLL { maxNits = 0, medNits = 0 } },
            new ImageTestDefinition { filename = "Tif_16bpcArgbIcc.tif"       , useWic = true, imageInfo = new ImageInfo { bitsPerPixel = 48 , bitsPerChannel = 16, isFloat = false, size = new Size(224, 149) , numProfiles = 1, imageKind = AdvancedColorKind.HighDynamicRange    , forceBT2100ColorSpace = false, isValid = true, isHeif = false }, imageCLL = new ImageCLL { maxNits = 0, medNits = 0 } },
        };

        /// <summary>
        /// Instead of trying to use the picker UI to access the test images, directly load them from the app package.
        /// </summary>
        /// <param name="def"></param>
        /// <returns></returns>
        public async static Task<StorageFile> GetTestImageAsync(ImageTestDefinition def)
        {
            string filepath = @"ms-appx://TestInputs/" + def.filename;
            var fileuri = new Uri(filepath);
            return await StorageFile.GetFileFromApplicationUriAsync(fileuri);
        }
    }
}
