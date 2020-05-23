//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"
#include "App.xaml.h"

using namespace HDRImageViewer;

using namespace concurrency;
using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

/// <summary>
/// Initializes the singleton application object.  This is the first line of authored code
/// executed, and as such is the logical equivalent of main() or WinMain().
/// </summary>
App::App()
{
    InitializeComponent();
    Suspending += ref new SuspendingEventHandler(this, &App::OnSuspending);
    Resuming += ref new EventHandler<Object^>(this, &App::OnResuming);
}

/// <summary>
/// Invoked when the application is launched normally by the end user.  Other entry points
/// will be used when the application is launched to open a specific file, to display
/// search results, and so forth.
/// </summary>
/// <param name="e">Details about the launch request and process.</param>
void App::OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e)
{
    if (m_directXPage == nullptr)
    {
        m_directXPage = ref new DirectXPage();
    }

    if (e->PreviousExecutionState == ApplicationExecutionState::Terminated)
    {
        m_directXPage->LoadInternalState(ApplicationData::Current->LocalSettings->Values);
    }

    m_directXPage->LoadDefaultImage();

    // Place the page in the current window and ensure that it is active.
    Window::Current->Content = m_directXPage;
    Window::Current->Activate();
}

// Invoked when app is activated for special purposes such as via command line.
void App::OnActivated(Windows::ApplicationModel::Activation::IActivatedEventArgs^ args)
{
    if (args->Kind == ActivationKind::CommandLineLaunch)
    {
        auto cmd = static_cast<CommandLineActivatedEventArgs^>(args);
        auto rawArgs = cmd->Operation->Arguments;

        std::wstringstream argsStream;
        argsStream << rawArgs->Data();

        bool useFullscreen = false;
        bool hideUI = false;
        String^ fullFilename;

        std::wstring arg;
        std::getline(argsStream, arg, L' '); // First argument is the executable name.
        while (std::getline(argsStream, arg, L' '))
        {
            if (arg.size() == 0)
            {
                // Second argument always is L"" for some reason.
                continue;
            }

            const WCHAR fullscreenArg[] = L"-f";
            if (!wcsncmp(fullscreenArg, arg.c_str(), ARRAYSIZE(fullscreenArg) - 1))
            {
                useFullscreen = true;
                continue;
            }

            const WCHAR suppressUIArg[] = L"-h";
            if (!wcsncmp(suppressUIArg, arg.c_str(), ARRAYSIZE(suppressUIArg) - 1))
            {
                hideUI = true;
                continue;
            }

            const WCHAR inputArg[] = L"-input:";
            const UINT countInputArg = ARRAYSIZE(inputArg) - 1;
            if (!wcsncmp(inputArg, arg.c_str(), countInputArg) && wcslen(arg.c_str()) > countInputArg)
            {
                std::wstringstream path;
                path
                    << cmd->Operation->CurrentDirectoryPath->Data()
                    << L"\\"
                    << arg.substr(countInputArg);

                fullFilename = ref new String(path.str().c_str());
                continue;
            }

            std::wstringstream help;
            help
                << L"-f\n\tStart in fullscreen mode\n"
                << L"\tNOTE: This only applies on next startup.\n\n"
                << L"-h\n\tStart with UI hidden\n\n"
                << L"-input:[filename]\n\tLoad [filename]\n"
                << L"\tNOTE: The file must be in the current working directory,\n"
                << L"\tas HDRImageViewer only has access to this directory.";

            Platform::String^ helpString = ref new String(help.str().c_str());

            auto usageCtrl = ref new Windows::UI::Xaml::Controls::ContentDialog();
            usageCtrl->Title = L"HDRImageViewer command line usage";
            usageCtrl->Content = helpString;
            usageCtrl->CloseButtonText = L"OK";
            usageCtrl->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily(L"Consolas");
            usageCtrl->ShowAsync();

            break;
        }

        if (m_directXPage == nullptr)
        {
            m_directXPage = ref new DirectXPage();
        }

        m_directXPage->SetUIFullscreen(useFullscreen);
        m_directXPage->SetUIHidden(hideUI);

        // Place the page in the current window and ensure that it is active.
        Window::Current->Content = m_directXPage;
        Window::Current->Activate();

        if (fullFilename != nullptr)
        {
            try
            {
                create_task(StorageFile::GetFileFromPathAsync(fullFilename)).then([=](StorageFile^ file) {
                    if (file != nullptr)
                    {
                        m_directXPage->LoadImage(file);
                    }
                    });
            }
            catch (Platform::Exception^ e)
            {
                auto fileCtrl = ref new Windows::UI::Xaml::Controls::ContentDialog();
                fileCtrl->Title = L"Error loading the specified file";
                fileCtrl->Content = e->Message;
                fileCtrl->CloseButtonText = L"OK";
                fileCtrl->ShowAsync();
            }
        }
    }
}

// Invoked when the app is launched via the file type association for which the app has registered.
void App::OnFileActivated(Windows::ApplicationModel::Activation::FileActivatedEventArgs ^ e)
{
    if (m_directXPage == nullptr)
    {
        m_directXPage = ref new DirectXPage();
    }

    if (e->PreviousExecutionState == ApplicationExecutionState::Terminated)
    {
        m_directXPage->LoadInternalState(ApplicationData::Current->LocalSettings->Values);
    }

    int numFiles = e->Files->Size;

    if (numFiles == 1)
    {
        IStorageItem^ storageItem = e->Files->GetAt(0);
        StorageFile^ storageFile = (StorageFile^)storageItem;

        m_directXPage->LoadImage(storageFile);
    }
    else
    {
        throw ref new Platform::FailureException(L"Activated with incorrect number of files.");
    }

    // Place the page in the current window and ensure that it is active.
    Window::Current->Content = m_directXPage;
    Window::Current->Activate();
}

/// <summary>
/// Invoked when application execution is being suspended.  Application state is saved
/// without knowing whether the application will be terminated or resumed with the contents
/// of memory still intact.
/// </summary>
/// <param name="sender">The source of the suspend request.</param>
/// <param name="e">Details about the suspend request.</param>
void App::OnSuspending(Object^, SuspendingEventArgs^)
{
    m_directXPage->SaveInternalState(ApplicationData::Current->LocalSettings->Values);
}

/// <summary>
/// Invoked when application execution is being resumed.
/// </summary>
/// <param name="sender">The source of the resume request.</param>
/// <param name="args">Details about the resume request.</param>
void App::OnResuming(Object^, Object^)
{
    m_directXPage->LoadInternalState(ApplicationData::Current->LocalSettings->Values);
}
