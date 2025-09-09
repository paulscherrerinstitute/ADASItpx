#include <string.h>

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <limits>
#include <cmath>
#include <algorithm>

/* EPICS includes */
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsString.h>

/* areaDetector includes */
#include <ADDriver.h>

/* Device Specific API */
#include "HTTPClient.h"
#include "json.hpp"

#include <iocsh.h>
#include <epicsExport.h>

#define DRIVER_VERSION "1.0.0"

#define ASIExposureModeString       "ASI_EXP_MODE"
#define ASITriggerSoftwareString    "ASI_TRIGGER_SOFT"
#define ASITriggerPolarityString    "ASI_TRIGGER_POL"
#define ASITriggerDelayString       "ASI_TRIGGER_DLY"
#define ASITriggerInString          "ASI_TRIGGER_IN"
#define ASITriggerOutString         "ASI_TRIGGER_OUT"
#define ASIRawEnableString          "ASI_RAW_ENABLE"
#define ASIRawFilePathString        "ASI_RAW_PATH"
#define ASIRawFileTemplateString    "ASI_RAW_TEMPLATE"
#define ASIImageEnableString        "ASI_IMG_ENABLE"
#define ASIImageFilePathString      "ASI_IMG_PATH"
#define ASIImageFileTemplateString  "ASI_IMG_TEMPLATE"
#define ASIPixelModeString          "ASI_PX_MODE"
#define ASIIntegrationSizeString    "ASI_INT_SIZE"
#define ASIIntegrationModeString    "ASI_INT_MODE"
#define ASIDataSourceString         "ASI_DATA_SOURCE"
#define ASIPreviewPeriodString      "ASI_PREVIEW_PERIOD"
#define ASIDroppedFramesString      "ASI_DROPPED_FRAMES"
#define ASITDC1EnableString         "ASI_TDC1_ENABLE"
#define ASITDC1EdgeString           "ASI_TDC1_EDGE"
#define ASITDC1OutputString         "ASI_TDC1_OUT"
#define ASITDC2EnableString         "ASI_TDC2_ENABLE"
#define ASITDC2EdgeString           "ASI_TDC2_EDGE"
#define ASITDC2OutputString         "ASI_TDC2_OUT"
#define ASILocalTemperatureString   "ASI_TEMP_LOC"
#define ASIFPGATemperatureString    "ASI_TEMP_FPGA"
#define ASIChipsTemperatureString   "ASI_TEMP_CHIPS"
#define ASIFansSpeedString          "ASI_FANS_SPEED"
#define ASIHumidityString           "ASI_HUMIDITY"

static const char *driverName = "asiTpx";
static const char *PIXEL_MODE[] = {"count", "tot", "toa", "tof"};
static const char *INTEGRATION_MODE[] = {"sum", "average", "last"};
static const char *TDC_EDGE[] = {"P", "N", "PN"}; /* Rising, Falling, Both */
static const char *TDC_OUTPUT[] = {"0123", "0", "1", "2", "3"}; /* All, Channel 0 - 3 */

static void asiTpxAcquisitionTaskC(void *drvPvt);
static void asiTpxPollTaskC(void *drvPvt);
static void asiTpxImageTaskC(void *drvPvt);

enum {
    DATA_SOURCE_NONE = 0,
    DATA_SOURCE_PREVIEW = 1,
    DATA_SOURCE_IMAGE = 2
};

class asiTpx : public ADDriver
{
public:
    asiTpx(const char *portName, const char *configFile, int maxBuffers, size_t maxMemory, int priority, int stackSize);
    virtual ~asiTpx();
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    void report(FILE *fp, int details);
    void asiTpxAcquisitionTask();
    void asiTpxPollTask();
    void asiTpxImageTask();

protected:
    asynStatus connectServer();
    asynStatus startMeasurement();
    asynStatus stopMeasurement();

    NDArray *readJsonImage(SOCKET s);

    int ASIExposureMode;
#define FIRST_ASITPX_PARAM ASIExposureMode
    int ASITriggerSoftware;
    int ASITriggerPolarity;
    int ASITriggerDelay;
    int ASITriggerIn;
    int ASITriggerOut;
    int ASIRawEnable;
    int ASIRawFilePath;
    int ASIRawFileTemplate;
    int ASIImageEnable;
    int ASIImageFilePath;
    int ASIImageFileTemplate;
    int ASIPixelMode;
    int ASIIntegrationSize;
    int ASIIntegrationMode;
    int ASIDataSource;
    int ASIPreviewPeriod;
    int ASIDroppedFrames;
    int ASITDC1Enable;
    int ASITDC1Edge;
    int ASITDC1Output;
    int ASITDC2Enable;
    int ASITDC2Edge;
    int ASITDC2Output;
    int ASILocalTemperature;
    int ASIFPGATemperature;
    int ASIChipsTemperature;
    int ASIFansSpeed;
    int ASIHumidity;
#define LAST_ASITPX_PARAM ASIHumidity

private:
    epicsEventId startEventId;
    epicsEventId stopEventId;

    nlohmann::json systemConfig;

    HTTPClient httpClient;
};

/* Number of asyn parameters (asyn commands) this driver supports*/
#define NUM_ASITPX_PARAMS (&LAST_ASITPX_PARAM - &FIRST_ASITPX_PARAM + 1)

/* asiTpx destructor */
asiTpx::~asiTpx()
{
}

/* asiTpx constructor */
asiTpx::asiTpx(const char *portName, const char *configFile, int maxBuffers, size_t maxMemory, int priority, int stackSize)
    : ADDriver(portName, 1, (int)NUM_ASITPX_PARAMS, maxBuffers, maxMemory,
               asynEnumMask | asynFloat64ArrayMask, asynEnumMask | asynFloat64ArrayMask,
               ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=0, autoConnect=1 */
               priority, stackSize)
{
    int status = asynSuccess;
    const char *functionName = "asiTpx";

    createParam(ASIExposureModeString,    asynParamInt32, &ASIExposureMode);
    createParam(ASITriggerSoftwareString, asynParamInt32, &ASITriggerSoftware);
    createParam(ASITriggerPolarityString, asynParamInt32, &ASITriggerPolarity);
    createParam(ASITriggerDelayString,    asynParamFloat64, &ASITriggerDelay);
    createParam(ASITriggerInString,       asynParamInt32, &ASITriggerIn);
    createParam(ASITriggerOutString,      asynParamInt32, &ASITriggerOut);
    createParam(ASIRawEnableString,       asynParamInt32, &ASIRawEnable);
    createParam(ASIRawFilePathString,     asynParamOctet, &ASIRawFilePath);
    createParam(ASIRawFileTemplateString, asynParamOctet, &ASIRawFileTemplate);
    createParam(ASIImageEnableString,       asynParamInt32, &ASIImageEnable);
    createParam(ASIImageFilePathString,     asynParamOctet, &ASIImageFilePath);
    createParam(ASIImageFileTemplateString, asynParamOctet, &ASIImageFileTemplate);
    createParam(ASIPixelModeString,         asynParamInt32, &ASIPixelMode);
    createParam(ASIIntegrationSizeString,   asynParamInt32, &ASIIntegrationSize);
    createParam(ASIIntegrationModeString,   asynParamInt32, &ASIIntegrationMode);
    createParam(ASIDataSourceString,        asynParamInt32, &ASIDataSource);
    createParam(ASIPreviewPeriodString,     asynParamFloat64, &ASIPreviewPeriod);
    createParam(ASIDroppedFramesString,     asynParamInt32, &ASIDroppedFrames);
    createParam(ASITDC1EnableString,       asynParamInt32, &ASITDC1Enable);
    createParam(ASITDC1EdgeString,         asynParamInt32, &ASITDC1Edge);
    createParam(ASITDC1OutputString,       asynParamInt32, &ASITDC1Output);
    createParam(ASITDC2EnableString,       asynParamInt32, &ASITDC2Enable);
    createParam(ASITDC2EdgeString,         asynParamInt32, &ASITDC2Edge);
    createParam(ASITDC2OutputString,       asynParamInt32, &ASITDC2Output);
    createParam(ASILocalTemperatureString, asynParamFloat64, &ASILocalTemperature);
    createParam(ASIFPGATemperatureString,  asynParamFloat64, &ASIFPGATemperature);
    createParam(ASIChipsTemperatureString, asynParamOctet, &ASIChipsTemperature);
    createParam(ASIFansSpeedString,        asynParamOctet, &ASIFansSpeed);
    createParam(ASIHumidityString,         asynParamFloat64, &ASIHumidity);

    /* Initialise variables */
    setStringParam(NDDriverVersion, DRIVER_VERSION);
    setStringParam(ADManufacturer, "Amsterdam Scientific Instruments");

    /* Read system config file */
    try
    {
        systemConfig = nlohmann::json::parse(std::ifstream(configFile));
    }
    catch (...)
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: Error in reading system config file %s\n",
                  driverName, functionName, configFile);
#if ADCORE_VERSION > 3 || (ADCORE_VERSION == 3 && ADCORE_REVISION >= 10)
        this->deviceIsReachable = false;
#endif
        this->disconnect(pasynUserSelf);
        setIntegerParam(ADStatus, ADStatusDisconnected);
        setStringParam(ADStatusMessage, "Error in system config file");
        return;
    }

    /* Connect serval server */
    if (connectServer())
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: Error in connectServer\n",
                  driverName, functionName);
#if ADCORE_VERSION > 3 || (ADCORE_VERSION == 3 && ADCORE_REVISION >= 10)
        this->deviceIsReachable = false;
#endif
        this->disconnect(pasynUserSelf);
        setIntegerParam(ADStatus, ADStatusDisconnected);
        setStringParam(ADStatusMessage, "No connection to serval server");
        return;
    }

    /* Signal to the acquistion task to start the acquisition */
    this->startEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->startEventId)
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s epicsEventCreate failure for start event\n",
                  driverName, functionName);
    }

    /* Signal to the acquistion task to stop the acquisition */
    this->stopEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->stopEventId)
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: epicsEventCreate failure for stop event\n",
                  driverName, functionName);
    }

    /* Create the acquisition thread */
    status = (epicsThreadCreate("ASItpxAcquisitionTask",
                                epicsThreadPriorityMedium, epicsThreadGetStackSize(epicsThreadStackMedium),
                                (EPICSTHREADFUNC)asiTpxAcquisitionTaskC, this) == NULL);

    if (status)
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: epicsTheadCreate failure for acquisition task\n",
                  driverName, functionName);
    }

    /* Create the image receiver thread */
    status = (epicsThreadCreate("ASItpxImageTask",
        epicsThreadPriorityMedium, epicsThreadGetStackSize(epicsThreadStackMedium),
        (EPICSTHREADFUNC)asiTpxImageTaskC, this) == NULL);

    /* Create the poll thread */
    status = (epicsThreadCreate("ASItpxPollTask",
                                epicsThreadPriorityMedium, epicsThreadGetStackSize(epicsThreadStackMedium),
                                (EPICSTHREADFUNC)asiTpxPollTaskC, this) == NULL);

    if (status)
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: epicsTheadCreate failure for poll task\n",
                  driverName, functionName);
    }
}

static void asiTpxAcquisitionTaskC(void *drvPvt)
{
    asiTpx *pPvt = (asiTpx *)drvPvt;
    pPvt->asiTpxAcquisitionTask();
}

/** Task to acquire from the detector and send them up to areaDetector.
 *
 *  It is started in the class constructor and must not return until the IOC stops.
 *
 */
void asiTpx::asiTpxAcquisitionTask()
{
    int adStatus;
    int acquire;
    int arrayCallbacks;
    int numImagesCounter = 0;
    double acquirePeriod, previewPeriod;
    double timeRemaining = 0;
    int droppedFrames = 0;
    epicsTimeStamp startTime, endTime;
    std::string statusMessage;
    std::string response;
    const char *functionName = "asiTpxAcquisitionTask";

    this->lock();
    while (1)
    {
        getIntegerParam(ADAcquire, &acquire);
        getIntegerParam(ADStatus, &adStatus);
        /* If we are not acquiring or encountered a problem then wait for a semaphore
         * that is given when acquisition is started */
        if (!acquire)
        {
            /* Only set the status message if we didn't encounter a problem last time,
             * so we don't overwrite the error mesage */
            if (adStatus == (int)ADStatusIdle)
            {
                asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                          "%s:%s: Waiting for the acquire command\n",
                          driverName, functionName);
                setStringParam(ADStatusMessage, "Waiting for the acquire command");
                callParamCallbacks();
            }
            /* Release the lock while we wait for the start event then lock again */
            this->unlock();
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s:%s: Waiting for acquire to start\n",
                      driverName, functionName);
            epicsEventWait(this->startEventId);
            this->lock();
            /* We are acquiring. */
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s:%s: We are acquiring\n",
                      driverName, functionName);

            if (startMeasurement() != asynSuccess)
            {
                setIntegerParam(ADAcquire, 0);
                setIntegerParam(ADStatus, ADStatusError);
                callParamCallbacks();
                continue;
            }

            numImagesCounter = 0;
            getIntegerParam(ADAcquire, &acquire);
            setIntegerParam(ADNumImagesCounter, numImagesCounter);
            timeRemaining = 0;
            setDoubleParam(ADTimeRemaining, timeRemaining);
            droppedFrames = 0;
            setIntegerParam(ASIDroppedFrames, droppedFrames);
            setIntegerParam(ADStatus, ADStatusAcquire);
            setStringParam(ADStatusMessage, "Acquiring....");
            callParamCallbacks();
        }
        epicsTimeGetCurrent(&startTime);

        getDoubleParam(ADAcquirePeriod, &acquirePeriod);
        getDoubleParam(ASIPreviewPeriod, &previewPeriod);
        getIntegerParam(NDArrayCallbacks, &arrayCallbacks);

        this->unlock();

        /* Poll measurement progress */
        if (httpClient.get("/dashboard", response))
        {
            auto dashboard = nlohmann::json::parse(response);
            numImagesCounter = dashboard["Measurement"]["FrameCount"];
            timeRemaining = dashboard["Measurement"]["TimeLeft"];
            droppedFrames = dashboard["Measurement"]["DroppedFrames"];
            if (dashboard["Measurement"]["Status"] == "DA_IDLE") {
                acquire = 0;
                /* serval server (version 2022/06/21 14:04) does not set
                 * remaining time to 0 after finish. So we force that. */
                timeRemaining = 0;
            }
        }
        this->lock();

        setDoubleParam(ADTimeRemaining, timeRemaining);
        setIntegerParam(ASIDroppedFrames, droppedFrames);
        setIntegerParam(ADNumImagesCounter, numImagesCounter);

        /* Check to see if acquisition is complete */
        if (!acquire)
        {
            setIntegerParam(ADAcquire, 0);
            setIntegerParam(ADStatus, ADStatusIdle);
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s:%s: Acquisition completed\n",
                      driverName, functionName);
        }

        /* Call the callbacks to update any changes */
        callParamCallbacks();

        /* Sync polling frequency with acquisitio period */
        epicsTimeGetCurrent(&endTime);
        double elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
        double delay = acquirePeriod - elapsedTime;
        if (delay > 0.0)
            epicsThreadSleep(delay);
    }
}

static void asiTpxPollTaskC(void *drvPvt)
{
    asiTpx *pPvt = (asiTpx *)drvPvt;
    pPvt->asiTpxPollTask();
}

/** Task to poll live images from the detector and send them up to areaDetector.
 *
 *  It is started in the class constructor and must not return until the IOC stops.
 *
 */
void asiTpx::asiTpxPollTask()
{
    std::string response;

    while (true)
    {
        /* Detector health */
        if (httpClient.get("/detector/health", response))
        {
            auto health = nlohmann::json::parse(response);
            std::string fans = "[";
            for (auto &item : health.items())
            {
                if (item.key().find("Fan") == 0)
                    fans += std::to_string(item.value().get<int>()) + ",";
            }
            if (fans.length() > 1)
                fans.pop_back();
            fans += "]";

            this->lock();
            setDoubleParam(ASILocalTemperature, health["LocalTemperature"].get<double>());
            setDoubleParam(ASIFPGATemperature, health["FPGATemperature"].get<double>());
            setDoubleParam(ASIHumidity, health["Humidity"].get<double>());
            setStringParam(ASIChipsTemperature, health["ChipTemperatures"].dump());
            setStringParam(ASIFansSpeed, fans);
            callParamCallbacks();
            this->unlock();
        }
        epicsThreadSleep(5);
    }
}

static void asiTpxImageTaskC(void *drvPvt)
{
    asiTpx *pPvt = (asiTpx *)drvPvt;
    pPvt->asiTpxImageTask();
}

/** Task to receive image from the detector and send them up to areaDetector */
void asiTpx::asiTpxImageTask()
{
    const char *functionName = "asiTpxImageTask";

    /* TCP server to receive images */
    SOCKET s = epicsSocketCreate(PF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
        char error[128];
        epicsSocketConvertErrnoToString(error, sizeof(error));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: Failed to create socket: %s\n",
                  driverName, functionName, error);
        return;
    }
    epicsSocketEnableAddressReuseDuringTimeWaitState(s);

    struct sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    std::string receiverAddress = systemConfig["ImageReceiver"]["Address"];
    size_t pos = receiverAddress.find(":");
    if (pos == std::string::npos) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: Wrong receiver address: %s\n",
            driverName, functionName, receiverAddress.c_str());
        return;
    }

    hostToIPAddr(receiverAddress.substr(0, pos).c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(std::stoi(receiverAddress.substr(pos + 1))); 

    int status = bind(s, (struct sockaddr *) &serverAddr, sizeof (serverAddr));
    if (status < 0) {
        char error[128];
        epicsSocketConvertErrnoToString(error, sizeof(error));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: Failed to create socket: %s\n",
                  driverName, functionName, error);
        epicsSocketDestroy(s);
        return;
    }

    if (listen(s, 0) < 0) {
        char error[128];
        epicsSocketConvertErrnoToString(error, sizeof(error));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: Failed to listen on socket: %s\n",
                  driverName, functionName, error);
        epicsSocketDestroy(s);
        return;
    }

    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        SOCKET c = epicsSocketAccept(s, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (SOCKERRNO == SOCK_EBADF || SOCKERRNO == SOCK_ENOTSOCK) {
            char error[128];
            epicsSocketConvertErrnoToString(error, sizeof(error));
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s:%s: Failed to accept connection: %s\n",
                      driverName, functionName, error);
            break;
        }
        if (c == INVALID_SOCKET) {
            char error[128];
            epicsSocketConvertErrnoToString(error, sizeof(error));
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s:%s: Failed to accept connection: %s\n",
                      driverName, functionName, error);
            continue;
        }

        while (NDArray *pArray = readJsonImage(c)) {
            lock();
            int imageCounter, arrayCallbacks;
            getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
            getIntegerParam(NDArrayCounter, &imageCounter);
            imageCounter++;
            setIntegerParam(NDArrayCounter, imageCounter);

            /* Put the frame number and time stamp into the buffer */
            pArray->uniqueId = imageCounter;
            updateTimeStamp(&pArray->epicsTS);

            /* Get any attributes that have been defined for this driver */
            this->getAttributes(pArray->pAttributeList);
            this->unlock();

            if (arrayCallbacks)
            {
                asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                            "%s:%s: Calling NDArray callback\n",
                            driverName, functionName);
                doCallbacksGenericPointer(pArray, NDArrayData, 0);
            }
            /* Free the image buffer */
            pArray->release();
        }

        epicsSocketDestroy(c);
    }

    epicsSocketDestroy(s);
}

/** Called when asyn clients call pasynInt32->write().
 * Write integer value to the drivers parameter table.
 * \param[in] pasynUser pasynUser structure that encodes the reason and address.
 * \param[in] value The value for this parameter
 * \return asynStatus Either asynError or asynSuccess
 */
asynStatus asiTpx::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int status = asynSuccess;
    int function = pasynUser->reason;
    int oldValue;
    const char *functionName = "writeInt32";

    getIntegerParam(function, &oldValue);
    status = setIntegerParam(function, value);

    if (function == ADAcquire)
    {
        int adStatus;
        getIntegerParam(ADStatus, &adStatus);

        if (value && adStatus != (int)ADStatusAcquire)
        {
            epicsEventSignal(this->startEventId);
        }

        if (!value && adStatus == (int)ADStatusAcquire)
        {
            if (stopMeasurement() == asynSuccess)
            {
                setIntegerParam(ADStatus, ADStatusAborted);
                setStringParam(ADStatusMessage, "Aborted by user");
            }
        }
    }
    else if (function == ASITriggerSoftware)
    {
        std::string response;
        int triggerMode, exposureMode;
        getIntegerParam(ADTriggerMode, &triggerMode);
        getIntegerParam(ASIExposureMode, &exposureMode);
        if (value)
        {
            if (!httpClient.get("/measurement/trigger/start", response))
            {
                setStringParam(ADStatusMessage, "Failed to start software trigger");
                asynPrint(pasynUser, ASYN_TRACE_ERROR,
                          "%s:%s /measurement/trigger/start %s\n",
                          driverName, functionName, response.c_str());
            }
            if (triggerMode == 2 && exposureMode == 0)
                setIntegerParam(function, 0);
        }
        else
        {
            if (triggerMode == 2 && exposureMode == 1)
                if (!httpClient.get("/measurement/trigger/stop", response))
                {
                    setStringParam(ADStatusMessage, "Failed to stop software trigger");
                    asynPrint(pasynUser, ASYN_TRACE_ERROR,
                              "%s:%s /measurement/trigger/stop %s\n",
                              driverName, functionName, response.c_str());
                }
        }
    }
    else if (function == NDDataType)
    {
        /* Lock the data type to Float32 as all
         * data collected will be of float type */
        setIntegerParam(NDDataType, NDFloat32);
    }
    else if (function == NDColorMode)
    {
        /* Lock the color mode to mono as all
         * data collected will be monochrome */
        setIntegerParam(NDColorMode, NDColorModeMono);
    }
    else
    {
        /* If this is not a parameter we have handled call the base class */
        if (function < FIRST_ASITPX_PARAM)
            status = ADDriver::writeInt32(pasynUser, value);
    }

    /* Do callbacks so higher layers see any changes */
    status = (asynStatus)callParamCallbacks();
    if (status)
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s:%s: error, status=%d function=%d, value=%d\n",
                  driverName, functionName, status, function, value);
    else
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                  "%s:%s: function=%d, value=%d\n",
                  driverName, functionName, function, value);
    return asynSuccess;
}

/** Report status of the driver for debugging/testing purpose.
 * Can be invoked from ioc shell. "asynReport <details>,<port>"
 * Prints details about the driver if details>0.
 * It then calls the ADDriver::report() method.
 * \param[in] fp File pointed passed by caller where the output is written to.
 * \param[in] details If >0 then driver details are printed.
 */
void asiTpx::report(FILE *fp, int details)
{
    fprintf(fp, "asiTpx detector %s\n", this->portName);
    fprintf(fp, "  serval address:      %s\n", this->systemConfig["Server"]["Address"].get<std::string>().c_str());

    if (details > 0)
    {
        int nx, ny, dataType;
        getIntegerParam(ADSizeX, &nx);
        getIntegerParam(ADSizeY, &ny);
        getIntegerParam(NDDataType, &dataType);
        fprintf(fp, "  NX, NY:              %d  %d\n", nx, ny);
        fprintf(fp, "  Data type:           %d\n", dataType);
    }
    /* Invoke the base class method */
    ADDriver::report(fp, details);
}

/** Connect to serval server.
 * It is normally called at the configuration time, but can also
 * be called to reconnect after serval server restart.
 */
asynStatus asiTpx::connectServer()
{
    std::string response;
    const char *functionName = "connectServer";

    if (!systemConfig.contains("/Server/Address"_json_pointer))
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: Server Address is not configured\n",
                  driverName, functionName);
        return asynError;
    }

    if (!httpClient.setHost(systemConfig["Server"]["Address"]))
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: Invalid Server Address '%s'\n",
                  driverName, functionName, systemConfig["Server"]["Address"].get<std::string>().c_str());
        return asynError;
    }

    /* Parse server and detector info */
    if (!httpClient.get("/dashboard", response))
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: /dashboard %s\n",
                  driverName, functionName, response.c_str());
        return asynError;
    }

    auto dashboard = nlohmann::json::parse(response);
    setStringParam(ADModel, dashboard["Detector"]["DetectorType"].get<std::string>());
    setStringParam(ADSDKVersion,
        dashboard["Server"]["SoftwareVersion"].get<std::string>() + " b" +
        dashboard["Server"]["SoftwareBuild"].get<std::string>() + "(" +
        dashboard["Server"]["SoftwareCommit"].get<std::string>() + ")");

    if (!httpClient.get("/detector/info", response))
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: /detector/info %s\n",
                  driverName, functionName, response.c_str());
        return asynError;
    }

    auto info = nlohmann::json::parse(response);
    setStringParam(ADFirmwareVersion, info["FW_version"].get<std::string>());

    setIntegerParam(ADMaxSizeX, info["PixCount"].get<int>() / info["NumberOfRows"].get<int>());
    setIntegerParam(ADMaxSizeY, info["NumberOfRows"].get<int>());

    /* Load pixelConfig and DACS */
    if (!systemConfig.contains("/Detector/Config/DACS"_json_pointer))
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: Detector DACS is not set in config file\n",
                  driverName, functionName);
        return asynError;
    }

    auto dacsFile = systemConfig["Detector"]["Config"]["DACS"].get<std::string>();
    if (!httpClient.get("/config/load?format=dacs&file=" + dacsFile, response))
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: Failed to load DACS file '%s'. %s\n",
                  driverName, functionName, dacsFile.c_str(), response.c_str());
        return asynError;
    }

    if (!systemConfig.contains("/Detector/Config/PixelConfig"_json_pointer))
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: Detector PixelConfig is not set in config file\n",
                  driverName, functionName);
        return asynError;
    }
    auto bpcFile = systemConfig["Detector"]["Config"]["PixelConfig"].get<std::string>();
    if (!httpClient.get("/config/load?format=pixelconfig&file=" + bpcFile, response))
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: Failed to load PixelConfig file '%s'. %s\n",
                  driverName, functionName, bpcFile.c_str(), response.c_str());
        return asynError;
    }

    return asynSuccess;
}

/** Start measurement
 *
 */
asynStatus asiTpx::startMeasurement()
{
    double acquireTime, acquirePeriod;
    int imageMode, numImages;
    int triggerMode, exposureMode, triggerPolarity, triggerIn, triggerOut;
    double triggerDelay;
    int tdc1Enable, tdc1Edge, tdc1Output, tdc2Enable, tdc2Edge, tdc2Output;
    std::string tdc1, tdc2;
    int rawEnabled, imageEnabled, dataSource;
    int mode, integrationSize, integrationMode;
    double previewPeriod;
    std::string response, message;
    const char *functionName = "startMeasurement";

    /* Get measurement inputs */
    getDoubleParam(ADAcquireTime, &acquireTime);
    getDoubleParam(ADAcquirePeriod, &acquirePeriod);

    getIntegerParam(ADTriggerMode, &triggerMode);
    getIntegerParam(ASIExposureMode, &exposureMode);
    getIntegerParam(ASITriggerPolarity, &triggerPolarity);
    getDoubleParam(ASITriggerDelay, &triggerDelay);
    getIntegerParam(ASITriggerIn, &triggerIn);
    getIntegerParam(ASITriggerOut, &triggerOut);

    getIntegerParam(ASITDC1Enable, &tdc1Enable);
    getIntegerParam(ASITDC1Edge, &tdc1Edge);
    getIntegerParam(ASITDC1Output, &tdc1Output);
    getIntegerParam(ASITDC2Enable, &tdc2Enable);
    getIntegerParam(ASITDC2Edge, &tdc2Edge);
    getIntegerParam(ASITDC2Output, &tdc2Output);

    getIntegerParam(ADNumImages, &numImages);
    getIntegerParam(ADImageMode, &imageMode);

    getIntegerParam(ASIRawEnable, &rawEnabled);
    getIntegerParam(ASIImageEnable, &imageEnabled);
    getIntegerParam(ASIDataSource, &dataSource);
    getDoubleParam(ASIPreviewPeriod, &previewPeriod);

    getIntegerParam(ASIPixelMode, &mode);
    getIntegerParam(ASIIntegrationSize, &integrationSize);
    getIntegerParam(ASIIntegrationMode, &integrationMode);

    /* At least one should be enabled */
    if (!rawEnabled && !imageEnabled && dataSource == DATA_SOURCE_NONE)
    {
        setStringParam(ADStatusMessage, "Enable at least one data output");
        return asynError;
    }

    /* Configure detector */
    auto config = nlohmann::json::object();
    config["ExposureTime"] = acquireTime;
    config["TriggerPeriod"] = acquirePeriod;
    config["nTriggers"] = (imageMode == ADImageSingle) ? 1 : numImages;
    config["TriggerDelay"] = triggerDelay;
    config["TriggerIn"] = triggerIn;
    config["TriggerOut"] = triggerOut;
    if (triggerMode == ADTriggerInternal)
    {
        if (exposureMode == 0)
        {
            config["TriggerMode"] = "AUTOTRIGSTART_TIMERSTOP";
        }
        else
        {
            config["TriggerPeriod"] = acquireTime;
            config["TriggerMode"] = "CONTINUOUS";
        }
    }
    else if (triggerMode == ADTriggerExternal)
    {
        if (exposureMode == 0)
            config["TriggerMode"] = triggerPolarity ? "PEXSTART_TIMERSTOP" : "NEXSTART_TIMERSTOP";
        else
            config["TriggerMode"] = triggerPolarity ? "PEXSTART_NEXSTOP" : "NEXSTART_PEXSTOP";
    }
    else if (triggerMode == 2) /* Software */
    {
        if (exposureMode == 0)
            config["TriggerMode"] = "SOFTWARESTART_TIMERSTOP";
        else
            config["TriggerMode"] = "SOFTWARESTART_SOFTWARESTOP";
    }

    if (tdc1Enable)
        tdc1 = std::string(TDC_EDGE[tdc1Edge]) + TDC_OUTPUT[tdc1Output];

    if (tdc2Enable)
        tdc2 = std::string(TDC_EDGE[tdc2Edge]) + TDC_OUTPUT[tdc2Output];

    config["Tdc"] = {tdc1, tdc2};

    if (!httpClient.put("/detector/config", config.dump(), response))
    {
        setStringParam(ADStatusMessage, "Failed to configure detector");
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: /detector/config %s\n",
                  driverName, functionName, response.c_str());
        return asynError;
    }

    /* Configure destination */
    auto destination = nlohmann::json::object();
    if (httpClient.get("/server/destination", response))
        destination = nlohmann::json::parse(response);

    if (rawEnabled)
    {
        std::string path, pattern;

        getStringParam(ASIRawFilePath, path);
        getStringParam(ASIRawFileTemplate, pattern);
        if (path.find("tcp://") == std::string::npos && path.find("http://") == std::string::npos)
            path = "file://" + path;
        else
            if (destination["Raw"][0].contains("SplitStrategy"))
                destination["Raw"][0].erase("SplitStrategy");

        destination["Raw"][0]["Base"] = path;
        destination["Raw"][0]["FilePattern"] = pattern;
    }
    else
    {
        destination["Raw"] = nlohmann::json();
    }

    destination["Image"] = nlohmann::json::array();
    destination["Preview"] = nlohmann::json();

    if (imageEnabled)
    {
        std::string path, pattern;

        getStringParam(ASIImageFilePath, path);
        getStringParam(ASIImageFileTemplate, pattern);
        if (path.find("tcp://") == std::string::npos && path.find("http://") == std::string::npos)
            path = "file://" + path;

        auto userImageDestination = nlohmann::json({
            {"Base", path},
            {"FilePattern", pattern},
            {"Mode", PIXEL_MODE[mode]},
            {"IntegrationSize", integrationSize}
        });

        if (integrationSize != 0 && integrationSize != 1)
            userImageDestination["IntegrationMode"] = INTEGRATION_MODE[integrationMode];

        if (path.find("tcp://") == 0)
            userImageDestination["Format"] = "jsonimage";
        else
            userImageDestination["Format"] = "tiff";

        destination["Image"].push_back(userImageDestination);
    }

    if (dataSource != DATA_SOURCE_NONE)
    {
        /* Receive Image/Preview jsonimage with the same receiver. */
        auto internalImageDestination = nlohmann::json({
            {"Base", "tcp://connect@" + systemConfig["ImageReceiver"]["Address"].get<std::string>()},
            {"Format", "jsonimage"},
            {"Mode", PIXEL_MODE[mode]},
            {"IntegrationSize", integrationSize}
        });

        if (integrationSize != 0 && integrationSize != 1)
            internalImageDestination["IntegrationMode"] = INTEGRATION_MODE[integrationMode];

        if (dataSource == DATA_SOURCE_IMAGE) {
            destination["Image"].push_back(internalImageDestination);
        } else if (dataSource == DATA_SOURCE_PREVIEW) {
            destination["Preview"]["Period"] = std::max<double>(acquirePeriod, previewPeriod);
            destination["Preview"]["SamplingMode"] = "skipOnFrame";
            destination["Preview"]["ImageChannels"][0] = internalImageDestination;
        }
    }

    if (!httpClient.put("/server/destination", destination.dump(), response))
    {
        setStringParam(ADStatusMessage, "Failed to set output destination");
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s /server/destination %s\n",
                  driverName, functionName, response.c_str());
        return asynError;
    }

    /* Acquire */
    if (!httpClient.get("/measurement/start", response))
    {
        setStringParam(ADStatusMessage, "Failed to start measurement");
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: /measurement/start %s\n",
                  driverName, functionName, response.c_str());
        return asynError;
    }

    return asynSuccess;
}

/** Start measurement
 *
 */
asynStatus asiTpx::stopMeasurement()
{
    std::string response;
    const char *functionName = "stopMeasurement";

    if (!httpClient.get("/measurement/stop", response))
    {
        setStringParam(ADStatusMessage, "Failed to abort acquisition");
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:%s: /measurement/stop %s\n",
                  driverName, functionName, response.c_str());
        return asynError;
    }

    return asynSuccess;
}

/**
 * Read jsonimage message
 */
NDArray *asiTpx::readJsonImage(SOCKET s)
{
    const char *functionName = "readJsonImage";

    std::string headerString;
    char ch;
    int n;

    /* Read and decode json header */
    while (true) {
        n = recv(s, &ch, 1, 0);
        if (n <= 0) return NULL;
        if (ch == '\n') break;
        headerString += ch;
    }

    nlohmann::json header;
    size_t dataSize;
    int bitDepth;
    size_t dims[2];
    int frameNumber;
    double timestamp;
    try {
        header = nlohmann::json::parse(headerString);
        dataSize = header["dataSize"].get<int>();
        bitDepth = header["bitDepth"].get<int>();
        dims[0] = header["width"].get<int>();
        dims[1] = header["height"].get<int>();
        frameNumber = header["frameNumber"].get<int>();
        timestamp = header["timeAtFrame"].get<double>();
    } catch (std::exception &e) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: Failed to parse json image header %s\n",
            driverName, functionName, e.what());
        return NULL;
    }

    NDDataType_t dataType;
    if (bitDepth == 16)
        dataType = NDUInt16;
    else if (bitDepth == 32)
        dataType = NDUInt32;
    else {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: Invalid bit depth %d\n",
            driverName, functionName, bitDepth);
        return NULL;
    }

    if (dataSize != dims[0] * dims[1] * bitDepth / 8) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: Invalid image size %zu != %zu x %zu x %d\n",
            driverName, functionName, dataSize, dims[0], dims[1], bitDepth/8);
        return NULL;
    }
    NDArray *pArray = this->pNDArrayPool->alloc(2, dims, dataType, 0, NULL);
    pArray->pAttributeList->add("FrameNumber", "Frame Number", NDAttrInt32, &frameNumber);
    pArray->timeStamp = timestamp;

    size_t read = 0;
    while (read < dataSize) {
        n = recv(s, (char*)pArray->pData + read, dataSize - read, 0);
        if (n <= 0) {
            char error[128];
            epicsSocketConvertErrnoToString(error, sizeof(error));
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: Failed to receive data %s\n",
                driverName, functionName, error);
            pArray->release();
            return NULL;
        }
        read += n;
    }

    /* Read and discard trailing newline */
    n = recv(s, &ch, 1, 0);
    if (n <= 0 || ch != '\n') {
        char error[128];
        epicsSocketConvertErrnoToString(error, sizeof(error));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: Failed to receive trailing newline %s\n",
            driverName, functionName, error);
        pArray->release();
        return NULL;
    }

    return pArray;
}


/** Configuration command, called directly or from iocsh */
extern "C" int asiTpxConfig(const char *portName, const char *configFile,
                            int maxBuffers, int maxMemory, int priority, int stackSize)
{
    new asiTpx(portName, configFile,
               (maxBuffers < 0) ? 0 : maxBuffers,
               (maxMemory < 0) ? 0 : maxMemory,
               priority, stackSize);
    return (asynSuccess);
}

/** Code for iocsh registration */
static const iocshArg asiTpxConfigArg0 = {"Port name", iocshArgString};
static const iocshArg asiTpxConfigArg1 = {"Config file", iocshArgString};
static const iocshArg asiTpxConfigArg2 = {"maxBuffers", iocshArgInt};
static const iocshArg asiTpxConfigArg3 = {"maxMemory", iocshArgInt};
static const iocshArg asiTpxConfigArg4 = {"priority", iocshArgInt};
static const iocshArg asiTpxConfigArg5 = {"stackSize", iocshArgInt};
static const iocshArg *const asiTpxConfigArgs[] = {&asiTpxConfigArg0,
                                                   &asiTpxConfigArg1,
                                                   &asiTpxConfigArg2,
                                                   &asiTpxConfigArg3,
                                                   &asiTpxConfigArg4,
                                                   &asiTpxConfigArg5};
static const iocshFuncDef configasiTpx = {"asiTpxConfig", 6, asiTpxConfigArgs};
static void configasiTpxCallFunc(const iocshArgBuf *args)
{
    asiTpxConfig(args[0].sval, args[1].sval,
                 args[2].ival, args[3].ival, args[4].ival, args[5].ival);
}

static void asiTpxRegister(void)
{
    iocshRegister(&configasiTpx, configasiTpxCallFunc);
}

extern "C"
{
    epicsExportRegistrar(asiTpxRegister);
}
