#pragma once

static const float sc_DefaultHdrDispMaxNits = 600.0f; // Conservative default, prevents egregious behavior on low spec displays.
static const float sc_DefaultSdrDispMaxNits = 300.0f; // Empirically chosen to produce reasonable results on typical SDR displays.
// Note that going below this target nits value can produce artifacts in the OS tone mapper.
static const float sc_DefaultPaperWhiteNits = 203.0f; // Based on BT.2100 recommended SDR viewing conditions.
static const float sc_DefaultImageMaxCLL = 600.0f; // Needs more tuning based on real world content.
static const float sc_DefaultImageMedCLL = 80.0f; // Needs more tuning based on real world content.
static const float sc_MaxZoom = 1.0f; // Restrict max zoom to 1:1 scale.
static const float sc_MinZoomSphereMap = 0.25f;

// 400 bins with gamma of 10 lets us measure luminance to within 10% error for any
// luminance above ~1.5 nits, up to 1 million nits.
static const unsigned int sc_histNumBins = 400;
static const float        sc_histGamma = 0.1f;
static const unsigned int sc_histMaxNits = 1000000;