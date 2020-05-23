# HDR + WCG Image Viewer
Repository hosting source code for the [HDR + WCG Image Viewer UWP app](https://www.microsoft.com/store/apps/9PGN3NWPBWL9).

This app is based on code from the D2DAdvancedColorImages SDK sample which is part of the [Windows 10 samples repository](http://go.microsoft.com/fwlink/p/?LinkId=619979). It is the spiritual successor to the [HDR Image Viewer](https://www.microsoft.com/store/productId/9NPSWXVL7W40) published by Rick Manning.

Releases/tags correspond to versions pushed to the Windows Store.

## Command line usage
You should invoke HDRImageViewer from the directory containing the image you wish to load - UWP apps launched from a command line only have access to files within the working directory.
### Parameters
`-f` Start in fullscreen mode

`-h` Start with UI hidden

`-input:[filename]` Load [filename]

**Note: Filename must be relative to the current working directory as HDRImageViewer only has access to that directory.**
### Example
`HDRImageViewer.exe -f -h -input:myimage.jxr`
