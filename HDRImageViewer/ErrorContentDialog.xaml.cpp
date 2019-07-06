//
// ErrorContentDialog.xaml.cpp
// Implementation of the ErrorContentDialog class
//

#include "pch.h"
#include "ErrorContentDialog.xaml.h"

using namespace HDRImageViewer;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

// The Content Dialog item template is documented at https://go.microsoft.com/fwlink/?LinkId=234238

ErrorContentDialog::ErrorContentDialog()
{
    InitializeComponent();
}

void ErrorContentDialog::SetNeedHevcText(bool val)
{
    if (val == true)
    {
        NeedHevcText->Visibility = Windows::UI::Xaml::Visibility::Visible;
    }
    else
    {
        NeedHevcText->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    }
}

void ErrorContentDialog::SetNeedAv1Text(bool val)
{
    if (val == true)
    {
        NeedAv1Text->Visibility = Windows::UI::Xaml::Visibility::Visible;
    }
    else
    {
        NeedAv1Text->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    }
}

void ErrorContentDialog::SetInvalidFileText(bool val)
{
    if (val == true)
    {
        InvalidImageText->Visibility = Windows::UI::Xaml::Visibility::Visible;
    }
    else
    {
        InvalidImageText->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    }
}
