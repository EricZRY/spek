#include <cmath>

#define __STDC_CONSTANT_MACROS
extern "C" {
// FFmpeg 6 deprecated libavcodec/avfft.h; FFmpeg 7+/9 removed it.
// Use libavutil/tx.h AV_TX_FLOAT_RDFT instead of av_rdft_*.
#include <libavutil/tx.h>
}

#include "spek-fft.h"

class FFTPlanImpl : public FFTPlan
{
public:
    FFTPlanImpl(int nbits);
    ~FFTPlanImpl() override;

    void execute() override;

private:
    AVTXContext *ctx;
    av_tx_fn tx_fn;
    AVComplexFloat *cplx; // length = n/2 + 1
};

std::unique_ptr<FFTPlan> FFT::create(int nbits)
{
    return std::unique_ptr<FFTPlan>(new FFTPlanImpl(nbits));
}

FFTPlanImpl::FFTPlanImpl(int nbits) : FFTPlan(nbits), ctx(nullptr), tx_fn(nullptr), cplx(nullptr)
{
    const int n = this->get_input_size();
    const float scale = 1.0f;
    // Forward real-to-complex RDFT of length n.
    if (av_tx_init(&this->ctx, &this->tx_fn, AV_TX_FLOAT_RDFT, 0, n, &scale, 0) < 0) {
        this->ctx = nullptr;
        this->tx_fn = nullptr;
    }
    // Complex bins: DC .. Nyquist (n/2 + 1)
    this->cplx = (AVComplexFloat *)av_malloc(sizeof(AVComplexFloat) * (n / 2 + 1));
}

FFTPlanImpl::~FFTPlanImpl()
{
    av_tx_uninit(&this->ctx);
    av_freep(&this->cplx);
}

void FFTPlanImpl::execute()
{
    if (!this->ctx || !this->tx_fn || !this->cplx) {
        return;
    }

    // R2C: stride is spacing between real input samples in bytes.
    this->tx_fn(this->ctx, this->cplx, this->get_input(), sizeof(float));

    // Calculate magnitudes (equivalent to old packed av_rdft layout).
    int n = this->get_input_size();
    float n2 = n * n;
    this->set_output(0, 10.0f * log10f(this->cplx[0].re * this->cplx[0].re / n2));
    this->set_output(n / 2, 10.0f * log10f(this->cplx[n / 2].re * this->cplx[n / 2].re / n2));
    for (int i = 1; i < n / 2; i++) {
        float re = this->cplx[i].re;
        float im = this->cplx[i].im;
        this->set_output(i, 10.0f * log10f((re * re + im * im) / n2));
    }
}
