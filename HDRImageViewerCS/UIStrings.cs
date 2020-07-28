using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Windows.Graphics.Display;

namespace HDRImageViewerCS
{
    static class UIStrings
    {
        public static string LABEL_PEAKLUMINANCE        = "Peak luminance: ";
        public static string LABEL_UNKNOWN              = "Unknown";
        public static string LABEL_LUMINANCE_NITS       = " nits"; // Leading space is intentional.
        public static string LABEL_ACKIND               = "Kind: ";
        public static string LABEL_COLORPROFILE         = "Color profile: ";
        public static string LABEL_YES                  = "Yes";
        public static string LABEL_NO                   = "No";
        public static string LABEL_BITDEPTH             = "Bits per channel: ";
        public static string LABEL_FLOAT                = "Floating point: ";
        public static string LABEL_MAXCLL               = "Estimated MaxCLL: ";
        public static string LABEL_NA                   = "N/A";
        public static string LABEL_MEDCLL               = "Estimated MedianCLL: ";

        public static string DIALOG_SAVECOMMIT          = "Export image to SDR";

        public static string[] FILEFORMATS_OPEN =
        {
            ".jxr",
            ".jpg",
            ".png",
            ".tif",
            ".hdr",
            ".exr",
            ".dds"
        };

        public static string[] FILEFORMATS_OPEN_19H1 =
        {
            ".heic",
            ".avif"
        };

        public static Dictionary<string, IList<string>> FILEFORMATS_SAVE = new Dictionary<string, IList<string>>()
        {
            { "JPEG image", new List<string> { ".jpg" } },
            { "PNG image" , new List<string> { ".png" } }
        };


        public static string ConvertACKindToString(AdvancedColorKind kind)
        {
            switch (kind)
            {
                case AdvancedColorKind.HighDynamicRange:
                    return "High Dynamic Range";

                case AdvancedColorKind.StandardDynamicRange:
                    return "Standard Dynamic Range";

                case AdvancedColorKind.WideColorGamut:
                    return "Wide Color Gamut";

                default:
                    return "Unknown";
            }
        }
    }
}
