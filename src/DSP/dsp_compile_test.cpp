// Compile-time verification that all DSP templates instantiate correctly.
// This file is built as part of the library but does no runtime work.

#include <SciQLopPlots/DSP/DSP.hpp>

// Force template instantiation for double, float, int32_t.
template struct sqp::dsp::Segment<double>;
template struct sqp::dsp::Segment<float>;
template struct sqp::dsp::Segment<int32_t>;

template struct sqp::dsp::TimeSeries<double>;
template struct sqp::dsp::TimeSeries<float>;
template struct sqp::dsp::TimeSeries<int32_t>;

template struct sqp::dsp::FFTResult<double>;
template struct sqp::dsp::FFTResult<float>;

template struct sqp::dsp::SpectrogramResult<double>;
template struct sqp::dsp::SpectrogramResult<float>;

// Force instantiation of key function templates.
namespace
{
[[maybe_unused]] void instantiate()
{
    // Segments
    {
        double x[] = { 1.0, 2.0, 3.0 };
        double y[] = { 1.0, 2.0, 3.0 };
        [[maybe_unused]] auto segs = sqp::dsp::split_segments<double>({ x, 3 }, { y, 3 }, 1);
    }

    // NaN interpolation
    {
        double x[] = { 1.0, 2.0, 3.0 };
        double y[] = { 1.0, std::nan(""), 3.0 };
        [[maybe_unused]] auto result = sqp::dsp::interpolate_nan(x, y, 3, 1, 1);
    }

    // Window
    {
        [[maybe_unused]] auto w = sqp::dsp::make_window<double>(256, sqp::dsp::WindowType::Hann);
    }

    // Resample stage
    {
        [[maybe_unused]] auto stage = sqp::dsp::resample_uniform<double>();
    }

    // Filter stages
    {
        double coeffs[] = { 0.25, 0.5, 0.25 };
        [[maybe_unused]] auto fir = sqp::dsp::fir_filter<double>({ coeffs, 3 });

        double sos[] = { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 }; // passthrough
        [[maybe_unused]] auto iir = sqp::dsp::iir_sos<double>({ sos, 6 }, 1);
    }

    // FFT
    {
        [[maybe_unused]] auto stage = sqp::dsp::fft_stage<double>();
    }

    // Stats
    {
        [[maybe_unused]] auto rm = sqp::dsp::rolling_mean<double>(5);
        [[maybe_unused]] auto rs = sqp::dsp::rolling_std<double>(5);
    }

    // Reduce
    {
        [[maybe_unused]] auto r = sqp::dsp::reduce<double>(sqp::dsp::ReduceOp::Sum);
    }
}
} // anonymous namespace
