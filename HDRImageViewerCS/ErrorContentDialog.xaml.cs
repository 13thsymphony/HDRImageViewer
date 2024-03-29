﻿using System;
using Windows.Storage.Search;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;


// The User Control item template is documented at https://go.microsoft.com/fwlink/?LinkId=234236

namespace HDRImageViewerCS
{
    [Flags]
    public enum ErrorDialogType
    {
        DefaultValue = 0,
        InvalidFile = 1,
        Need19H1 = 2,
        NeedHevc = 4,
        NeedAv1 = 8,
        InvalidCmdArgs = 16
    }

    public sealed partial class ErrorContentDialog : ContentDialog
    {
        public ErrorContentDialog(ErrorDialogType type, String customTitle = UIStrings.ERROR_DEFAULTTITLE)
        {
            this.InitializeComponent();

            this.Title = customTitle;

            if (type.HasFlag(ErrorDialogType.InvalidFile))
            {
                InvalidFileText.Visibility = Visibility.Visible;
            }

            if (type.HasFlag(ErrorDialogType.Need19H1))
            {
                Need19H1Text.Visibility = Visibility.Visible;
            }

            if (type.HasFlag(ErrorDialogType.NeedHevc))
            {
                NeedHevcText.Visibility = Visibility.Visible;
            }

            if (type.HasFlag(ErrorDialogType.NeedAv1))
            {
                NeedAv1Text.Visibility = Visibility.Visible;
            }

            if (type.HasFlag(ErrorDialogType.InvalidCmdArgs))
            {
                CmdArgsText.Visibility = Visibility.Visible;
                this.Title = UIStrings.ERROR_INVALIDCMDARGS;
            }
        }
    }
}
