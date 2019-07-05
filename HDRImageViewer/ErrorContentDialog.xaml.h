//
// ErrorContentDialog.xaml.h
// Declaration of the ErrorContentDialog class
//

#pragma once

#include "ErrorContentDialog.g.h"

namespace HDRImageViewer
{
    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class ErrorContentDialog sealed
    {
    public:
        ErrorContentDialog();

        void SetNeedHevcText(bool val);
        void SetInvalidFileText(bool val);
    private:
    };
}
