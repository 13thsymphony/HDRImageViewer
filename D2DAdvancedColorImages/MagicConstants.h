#pragma once

static const float sc_DefaultHdrDispMaxNits = 1499.0f; // Experimentally chosen for compatibility with 2018 TVs.
static const float sc_DefaultSdrDispMaxNits = 270.0f; // Experimentally chosen based on typical SDR displays.
static const float sc_DefaultImageMaxCLL = 1000.0f; // Needs more tuning based on real world content.
static const float sc_DefaultImageMedCLL = 200.0f; // Needs more tuning based on real world content.
static const float sc_MaxZoom = 1.0f; // Restrict max zoom to 1:1 scale.
static const float sc_MinZoomSphereMap = 0.25f;
static const float sc_nominalRefWhite = 80.0f; // Nominal white nits for sRGB and scRGB.

// 400 bins with gamma of 10 lets us measure luminance to within 10% error for any
// luminance above ~1.5 nits, up to 1 million nits.
static const unsigned int sc_histNumBins = 400;
static const float        sc_histGamma = 0.1f;
static const unsigned int sc_histMaxNits = 1000000;