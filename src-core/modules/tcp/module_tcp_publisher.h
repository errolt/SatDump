#pragma once

#include "module.h"
#include <complex>
#include <fstream>

namespace tcp
{
    class TCPPublisherModule : public ProcessingModule
    {
    protected:
        uint8_t *buffer;

        std::ifstream data_in;
        std::atomic<size_t> filesize;
        std::atomic<size_t> progress;

        std::string address;
        int port;
        int frame_size;

        // UI Stuff
        float ber_history[200];
        float cor_history[200];

    public:
        TCPPublisherModule(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
        ~TCPPublisherModule();
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
}