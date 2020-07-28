using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;


// The User Control item template is documented at https://go.microsoft.com/fwlink/?LinkId=234236

namespace HDRImageViewerCS
{
    public sealed partial class ErrorContentDialog : ContentDialog
    {
        public ErrorContentDialog()
        {
            this.InitializeComponent();
        }

        public void SetNeedHevcText(bool val)
        {
            if (val == true)
            {
                NeedHevcText.Visibility = Visibility.Visible;
            }
            else
            {
                NeedHevcText.Visibility = Visibility.Collapsed;
            }
        }

        public void SetNeedAv1Text(bool val)
        {
            if (val == true)
            {
                NeedAv1Text.Visibility = Visibility.Visible;
            }
            else
            {
                NeedAv1Text.Visibility = Visibility.Collapsed;
            }
        }

        public void SetInvalidFileText(bool val)
        {
            if (val == true)
            {
                InvalidImageText.Visibility = Visibility.Visible;
            }
            else
            {
                InvalidImageText.Visibility = Visibility.Collapsed;
            }
        }
    }
}
