using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Windows.UI.Xaml.Data;

namespace HDRImageViewerCS
{
    // The exposure adjustment slider maps the UI linear value to an exponential (power of 2) multiplier factor.
    // The user sees the multiplier as the tooltip text, and the renderer uses this multiplier internally.
    public class SdrExposureFormatter : IValueConverter
    {
        // Expects value is a numeric type (converts to double).
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            double percent = SliderToExposure((double)value) * 100.0;

            string text = percent.ToString("N1") + "%";
            return text;
        }

        // No need to implement converting back on a one-way binding.
        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            throw new NotImplementedException();
        }

        // Convert slider UI value (linear) to exposure multiplier (2^x).
        public static double SliderToExposure(double slider)
        {
            return Math.Pow(2.0, slider);
        }

        // Convert exposure multiplier (2^x) to slider UI value (linear).
        public static double ExposureToSlider(double multiplier)
        {
            multiplier = Math.Max(float.MinValue, multiplier);

            return Math.Log(multiplier) / Math.Log(2.0);
        }
    }
}
