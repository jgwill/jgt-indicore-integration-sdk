#include "stdafx.h"

#include "Utils.h"
#include "SampleParams.h"

bool parseCmdParams(int, char*[], std::string&, std::string&);
void preparePriceData(indicore3::BarPriceStorage *priceStorage);

int main(int argc, char* argv[])
{
    std::string indicatorId;
    std::string indicatorsPath;

    bool paramsAreValid = parseCmdParams(argc, argv, indicatorId, indicatorsPath);
    if (!paramsAreValid)
        return 1;

    //create file accessor for loading Lua indicators
    AutoRelease<indicore3::FileSystemAccessor> fileAccessor(new indicore3::FileSystemAccessor());

    AutoRelease<indicore3::IFileSystemMetadataProvider> fsmp(new indicore3::FileSystemMetadataProvider(indicore3::ILanguageService::Lua,
        indicore3::ILanguageService::Indicator));

    indicore3::IError *indiError = nullptr;
    if (!fileAccessor->init(indicatorsPath.c_str(), fsmp.get(), &indiError))
    {
        AutoRelease<indicore3::IError> autoIndiError(indiError);
        Utils::printIndicoreError(indiError);
        return -1;
    }

    const char *mask[] = { "*.lua", nullptr };

    //create an enumerator of standard indicators
    std::string pathStandard = indicatorsPath;
    AutoRelease<indicore3::IFileEnumerator> enumeratorStandard(fileAccessor->enumerator(mask, false, &indiError));
    if (!enumeratorStandard.get())
    {
        AutoRelease<indicore3::IError> autoIndiError(indiError);
        Utils::printIndicoreError(indiError);
        return -1;
    }

    //create IndicoreManager and Host
    AutoRelease<indicore3::IndicoreManager> indicoreManager(indicore3::IndicoreManager::createInstance());
    AutoRelease<indicore3::IHost> host(new indicore3::BaseHostImpl());

    indicore3::IDomain *domain = indicoreManager->createDomain("Main", "Main domain");

    //load indicators
    AutoRelease<indicore3::ILoadMetadata> loadMetadata(new indicore3::LoadMetadata(host.get()));

    indicoreManager->loadIntoDomain(domain, fileAccessor.get(), enumeratorStandard.get(), loadMetadata.get(), &indiError);
    if (indiError)
    {
        AutoRelease<indicore3::IError> autoIndiError(indiError);
        Utils::printIndicoreError(indiError);
        return -1;
    }

    //prepare price data
    AutoRelease<indicore3::BarPriceStorage> storage( new indicore3::BarPriceStorage(
                                                            "MyIndicatorSource",                           //name
                                                            "AAPL",                                        //instrument
                                                            "D1",                                          //timeframe
                                                            indicore3::BarPriceStorage::FirstTickNewBar,   //mode
                                                            -7,                                            //tradingDayOffset
                                                            0,                                             //tradingWeekOffset
                                                            4,                                             //precision
                                                            4,                                             //displayprecision
                                                            0.01,                                          //pipSize
                                                            false,                                         //supportVolume
                                                            true,                                          //alive
                                                            1,                                             //m_id
                                                            100,                                           //limit
                                                            1));                                            //instrumentIndex

    preparePriceData(storage.get());

    //create indicator instance
    AutoRelease<indicore3::IIndicatorProfiles> indicators(indicoreManager->getIndicatorProfiles());
    indicore3::IIndicatorProfile *profile = indicators->getProfile(indicatorId.c_str());
    if (profile == nullptr)
    {
        std::cout << "Error: the indicator " << indicatorId << " is not found ." << std::endl;
        return 1;
    }

    AutoRelease<indicore3::IPriceBarStream> source(storage->getBidPrices());

    AutoRelease<indicore3::IParameters> params(profile->getParameters());
    AutoRelease<indicore3::IIndicatorInstance> instance(profile->createInstance(host.get(), source.get(), params.get()));
    if (instance.get() == nullptr)
    {
        std::cout << "Error: cannot create the indicator." << std::endl;
        return 1;
    }

    //run prepare method
    if (!instance->prepare(false, &indiError))
    {
        AutoRelease<indicore3::IError> autoIndiError(indiError);
        Utils::printIndicoreError(indiError);
        return -1;
    }

    //update price data
    {
        if (!instance->update(true, &indiError))
        {
            AutoRelease<indicore3::IError> autoIndiError(indiError);
            Utils::printIndicoreError(indiError);
            return -1;
        }

        std::cout.precision(storage->getDisplayPrecision());
        //processing results
        for (int i = 0; i < instance->getStreamCount(); ++i)
        {
            AutoRelease<indicore3::IOutputStream> out(instance->getStream(i));
            {
                std::cout << "Output stream name: " << out->getName() << std::endl;
                std::cout << "Date;Price;" << std::endl;
                for (index_t i = out->getFirstPeriod(); i < out->size(); i++)
                {
                    if (out->hasData(i))
                        std::cout << Utils::formatDate(out->date(i)) << ";" << std::fixed << out->getPrice(i) << ";" << std::endl;
                    else
                        std::cout << "n/a" << std::endl;
                }
            }
        }
    }

    return 0;
}

bool parseCmdParams(int argc, char* argv[], std::string& indicatorId, std::string& indicatorsPath)
{
    const std::string progName = "CalculateIndicator";

    if (argc == 1) {
        SampleParams::printHelp(progName);
        return false;
    }

    SampleParams sampleParam(argc, argv);
    std::string error;
    if (!sampleParam.checkObligatoryParams(error))
    {
        std::cout << error << std::endl;
        return false;
    }

    indicatorId = sampleParam.getIndicatorID();
    indicatorsPath = sampleParam.getIndicatorsPath();

    sampleParam.printSampleParams(progName);

    if (indicatorId.empty())
    {
        std::cout << "You does not specify the indicator_id. "
            << "Sample will be run the MVA indicator"
            << "and run indicators with default parameters."
            << std::endl;

        indicatorId = "MVA";
    }

    if (!Utils::IsPathExist(indicatorsPath))
    {
        std::cout << "Error: specified path does not exists" << std::endl;
        return false;
    }

    return true;
}

/**
Construct double value in OLE Automation date format
for given year, month and day.
*/
double constructDate(int year, int month, int day)
{
    SYSTEMTIME st;
    double dt;

    st.wYear = year;
    st.wMonth = month;
    st.wDay = day;
    st.wHour = 0;
    st.wMinute = 0;
    st.wSecond = 0;
    st.wMilliseconds = 0;

    SystemTimeToVariantTime(&st, &dt);

    return dt;
}

void preparePriceData(indicore3::BarPriceStorage *priceStorage)
{
    priceStorage->addBar(constructDate(2018, 05, 15), 67.37, 68.38, 67.12, 67.79, 67.37, 68.38, 67.12, 67.79, 12);
    priceStorage->addBar(constructDate(2018, 05, 16), 68.1, 68.25, 64.75, 64.98, 67.37, 68.38, 67.12, 67.79, 6);
    priceStorage->addBar(constructDate(2018, 05, 17), 64.71, 65.7, 64.07, 65.26, 64.71, 65.7, 64.07, 65.26, 4);
    priceStorage->addBar(constructDate(2018, 05, 18), 65.68, 66.26, 63.12, 63.18, 65.68, 66.26, 63.12, 63.18, 4);
    priceStorage->addBar(constructDate(2018, 05, 19), 63.26, 64.88, 62.82, 64.51, 63.26, 64.88, 62.82, 64.51, 4);
    priceStorage->addBar(constructDate(2018, 05, 22), 63.87, 63.99, 62.77, 63.38, 63.87, 63.99, 62.77, 63.38, 4);
    priceStorage->addBar(constructDate(2018, 05, 24), 62.99, 63.65, 61.56, 63.34, 62.99, 63.65, 61.56, 63.34, 6);
    priceStorage->addBar(constructDate(2018, 05, 25), 64.26, 64.45, 63.29, 64.33, 64.26, 64.45, 63.29, 64.33, 4);
    priceStorage->addBar(constructDate(2018, 05, 26), 64.31, 64.56, 63.14, 63.55, 64.31, 64.56, 63.14, 63.55, 4);
    priceStorage->addBar(constructDate(2018, 05, 30), 63.29, 63.3, 61.22, 61.22, 63.29, 63.3, 61.22, 61.22, 4);
    priceStorage->addBar(constructDate(2018, 05, 31), 61.76, 61.79, 58.69, 59.77, 61.76, 61.79, 58.69, 59.77, 4);
    priceStorage->addBar(constructDate(2018, 06, 01), 59.85, 62.28, 59.52, 62.17, 59.85, 62.28, 59.52, 62.17, 4);
    priceStorage->addBar(constructDate(2018, 06, 02), 62.99, 63.1, 60.88, 61.66, 62.99, 63.1, 60.88, 61.66, 4);
    priceStorage->addBar(constructDate(2018, 06, 06), 60.22, 60.63, 58.91, 59.72, 60.22, 60.63, 58.91, 59.72, 4);
    priceStorage->addBar(constructDate(2018, 06, 07), 60.1, 60.4, 58.35, 58.56, 60.1, 60.4, 58.35, 58.56, 4);
    priceStorage->addBar(constructDate(2018, 06, 8), 58.44, 60.93, 57.15, 60.76, 58.44, 60.93, 57.15, 60.76, 12);
    priceStorage->addBar(constructDate(2018, 06, 9), 61.18, 61.56, 59.1, 59.24, 61.18, 61.56, 59.1, 59.24, 4);
    priceStorage->addBar(constructDate(2018, 06, 13), 57.61, 59.1, 57.36, 58.33, 57.61, 59.1, 57.36, 58.33, 4);
    priceStorage->addBar(constructDate(2018, 06, 14), 58.28, 58.78, 56.69, 57.61, 58.28, 58.78, 56.69, 57.61, 4);
    priceStorage->addBar(constructDate(2018, 06, 15), 57.3, 59.74, 56.75, 59.38, 57.3, 59.74, 56.75, 59.38, 4);
    priceStorage->addBar(constructDate(2018, 06, 16), 58.96, 59.19, 57.52, 57.56, 58.96, 59.19, 57.52, 57.56, 2);
    priceStorage->addBar(constructDate(2018, 06, 20), 57.61, 58.35, 57.29, 57.47, 57.61, 58.35, 57.29, 57.47, 4);
    priceStorage->addBar(constructDate(2018, 06, 21), 57.74, 58.71, 57.3, 57.86, 57.74, 58.71, 57.3, 57.86, 4);
    priceStorage->addBar(constructDate(2018, 06, 22), 58.2, 59.75, 58.07, 59.58, 58.2, 59.75, 58.07, 59.58, 4);
    priceStorage->addBar(constructDate(2018, 06, 23), 59.72, 60.17, 58.73, 58.83, 59.72, 60.17, 58.73, 58.83, 4);
    priceStorage->addBar(constructDate(2018, 06, 26), 59.17, 59.2, 58.37, 58.99, 59.17, 59.2, 58.37, 58.99, 3);
    priceStorage->addBar(constructDate(2018, 06, 27), 59.09, 59.22, 57.4, 57.43, 59.09, 59.22, 57.4, 57.43, 4);
    priceStorage->addBar(constructDate(2018, 06, 28), 57.29, 57.3, 55.41, 56.02, 57.29, 57.3, 55.41, 56.02, 4);
    priceStorage->addBar(constructDate(2018, 06, 29), 56.76, 59.09, 56.39, 58.97, 56.76, 59.09, 56.39, 58.97, 4);
    priceStorage->addBar(constructDate(2018, 06, 30), 57.59, 57.75, 56.5, 57.27, 57.59, 57.75, 56.5, 57.27, 4);
    priceStorage->addBar(constructDate(2018, 07, 03), 57.52, 58.18, 57.34, 57.95, 57.52, 58.18, 57.34, 57.95, 4);
    priceStorage->addBar(constructDate(2018, 07, 06), 57.09, 57.4, 55.61, 55.77, 57.09, 57.4, 55.61, 55.77, 4);
    priceStorage->addBar(constructDate(2018, 07, 07), 55.48, 56.55, 54.67, 55.4, 55.48, 56.55, 54.67, 55.4, 4);
    priceStorage->addBar(constructDate(2018, 07, 11), 55.11, 55.99, 54.53, 55.65, 55.11, 55.99, 54.53, 55.65, 4);
    priceStorage->addBar(constructDate(2018, 07, 12), 55.17, 55.24, 52.92, 52.96, 55.17, 55.24, 52.92, 52.96, 4);
    priceStorage->addBar(constructDate(2018, 07, 13), 52.03, 54.12, 51.41, 52.25, 52.03, 54.12, 51.41, 52.25, 4);
    priceStorage->addBar(constructDate(2018, 07, 14), 52.5, 52.89, 50.16, 50.67, 52.5, 52.89, 50.16, 50.67, 4);
    priceStorage->addBar(constructDate(2018, 07, 17), 51.73, 53.11, 51.65, 52.37, 51.73, 53.11, 51.65, 52.37, 4);
    priceStorage->addBar(constructDate(2018, 07, 18), 53.16, 53.85, 51.85, 52.9, 53.16, 53.85, 51.85, 52.9, 6);
    priceStorage->addBar(constructDate(2018, 07, 19), 52.96, 55.08, 52.36, 54.1, 52.96, 55.08, 52.36, 54.1, 4);
    priceStorage->addBar(constructDate(2018, 07, 20), 60.96, 61.59, 59.72, 60.5, 60.96, 61.59, 59.72, 60.5, 4);
    priceStorage->addBar(constructDate(2018, 07, 21), 59.82, 61.15, 59.64, 60.72, 59.82, 61.15, 59.64, 60.72, 4);
    priceStorage->addBar(constructDate(2018, 07, 24), 61.26, 62.1, 60.43, 61.42, 61.26, 62.1, 60.43, 61.42, 4);
    priceStorage->addBar(constructDate(2018, 07, 25), 61.78, 62.09, 60.78, 61.93, 61.78, 62.09, 60.78, 61.93, 4);
    priceStorage->addBar(constructDate(2018, 07, 27), 64.5, 65.02, 62.86, 63.4, 64.5, 65.02, 62.86, 63.4, 4);
    priceStorage->addBar(constructDate(2018, 07, 28), 63.94, 65.68, 63.5, 65.59, 63.94, 65.68, 63.5, 65.59, 4);
    priceStorage->addBar(constructDate(2018, 07, 31), 66.83, 68.63, 66.28, 67.96, 66.83, 68.63, 66.28, 67.96, 4);
    priceStorage->addBar(constructDate(2018, 8, 01), 67.22, 67.93, 65.94, 67.18, 67.22, 67.93, 65.94, 67.18, 4);
    priceStorage->addBar(constructDate(2018, 8, 02), 67.65, 68.68, 67.51, 68.16, 67.65, 68.68, 67.51, 68.16, 4);
    priceStorage->addBar(constructDate(2018, 8, 04), 67.05, 68.61, 64.96, 68.3, 67.05, 68.61, 64.96, 68.3, 4);
    priceStorage->addBar(constructDate(2018, 8, 07), 67.72, 69.6, 66.31, 67.21, 67.72, 69.6, 66.31, 67.21, 4);
    priceStorage->addBar(constructDate(2018, 8, 8), 67.09, 67.11, 64.51, 64.78, 67.09, 67.11, 64.51, 64.78, 4);
    priceStorage->addBar(constructDate(2018, 8, 9), 65.43, 65.6, 63.4, 63.59, 65.43, 65.6, 63.4, 63.59, 4);
    priceStorage->addBar(constructDate(2018, 8, 10), 63.25, 64.81, 62.7, 64.07, 63.25, 64.81, 62.7, 64.07, 4);
    priceStorage->addBar(constructDate(2018, 8, 11), 63.23, 64.13, 62.58, 63.65, 63.23, 64.13, 62.58, 63.65, 14);
    priceStorage->addBar(constructDate(2018, 8, 14), 64.05, 65.22, 63.6, 63.94, 64.05, 65.22, 63.6, 63.94, 4);
    priceStorage->addBar(constructDate(2018, 8, 15), 65.34, 66.5, 64.8, 66.45, 65.34, 66.5, 64.8, 66.45, 4);
    priceStorage->addBar(constructDate(2018, 8, 16), 67.1, 68.07, 66.33, 67.98, 67.1, 68.07, 66.33, 67.98, 4);
    priceStorage->addBar(constructDate(2018, 8, 18), 67.71, 68.4, 67.26, 67.91, 67.71, 68.4, 67.26, 67.91, 4);
    priceStorage->addBar(constructDate(2018, 8, 21), 67.3, 67.31, 66.15, 66.56, 67.3, 67.31, 66.15, 66.56, 4);
    priceStorage->addBar(constructDate(2018, 8, 22), 66.68, 68.32, 66.5, 67.62, 66.68, 68.32, 66.5, 67.62, 4);
    priceStorage->addBar(constructDate(2018, 8, 24), 67.89, 68.19, 66.27, 67.81, 67.89, 68.19, 66.27, 67.81, 4);
    priceStorage->addBar(constructDate(2018, 8, 25), 67.34, 69.05, 67.31, 68.75, 67.34, 69.05, 67.31, 68.75, 4);
    priceStorage->addBar(constructDate(2018, 8, 28), 68.5, 68.61, 66.68, 66.98, 68.5, 68.61, 66.68, 66.98, 4);
    priceStorage->addBar(constructDate(2018, 8, 29), 66.99, 67.26, 65.12, 66.48, 66.99, 67.26, 65.12, 66.48, 4);
    priceStorage->addBar(constructDate(2018, 8, 30), 67.34, 67.82, 66.68, 66.96, 67.34, 67.82, 66.68, 66.96, 4);
    priceStorage->addBar(constructDate(2018, 8, 31), 67.28, 68.3, 66.66, 67.85, 67.28, 68.3, 66.66, 67.85, 4);
    priceStorage->addBar(constructDate(2018, 9, 01), 68.48, 68.65, 67.82, 68.38, 68.48, 68.65, 67.82, 68.38, 4);
    priceStorage->addBar(constructDate(2018, 9, 05), 68.97, 71.5, 68.55, 71.48, 68.97, 71.5, 68.55, 71.48, 4);
    priceStorage->addBar(constructDate(2018, 9, 06), 71.08, 71.69, 69.7, 70.03, 71.08, 71.69, 69.7, 70.03, 18);
    priceStorage->addBar(constructDate(2018, 9, 07), 70.6, 73.48, 70.25, 72.8, 70.6, 73.48, 70.25, 72.8, 4);
    priceStorage->addBar(constructDate(2018, 9, 8), 73.37, 73.57, 71.91, 72.52, 73.37, 73.57, 71.91, 72.52, 4);
    priceStorage->addBar(constructDate(2018, 9, 11), 72.43, 73.73, 71.42, 72.5, 72.43, 73.73, 71.42, 72.5, 4);
    priceStorage->addBar(constructDate(2018, 9, 12), 72.81, 73.45, 71.45, 72.63, 72.81, 73.45, 71.45, 72.63, 4);
    priceStorage->addBar(constructDate(2018, 9, 13), 72.85, 74.32, 72.3, 74.2, 72.85, 74.32, 72.3, 74.2, 4);
    priceStorage->addBar(constructDate(2018, 9, 14), 73.72, 74.67, 73.46, 74.17, 73.72, 74.67, 73.46, 74.17, 4);
    priceStorage->addBar(constructDate(2018, 9, 15), 74.6, 74.98, 73.29, 74.1, 74.6, 74.98, 73.29, 74.1, 4);
    priceStorage->addBar(constructDate(2018, 9, 18), 73.8, 74.86, 73.3, 73.89, 73.8, 74.86, 73.3, 73.89, 4);
    priceStorage->addBar(constructDate(2018, 9, 19), 74.1, 74.36, 72.8, 73.77, 74.1, 74.36, 72.8, 73.77, 4);
    priceStorage->addBar(constructDate(2018, 9, 20), 74.38, 75.68, 74.22, 75.26, 74.38, 75.68, 74.22, 75.26, 4);
    priceStorage->addBar(constructDate(2018, 9, 21), 75.25, 76.06, 74.02, 74.65, 75.25, 76.06, 74.02, 74.65, 4);
    priceStorage->addBar(constructDate(2018, 9, 25), 73.81, 75.86, 73.72, 75.75, 73.81, 75.86, 73.72, 75.75, 4);
    priceStorage->addBar(constructDate(2018, 9, 26), 76.18, 77.78, 76.1, 77.61, 76.18, 77.78, 76.1, 77.61, 4);
    priceStorage->addBar(constructDate(2018, 9, 27), 77.17, 77.47, 75.82, 76.41, 77.17, 77.47, 75.82, 76.41, 4);
    priceStorage->addBar(constructDate(2018, 9, 28), 77.02, 77.48, 75.95, 77.01, 77.02, 77.48, 75.95, 77.01, 4);
    priceStorage->addBar(constructDate(2018, 9, 29), 77.11, 77.52, 76.68, 76.98, 77.11, 77.52, 76.68, 76.98, 9);
    priceStorage->addBar(constructDate(2018, 10, 02), 75.1, 75.87, 74.3, 74.86, 75.1, 75.87, 74.3, 74.86, 4);
    priceStorage->addBar(constructDate(2018, 10, 03), 74.45, 74.95, 73.19, 74.08, 74.45, 74.95, 73.19, 74.08, 4);
    priceStorage->addBar(constructDate(2018, 10, 04), 74.1, 75.46, 73.16, 75.38, 74.1, 75.46, 73.16, 75.38, 4);
}