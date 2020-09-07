using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.ApplicationModel;
using Windows.ApplicationModel.Activation;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Storage;
using Windows.Storage.AccessCache;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

namespace HDRImageViewerCS
{
    /// <summary>
    /// Provides application-specific behavior to supplement the default Application class.
    /// </summary>
    sealed partial class App : Application
    {
        /// <summary>
        /// Initializes the singleton application object.  This is the first line of authored code
        /// executed, and as such is the logical equivalent of main() or WinMain().
        /// </summary>
        public App()
        {
            this.InitializeComponent();
            this.Suspending += OnSuspending;
            // TODO: Add Resuming.
        }

        /// <summary>
        /// Invoked when the application is launched normally by the end user.  Other entry points
        /// will be used such as when the application is launched to open a specific file.
        /// </summary>
        /// <param name="e">Details about the launch request and process.</param>
        protected override void OnLaunched(LaunchActivatedEventArgs e)
        {
            if (e.PreviousExecutionState == ApplicationExecutionState.Terminated)
            {
                // TODO: Load state from previously suspended application and pass into LaunchApp.
            }

            LaunchAppCommon(new DXViewerLaunchArgs(), e.PrelaunchActivated);
        }

        // Invoked when app is activated for special purposes such as via command line.
        protected async override void OnActivated(IActivatedEventArgs activatedArgs)
        {
            base.OnActivated(activatedArgs);

            DXViewerLaunchArgs launchArgs = new DXViewerLaunchArgs();

            if (activatedArgs.Kind == ActivationKind.CommandLineLaunch)
            {
                var cmd = (CommandLineActivatedEventArgs)activatedArgs;
                var cmdArgs = cmd.Operation.Arguments.Split(' ');

                var inputArgString = "-input:";

                // Ignore the first argument which is always the "executable" name.
                // This also ensures that just invoking the executable without arguments succeeds.
                for (int i = 1; i < cmdArgs.Length; i++)
                {
                    // Ignore zero-length arguments.
                    if (cmdArgs[i].Length == 0) { continue; }

                    if (cmdArgs[i].Equals("-f", StringComparison.InvariantCultureIgnoreCase))
                    {
                        launchArgs.useFullscreen = true;
                    }
                    else if (cmdArgs[i].Equals("-h", StringComparison.InvariantCultureIgnoreCase))
                    {
                        launchArgs.hideUI = true;
                    }
                    else if (cmdArgs[i].StartsWith(inputArgString, StringComparison.InvariantCultureIgnoreCase))
                    {
                        var fullPath = cmd.Operation.CurrentDirectoryPath + "\\" + cmdArgs[i].Substring(inputArgString.Length);
                        try
                        {
                            var file = await StorageFile.GetFileFromPathAsync(fullPath);
                            launchArgs.initialFileToken = StorageApplicationPermissions.FutureAccessList.Add(file);
                        }
                        catch
                        {
                            launchArgs.errorType |= ErrorDialogType.InvalidFile;
                            launchArgs.initialFileToken = null;
                            launchArgs.errorFilename = fullPath;
                        }
                    }
                    else // All other tokens are invalid.
                    {
                        launchArgs.errorType |= ErrorDialogType.InvalidCmdArgs;
                    }
                }
            }

            LaunchAppCommon(launchArgs, false);
        }

        private void LaunchAppCommon(DXViewerLaunchArgs launchArgs, bool PrelaunchActivated)
        {
            Frame rootFrame = Window.Current.Content as Frame;

            // Do not repeat app initialization when the Window already has content,
            // just ensure that the window is active
            if (rootFrame == null)
            {
                // Create a Frame to act as the navigation context and navigate to the first page
                rootFrame = new Frame();

                rootFrame.NavigationFailed += OnNavigationFailed;

                // Place the frame in the current Window
                Window.Current.Content = rootFrame;
            }

            if (PrelaunchActivated == false)
            {
                if (rootFrame.Content == null)
                {
                    rootFrame.Navigate(typeof(DXViewerPage), launchArgs);
                }
                // Ensure the current window is active
                Window.Current.Activate();
            }
        }

        // Invoked when the app is launched via the file type association for which the app has registered.
        protected override void OnFileActivated(Windows.ApplicationModel.Activation.FileActivatedEventArgs e)
        {
            IStorageItem file = e.Files.First();

            var args = new DXViewerLaunchArgs()
            {
                hideUI = false,
                useFullscreen = false,
                initialFileToken = StorageApplicationPermissions.FutureAccessList.Add(file)
            };

            LaunchAppCommon(args, false);
        }

        /// <summary>
        /// Invoked when Navigation to a certain page fails
        /// </summary>
        /// <param name="sender">The Frame which failed navigation</param>
        /// <param name="e">Details about the navigation failure</param>
        void OnNavigationFailed(object sender, NavigationFailedEventArgs e)
        {
            throw new Exception("Failed to load Page " + e.SourcePageType.FullName);
        }
        
        private void InitializeContent(FileActivatedEventArgs e)
        {

        }

        /// <summary>
        /// Invoked when application execution is being suspended.  Application state is saved
        /// without knowing whether the application will be terminated or resumed with the contents
        /// of memory still intact.
        /// </summary>
        /// <param name="sender">The source of the suspend request.</param>
        /// <param name="e">Details about the suspend request.</param>
        private void OnSuspending(object sender, SuspendingEventArgs e)
        {
            var deferral = e.SuspendingOperation.GetDeferral();
            //TODO: Save application state and stop any background activity
            deferral.Complete();
        }
    }
}
