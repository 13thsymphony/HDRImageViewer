using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Storage;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

using DXRenderer;
using Windows.UI.Input;
using Windows.Graphics.Display;

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace HDRImageViewerCS
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class DXViewerPage : Page
    {
        public DXViewerPage()
        {
            this.InitializeComponent();
        }

        public async void LoadImageAsync(IStorageItem file)
        {

        }

        // Resources used to draw the DirectX content in the XAML page.
        HDRImageViewerRenderer  Renderer;
        GestureRecognizer       GestureRecognizer;
        bool                    IsWindowVisible;

        // Cached information for UI.
        ImageInfo               ImageInfo;
        ImageCLL                ImageCLL;
        AdvancedColorInfo       DispInfo;
        RenderOptionsViewModel  RenderOptionsViewModel;
    }
}
