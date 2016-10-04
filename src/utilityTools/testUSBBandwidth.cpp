#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <getopt.h>
#include <chrono>

#include "IConnection.h"
#include "ConnectionRegistry.h"
#include "LMS7002M.h"
#include "ErrorReporting.h"
#include "dataTypes.h"
#include <math.h>


using namespace std;
using namespace lime;

void PrintHelp()
{
    printf("--refClk= \tReference clock in Hz\n");
    printf("--config= \tChip configuration file to use for test\n");
    printf("--device= \tDevice index to automatically connect\n");
    printf("--cgen=   \tSet CGEN frequency in Hz\n");
    printf("--decimation= \tSet CGEN frequency in Hz\n");
}

int main(int argc, char** argv)
{
    double refClk = 30.72e6;
    double cgenFreq = 640e6;
    int decimation = 0;
    int deviceIndex = -1;

    string configFilename;
    while (1)
    {
        static struct option long_options[] =
        {
            {"refClk",      required_argument, 0, 'r'},
            {"config",      required_argument, 0, 'c'},
            {"device",      required_argument, 0, 'd'},
            {"help",        no_argument,       0, 'h'},
            {"cgen",        required_argument, 0, 'g'},
            {"decimation",  required_argument, 0, 'i'},
            {0, 0, 0, 0}
        };
        int option_index = 0;
        int c = getopt_long (argc, argv, "r:c:d:g:i:h", long_options, &option_index);

        if (c == -1) //no parameters given
            break;
        switch (c)
        {
        case 'g':{
            stringstream ss;
            ss << optarg;
            ss >> cgenFreq;
            break;
        }
        case 'i':{
            stringstream ss;
            ss << optarg;
            ss >> decimation;
            break;
        }
        case 'r':{
            stringstream ss;
            ss << optarg;
            ss >> refClk;
            break;
        }
        case 'c':{
            stringstream ss;
            ss << optarg;
            ss >> configFilename;
            break;
        }
        case 'd':{
            stringstream ss;
            ss << optarg;
            ss >> deviceIndex;
            break;
        }
        case 'h':
            PrintHelp();
            return 0;
        case '?':
            /* getopt_long already printed an error message. */
            break;
        default:
            abort();
        }
    }

    IConnection* serPort;
    std::vector<lime::ConnectionHandle> cachedHandles = ConnectionRegistry::findConnections();
    if(cachedHandles.size() == 0)
    {
        cout << "No devices found" << endl;
        return -1;
    }
    if(cachedHandles.size() == 1) //open the only available device
        serPort = ConnectionRegistry::makeConnection(cachedHandles.at(0));
    else //display device selection
    {
        if(deviceIndex < 0)
        {
            cout << "Device list:" << endl;
            for (size_t i = 0; i < cachedHandles.size(); i++)
               cout << setw(2) << i << ". " << cachedHandles[i].name << endl;
            cout << "Select device index (0-" << cachedHandles.size()-1 << "): ";
            int selection = 0; cin >> selection;
            selection = selection % cachedHandles.size();
            serPort = ConnectionRegistry::makeConnection(cachedHandles.at(selection));
        }
        else
            serPort = ConnectionRegistry::makeConnection(cachedHandles.at(deviceIndex));
    }
    if(serPort == nullptr)
    {
        cout << "Failed to connected to device" << endl;
        return -1;
    }
    DeviceInfo info = serPort->GetDeviceInfo();
    cout << "\nConnected to: " << info.deviceName
    << " FW: " << info.firmwareVersion << " HW: " << info.hardwareVersion
    << " GW: " << info.gatewareVersion << " GW_rev: " << info.gatewareRevision << endl;

    LMS7002M* lmsControl = new LMS7002M();
    lmsControl->SetConnection(serPort, 0);
    lmsControl->ResetChip();
    serPort->SetReferenceClockRate(refClk);
    if(configFilename.length() > 0)
    {
        if(lmsControl->LoadConfig(configFilename.c_str()) != 0)
        {
            cout << GetLastErrorMessage() << endl;
            return -1;
        }
    }
    else
    {
        lmsControl->UploadAll();
        lmsControl->SetActiveChannel(LMS7002M::ChA);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(EN_ADCCLKH_CLKGN), 0);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(CLKH_OV_CLKL_CGEN), 2);
        lmsControl->SetFrequencySX(LMS7002M::Tx, 1e6);
        lmsControl->SetFrequencySX(LMS7002M::Rx, 1e6);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(LML1_MODE), 0);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(LML2_MODE), 0);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(PD_RX_AFE2), 0);

        lmsControl->SetActiveChannel(LMS7002M::ChAB);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(INSEL_RXTSP), 1);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(GFIR1_BYP_RXTSP), 1);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(GFIR2_BYP_RXTSP), 1);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(GFIR3_BYP_RXTSP), 1);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(AGC_BYP_RXTSP), 1);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_RXTSP), 1);

        lmsControl->SetActiveChannel(LMS7002M::ChA);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(TSGFCW_RXTSP), 1);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(TSGFC_RXTSP), 1);
        lmsControl->SetActiveChannel(LMS7002M::ChB);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(TSGFCW_RXTSP), 1);
        lmsControl->Modify_SPI_Reg_bits(LMS7param(TSGFC_RXTSP), 0);
        lmsControl->SetActiveChannel(LMS7002M::ChA);
        lmsControl->SetInterfaceFrequency(cgenFreq, 0, 0);
    }

    auto txRate = lmsControl->GetSampleRate(LMS7002M::Tx, LMS7002M::Channel::ChA);
    auto rxRate = lmsControl->GetSampleRate(LMS7002M::Rx, LMS7002M::Channel::ChA);
    serPort->UpdateExternalDataRate(0, txRate, rxRate);
    printf("Sampling rates - Tx : %g MHz\t Rx : %g MHz\n", txRate/1e6, rxRate/1e6);

    vector<float> linkRates;
    linkRates.reserve(10);

    for(int c = 0; c<1; ++c)
    {
        //setup streaming
        size_t streamId;
        StreamConfig config;
        config.channelID = 0;
        config.isTx = false;
        config.performanceLatency = 1;
        config.format = StreamConfig::STREAM_12_BIT_COMPRESSED;
        config.linkFormat = StreamConfig::STREAM_12_BIT_COMPRESSED;

        //create streaming channel
        serPort->SetupStream(streamId, config);

        const int streamsize = 680*32;
        complex16_t* buffer = new complex16_t[streamsize];

        auto status = serPort->ControlStream(streamId, true);

        lime::IStreamChannel::Metadata metadata;
        int samplesRead = 0;
        auto t1 = chrono::high_resolution_clock::now();
        auto t2 = t1;
        auto probeT1 = t1;
        auto probeT2 = probeT1;

        IStreamChannel* channel = ((IStreamChannel*)streamId);
        bool stop = false;
        while(t2-t1 < chrono::seconds(10) && !stop)
        {
            samplesRead = channel->Read((void *)buffer, streamsize, &metadata, 1000);
            for(int i=0; i < samplesRead; ++i)
            {
                float ampA = pow(buffer[i].i, 2) + pow(buffer[i].q, 2);
                //check if samples are valid
                const int minAmp = 1900*1900;
                const int maxAmp = 2100*2100;
                if(ampA == 0 || ampA < minAmp || ampA > maxAmp)
                {
                    printf("Received invalid samples\n");
                    stop = true;
                    break;
                }
            }
            t2 = chrono::high_resolution_clock::now();
            if(t2 - probeT1 > chrono::milliseconds(1100))
            {
                probeT1 = t2;
                auto info = channel->GetInfo();
                linkRates.push_back(info.linkRate);
                printf("Rx rate: %g MB/s\n", info.linkRate/1e6);
            }
        }
        status = serPort->ControlStream(streamId, false);
        status = serPort->CloseStream(streamId);
        delete []buffer;
    }
    ConnectionRegistry::freeConnection(serPort);

    double avgRate = 0;
    for(auto a : linkRates)
        avgRate += a;
    avgRate /= linkRates.size();
    printf("Average Rx transfer speed: %g MB/s\n", avgRate/1e6);
    return 0;
}
