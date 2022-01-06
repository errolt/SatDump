#pragma once

#include "module.h"
#include <complex>
#include "common/codings/viterbi/viterbi_1_2.h"
#include "common/codings/deframing/bpsk_ccsds_deframer.h"
#include <fstream>
#include "common/dsp/random.h"

namespace coriolis
{
    class CoriolisDBDecoderModule : public ProcessingModule
    {
    protected:
        uint8_t *buffer;

        std::ifstream data_in;
        std::ofstream data_out;
        std::atomic<size_t> filesize;
        std::atomic<size_t> progress;

        bool locked = false;
        int errors[4];
        int cor;

        viterbi::Viterbi1_2 viterbi;
        deframing::BPSK_CCSDS_Deframer deframer;

        // UI Stuff
        float ber_history[200];
        float cor_history[200];

        dsp::Random rng;

    public:
        CoriolisDBDecoderModule(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
        ~CoriolisDBDecoderModule();
        void process();
        void drawUI(bool window);
        std::vector<ModuleDataType> getInputTypes();
        std::vector<ModuleDataType> getOutputTypes();

    public:
        static std::string getID();
        virtual std::string getIDM() { return getID(); };
        static std::vector<std::string> getParameters();
        static std::shared_ptr<ProcessingModule> getInstance(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
    };
} // namespace meteor