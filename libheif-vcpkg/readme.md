This directory contains a copy of [libheif](https://github.com/strukturag/libheif) built using [vcpkg](https://github.com/Microsoft/vcpkg/) and exported into a local NuGet package. Libheif is consumed by HDRImageViewer as a dynamic link library to comply with requirements of the LGPL license.

## Instructions for using libheif

You must manually add the libheif-vcpkg directory to Visual Studio's list of NuGet package repositories:

1. In Visual Studio, go to `Tools > NuGet Package Manager > Package Manager Settings`.
2. In the `Package Sources` page, add the `libheif-vcpkg` directory as a new source.