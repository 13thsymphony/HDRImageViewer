using DXRenderer;
using System;
using System.Threading.Tasks;
using Windows.Foundation.Collections;
using Windows.Graphics.Display;
using Windows.Storage;
using Windows.Storage.AccessCache;
using Windows.Storage.Pickers;
using Windows.System;
using Windows.UI.Core;
using Windows.UI.Input;
using Windows.UI.ViewManagement;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Navigation;

namespace HDRImageViewerCS
{
    /// <summary>
    /// Passed by the app to a new DXViewerPage. Note the defaults need to be sensible.
    /// </summary>
    public struct DXViewerLaunchArgs
    {
        public bool useFullscreen;
        public bool hideUI;
        public string initialFileToken; // StorageItemAccessList token
    }

    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class DXViewerPage : Page
    {
        HDRImageViewerRenderer renderer;
        GestureRecognizer gestureRecognizer;

        ImageInfo imageInfo;
        ImageCLL imageCLL;
        AdvancedColorInfo dispInfo;

        bool isImageValid;
        bool isWindowVisible;
        bool enableExperimentalTools;

        RenderOptionsViewModel viewModel;
        public RenderOptionsViewModel ViewModel { get { return viewModel; } }

        public DXViewerPage()
        {
            this.InitializeComponent();

            isWindowVisible = true;
            isImageValid = false;
            imageCLL.maxNits = imageCLL.medNits = -1.0f;

            // Register event handlers for page lifecycle.
            var window = Window.Current.CoreWindow;

            window.VisibilityChanged += OnVisibilityChanged;
            window.ResizeCompleted += OnResizeCompleted;

            var currDispInfo = DisplayInformation.GetForCurrentView();

            currDispInfo.DpiChanged += OnDpiChanged;
            currDispInfo.OrientationChanged += OnOrientationChanged;
            DisplayInformation.DisplayContentsInvalidated += OnDisplayContentsInvalidated;

            currDispInfo.AdvancedColorInfoChanged += OnAdvancedColorInfoChanged;
            var acInfo = currDispInfo.GetAdvancedColorInfo();

            swapChainPanel.CompositionScaleChanged += OnCompositionScaleChanged;
            swapChainPanel.SizeChanged += OnSwapChainPanelSizeChanged;

            // Pointer and manipulation events handle image pan and zoom.
            swapChainPanel.PointerPressed += OnPointerPressed;
            swapChainPanel.PointerMoved += OnPointerMoved;
            swapChainPanel.PointerReleased += OnPointerReleased;
            swapChainPanel.PointerCanceled += OnPointerCanceled;
            swapChainPanel.PointerWheelChanged += OnPointerWheelChanged;

            gestureRecognizer = new GestureRecognizer();
            gestureRecognizer.ManipulationStarted += OnManipulationStarted;
            gestureRecognizer.ManipulationUpdated += OnManipulationUpdated;
            gestureRecognizer.ManipulationCompleted += OnManipulationCompleted;
            gestureRecognizer.GestureSettings =
                GestureSettings.ManipulationTranslateX |
                GestureSettings.ManipulationTranslateY |
                GestureSettings.ManipulationScale;

            viewModel = new RenderOptionsViewModel();

            // At this point we have access to the device and can create the device-dependent resources.
            renderer = new HDRImageViewerRenderer(swapChainPanel);

            UpdateDisplayACState(acInfo);
        }

        protected async override void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);

            if (e.Parameter.GetType() == typeof(DXViewerLaunchArgs))
            {
                var args = (DXViewerLaunchArgs)e.Parameter;

                if (args.hideUI)
                {
                    SetUIHidden(true);
                }

                if (args.useFullscreen)
                {
                    SetUIFullscreen(true);
                }

                if (args.initialFileToken != null)
                {
                    var file = await StorageApplicationPermissions.FutureAccessList.GetFileAsync(args.initialFileToken);
                    await LoadImageAsync(file);
                }
            }
        }

        private void UpdateDisplayACState(AdvancedColorInfo newAcInfo)
        {
            AdvancedColorKind oldDispKind = AdvancedColorKind.StandardDynamicRange;
            if (dispInfo != null)
            {
                // dispInfo won't be available until the first image has been loaded.
                oldDispKind = dispInfo.CurrentAdvancedColorKind;
            }

            // TODO: Confirm that newAcInfo is never null. I believe this was needed in past versions for RS4 compat.
            dispInfo = newAcInfo;
            AdvancedColorKind newDispKind = dispInfo.CurrentAdvancedColorKind;
            DisplayACState.Text = UIStrings.LABEL_ACKIND + UIStrings.ConvertACKindToString(newDispKind);

            int maxcll = (int)dispInfo.MaxLuminanceInNits;

            if (maxcll == 0)
            {
                // Luminance value of 0 means that no valid data was provided by the display.
                DisplayPeakLuminance.Text = UIStrings.LABEL_PEAKLUMINANCE + UIStrings.LABEL_UNKNOWN;
            }
            else
            {
                DisplayPeakLuminance.Text = UIStrings.LABEL_PEAKLUMINANCE + maxcll.ToString() + UIStrings.LABEL_LUMINANCE_NITS;
            }

            if (oldDispKind == newDispKind)
            {
                // Some changes, such as peak luminance or SDR white level, don't need to reset rendering options.
                UpdateRenderOptions();
            }
            else
            {
                // If display has changed kind between SDR/HDR/WCG, we must reset all rendering options.
                UpdateDefaultRenderOptions();
            }
        }

        // Based on image and display parameters, choose the best rendering options.
        private void UpdateDefaultRenderOptions()
        {
            if (!isImageValid)
            {
                // Render options are only meaningful if an image is already loaded.
                return;
            }

            switch (imageInfo.imageKind)
            {
                case AdvancedColorKind.StandardDynamicRange:
                case AdvancedColorKind.WideColorGamut:
                default:
                    // SDR and WCG images don't need to be tonemapped.
                    RenderEffectCombo.SelectedIndex = 0; // See RenderOptions.h for which value this indicates.

                    // Manual brightness adjustment is only useful for HDR content.
                    // SDR and WCG content is adjusted by the OS-provided AdvancedColorInfo.SdrWhiteLevel parameter.
                    BrightnessAdjustSlider.Value = SdrBrightnessFormatter.BrightnessToSlider(1.0);
                    BrightnessAdjustPanel.Visibility = Visibility.Collapsed;
                    break;

                case AdvancedColorKind.HighDynamicRange:
                    // HDR images need to be tonemapped regardless of display kind.
                    RenderEffectCombo.SelectedIndex = 1; // See RenderOptions.h for which value this indicates.

                    // Manual brightness adjustment is useful for any HDR content.
                    BrightnessAdjustPanel.Visibility = Visibility.Visible;
                    break;
            }

            UpdateRenderOptions();
        }

        // Common method for updating options on the renderer.
        private void UpdateRenderOptions()
        {
            if ((renderer != null) && (RenderEffectCombo.SelectedItem != null))
            {
                var tm = (EffectOption)RenderEffectCombo.SelectedItem;

                var dispcll = enableExperimentalTools ? (float)DispMaxCLLOverrideSlider.Value : 0.0f;

                renderer.SetRenderOptions(
                    tm.Kind,
                    (float)SdrBrightnessFormatter.SliderToBrightness(BrightnessAdjustSlider.Value),
                    dispcll, // Display MaxCLL override
                    dispInfo
                    );
            }
        }

        // Swap chain event handlers.

        private void OnSwapChainPanelSizeChanged(object sender, SizeChangedEventArgs e)
        {
            renderer.SetLogicalSize(e.NewSize);
            renderer.CreateWindowSizeDependentResources();
            renderer.Draw();
        }

        private void OnCompositionScaleChanged(SwapChainPanel sender, object args)
        {
            renderer.SetCompositionScale(sender.CompositionScaleX, sender.CompositionScaleY);
            renderer.CreateWindowSizeDependentResources();
            renderer.Draw();
        }

        // Display state event handlers.

        private void OnAdvancedColorInfoChanged(DisplayInformation sender, object args)
        {
            UpdateDisplayACState(sender.GetAdvancedColorInfo());
        }

        private void OnDisplayContentsInvalidated(DisplayInformation sender, object args)
        {
            renderer.ValidateDevice();
            renderer.CreateWindowSizeDependentResources();
            renderer.Draw();
        }

        private void OnOrientationChanged(DisplayInformation sender, object args)
        {
            renderer.SetCurrentOrientation(sender.CurrentOrientation);
            renderer.CreateWindowSizeDependentResources();
            renderer.Draw();
        }

        private void OnDpiChanged(DisplayInformation sender, object args)
        {
            renderer.SetDpi(sender.LogicalDpi);
            renderer.CreateWindowSizeDependentResources();
            renderer.Draw();
        }

        // Window event handlers.

        // ResizeCompleted is used to detect when the window has been moved between different displays.
        private void OnResizeCompleted(CoreWindow sender, object args)
        {
            UpdateRenderOptions();
        }

        private void OnVisibilityChanged(CoreWindow sender, VisibilityChangedEventArgs args)
        {
            isWindowVisible = args.Visible;
            if (isWindowVisible)
            {
                renderer.Draw();
            }
        }

        // Keyboard accelerators.

        private void ToggleUIInvoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args)
        {
            if (Windows.UI.Xaml.Visibility.Collapsed == ControlsPanel.Visibility)
            {
                SetUIHidden(false);
            }
            else
            {
                SetUIHidden(true);
            }
        }

        private void ToggleFullscreenInvoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args)
        {
            if (ApplicationView.GetForCurrentView().IsFullScreenMode)
            {
                SetUIFullscreen(false);
            }
            else
            {
                SetUIFullscreen(true);
            }
        }

        private void EscapeFullscreenInvoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args)
        {
            SetUIFullscreen(false);
        }

        private void ToggleExperimentalToolsInvoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args)
        {
            SetExperimentalTools(!enableExperimentalTools);
        }

        /// <summary>
        /// If the OS is not at least 19H1, then shows an error dialog to the user.
        /// </summary>
        /// <returns>True if the check succeeded, and execution should continue.</returns>

        private bool CheckHeifAvifOsVersion()
        {
            // TODO: This helper is part of DXRenderer, for simplicity just copy the OS check.
            if (!Windows.Foundation.Metadata.ApiInformation.IsApiContractPresent(
                "Windows.Foundation.UniversalApiContract", 8)) // 8 == Windows 1903/19H1
            {
                var dialog = new ErrorContentDialog(ErrorDialogType.Need19H1);
#pragma warning disable CS4014 // Because this call is not awaited, execution of the current method continues before the call is completed
                dialog.ShowAsync();
#pragma warning restore CS4014 // Because this call is not awaited, execution of the current method continues before the call is completed

                return false;
            }
            else
            {
                return true;
            }
        }

        public async Task LoadImageAsync(StorageFile imageFile)
        {
            // File format handler registration is static vs. OS version (in the appxmanifset), so a user may attempt to activate
            // the app for a HEIF or AVIF image on RS5, which won't work.
            if (!CheckHeifAvifOsVersion())
            {
                return;
            }

            isImageValid = false;
            BrightnessAdjustSlider.IsEnabled = false;
            RenderEffectCombo.IsEnabled = false;

            bool useDirectXTex = false;

            var type = imageFile.FileType.ToLowerInvariant();
            if (type == ".hdr" ||
                type == ".exr" ||
                type == ".dds")
            {
                useDirectXTex = true;
            }

            ImageInfo info;

            if (useDirectXTex)
            {
                // For formats that are loaded by DirectXTex, we must use a file path from the temporary folder.
                imageFile = await imageFile.CopyAsync(
                        ApplicationData.Current.TemporaryFolder,
                        imageFile.Name,
                        NameCollisionOption.ReplaceExisting);

                info = renderer.LoadImageFromDirectXTex(imageFile.Path, type);
            }
            else
            {
                info = renderer.LoadImageFromWic(await imageFile.OpenAsync(FileAccessMode.Read));
            }

            if (info.isValid == false)
            {
                // Exit before any of the current image state is modified.
                ErrorContentDialog dialog;

                if (type == ".heic" && info.isHeif == true)
                {
                    dialog = new ErrorContentDialog(ErrorDialogType.NeedHevc, imageFile.Name);
                }
                else if (type == ".avif" && info.isHeif == true)
                {
                    dialog = new ErrorContentDialog(ErrorDialogType.NeedAv1, imageFile.Name);
                }
                else
                {
                    dialog = new ErrorContentDialog(ErrorDialogType.InvalidFile, imageFile.Name);
                }

                await dialog.ShowAsync();

                return;
            }

            imageInfo = info;

            renderer.CreateImageDependentResources();
            imageCLL = renderer.FitImageToWindow(true); // On first load of image, need to generate HDR metadata.

            ApplicationView.GetForCurrentView().Title = imageFile.Name;
            ImageACKind.Text = UIStrings.LABEL_ACKIND + UIStrings.ConvertACKindToString(imageInfo.imageKind);
            ImageHasProfile.Text = UIStrings.LABEL_COLORPROFILE + (imageInfo.numProfiles > 0 ? UIStrings.LABEL_YES : UIStrings.LABEL_NO);
            ImageBitDepth.Text = UIStrings.LABEL_BITDEPTH + imageInfo.bitsPerChannel;
            ImageIsFloat.Text = UIStrings.LABEL_FLOAT + (imageInfo.isFloat ? UIStrings.LABEL_YES : UIStrings.LABEL_NO);

            // TODO: Should we treat the 0 nit case as N/A as well? A fully black image would be known to have 0 CLL, which is valid...
            if (imageCLL.maxNits < 0.0f)
            {
                ImageMaxCLL.Text = UIStrings.LABEL_MAXCLL + UIStrings.LABEL_NA;
            }
            else
            {
                ImageMaxCLL.Text = UIStrings.LABEL_MAXCLL + imageCLL.maxNits.ToString("N1") + UIStrings.LABEL_LUMINANCE_NITS;
            }

            if (imageCLL.medNits < 0.0f)
            {
                ImageMedianCLL.Text = UIStrings.LABEL_MEDCLL + UIStrings.LABEL_NA;
            }
            else
            {
                ImageMedianCLL.Text = UIStrings.LABEL_MEDCLL + imageCLL.medNits.ToString("N1") + UIStrings.LABEL_LUMINANCE_NITS;
            }

            // Image loading is done at this point.
            isImageValid = true;
            BrightnessAdjustSlider.IsEnabled = true;
            RenderEffectCombo.IsEnabled = true;

            if (imageInfo.imageKind == AdvancedColorKind.HighDynamicRange)
            {
                ExportImageButton.IsEnabled = true;
            }
            else
            {
                ExportImageButton.IsEnabled = false;
            }

            UpdateDefaultRenderOptions();
        }

        private async Task ExportImageToSdrAsync(StorageFile file)
        {
            Guid wicFormat;
            if (file.FileType.Equals(".jpg", StringComparison.OrdinalIgnoreCase)) // TODO: Remove this hardcoded constant.
            {
                wicFormat = DirectXCppConstants.GUID_ContainerFormatJpeg;
            }
            else
            {
                wicFormat = DirectXCppConstants.GUID_ContainerFormatPng;
            }

            var ras = await file.OpenAsync(FileAccessMode.ReadWrite);
            renderer.ExportImageToSdr(ras, wicFormat);
        }

        private void SetUIHidden(bool value)
        {
            if (value == false)
            {
                ControlsPanel.Visibility = Visibility.Visible;
            }
            else
            {
                ControlsPanel.Visibility = Visibility.Collapsed;
            }
        }

        private void SetUIFullscreen(bool value)
        {
            if (value == false)
            {
                ApplicationView.GetForCurrentView().ExitFullScreenMode();
                ApplicationView.PreferredLaunchWindowingMode = ApplicationViewWindowingMode.Auto;
            }
            else
            {
                ApplicationView.GetForCurrentView().TryEnterFullScreenMode();
                ApplicationView.PreferredLaunchWindowingMode = ApplicationViewWindowingMode.FullScreen;
            }
        }

        private void SetExperimentalTools(bool value)
        {
            if (value == false)
            {
                enableExperimentalTools = false;
                ExperimentalTools.Visibility = Visibility.Collapsed;

            }
            else
            {
                enableExperimentalTools = true;
                ExperimentalTools.Visibility = Visibility.Visible;
            }

            // Right now this will both remove or apply the experimental tools.
            UpdateRenderOptions();
        }

        // Saves the current state of the app for suspend and terminate events.
        public void SaveInternalState(IPropertySet state)
        {
            renderer.Trim();
        }

        // Loads the current state of the app for resume events.
        public void LoadInternalState(IPropertySet state)
        {
        }

        // UI Element event handlers.

        private async void ExportImageButton_Click(object sender, RoutedEventArgs e)
        {
            var picker = new FileSavePicker
            {
                SuggestedStartLocation = PickerLocationId.PicturesLibrary,
                CommitButtonText = "Export image to SDR"
            };

            foreach (var format in UIStrings.FILEFORMATS_SAVE)
            {
                picker.FileTypeChoices.Add(format);
            }

            var pickedFile = await picker.PickSaveFileAsync();
            if (pickedFile != null)
            {
                await ExportImageToSdrAsync(pickedFile);
            }
        }

        private async void OpenImageButton_Click(object sender, RoutedEventArgs e)
        {
            var picker = new FileOpenPicker
            {
                SuggestedStartLocation = PickerLocationId.PicturesLibrary
            };

            foreach (var ext in UIStrings.FILEFORMATS_OPEN)
            {
                picker.FileTypeFilter.Add(ext);
            }

            // TODO: This helper is part of DXRenderer, for simplicity just copy the OS check.
            if (Windows.Foundation.Metadata.ApiInformation.IsApiContractPresent(
                "Windows.Foundation.UniversalApiContract", 8)) // 8 == Windows 1903/19H1
            {
                foreach (var ext in UIStrings.FILEFORMATS_OPEN_19H1)
                {
                    picker.FileTypeFilter.Add(ext);
                }
            }

            var file = await picker.PickSingleFileAsync();
            if (file != null)
            {
                await LoadImageAsync(file);
            }
        }

        private void BrightnessAdjustSlider_Changed(object sender, RangeBaseValueChangedEventArgs e)
        {
            UpdateRenderOptions();
        }

        private void RenderEffectCombo_Changed(object sender, SelectionChangedEventArgs e)
        {
            UpdateRenderOptions();
        }

        // Pointer input event handlers.

        private void OnPointerPressed(object sender, PointerRoutedEventArgs e)
        {
            swapChainPanel.CapturePointer(e.Pointer);
            gestureRecognizer.ProcessDownEvent(e.GetCurrentPoint(swapChainPanel));
        }

        private void OnPointerMoved(object sender, PointerRoutedEventArgs e)
        {
            gestureRecognizer.ProcessMoveEvents(e.GetIntermediatePoints(swapChainPanel));
        }

        private void OnPointerReleased(object sender, PointerRoutedEventArgs e)
        {
            gestureRecognizer.ProcessUpEvent(e.GetCurrentPoint(swapChainPanel));
            swapChainPanel.ReleasePointerCapture(e.Pointer);
        }

        private void OnPointerCanceled(object sender, PointerRoutedEventArgs e)
        {
            gestureRecognizer.CompleteGesture();
            swapChainPanel.ReleasePointerCapture(e.Pointer);
        }

        private void OnPointerWheelChanged(object sender, PointerRoutedEventArgs e)
        {
            // Passing isControlKeyDown = true causes the wheel delta to be treated as scrolling.
            gestureRecognizer.ProcessMouseWheelEvent(e.GetCurrentPoint(swapChainPanel), false, true);
        }

        private void OnManipulationStarted(GestureRecognizer sender, ManipulationStartedEventArgs args)
        {
        }
        private void OnManipulationUpdated(GestureRecognizer sender, ManipulationUpdatedEventArgs args)
        {
            renderer.UpdateManipulationState(args);
        }

        private void OnManipulationCompleted(GestureRecognizer sender, ManipulationCompletedEventArgs args)
        {
        }

        private void DispMaxCLLOverrideSlider_ValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            UpdateRenderOptions();
        }
    }
}
