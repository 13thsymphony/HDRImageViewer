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
        public const string LABEL_PEAKLUMINANCE        = "Peak luminance: ";
        public const string LABEL_UNKNOWN              = "Unknown";
        public const string LABEL_LUMINANCE_NITS       = " nits"; // Leading space is intentional.
        public const string LABEL_ACKIND               = "Kind: ";
        public const string LABEL_COLORPROFILE         = "Color profile: ";
        public const string LABEL_YES                  = "Yes";
        public const string LABEL_NO                   = "No";
        public const string LABEL_BITDEPTH             = "Bits per channel: ";
        public const string LABEL_FLOAT                = "Floating point: ";
        public const string LABEL_MAXCLL               = "Estimated MaxCLL: ";
        public const string LABEL_NA                   = "N/A";
        public const string LABEL_MEDCLL               = "Estimated MedianCLL: ";
        public const string LABEL_GAMUTMAP             = "Constrain image gamut: ";
        public const string LABEL_PROFILEOVERRIDE      = "Override display colorimetry: ";

        public const string ERROR_DEFAULTTITLE         = "Unable to load image";
        public const string ERROR_INVALIDCMDARGS       = "Command line usage";

        public const string DIALOG_SAVECOMMIT          = "Export image to SDR";

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
            { "JPEG image (SDR)", new List<string> { ".jpg" } },
            { "PNG image (SDR)" , new List<string> { ".png" } },
            { "JPEG-XR image (HDR)" , new List<string> { ".jxr" } }
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
