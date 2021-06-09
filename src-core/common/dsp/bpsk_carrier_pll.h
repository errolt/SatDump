#pragma once

#include "block.h"
#include "lib/carrier_pll_psk.h"

namespace dsp
{
    class BPSKCarrierPLLBlock : public Block<std::complex<float>, float>
    {
    private:
        dsp::BPSKCarrierPLL d_pll;
        void work();

    public:
        BPSKCarrierPLLBlock(std::shared_ptr<dsp::stream<std::complex<float>>> input, float alpha, float beta, float max_offset);
    };
}