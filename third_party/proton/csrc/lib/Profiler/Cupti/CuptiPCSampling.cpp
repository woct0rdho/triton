#include "Profiler/Cupti/CuptiPCSampling.h"
#include "Data/Metric.h"
#include "Driver/GPU/CudaApi.h"
#include "Driver/GPU/CuptiApi.h"
#include "Utility/Atomic.h"
#include "Utility/Map.h"
#include "Utility/String.h"
#include <memory>
#include <stdexcept>
#include <tuple>

namespace proton {

namespace {

uint64_t getCubinCrc(const char *cubin, size_t size) {
  CUpti_GetCubinCrcParams cubinCrcParams = {
      CUpti_GetCubinCrcParamsSize,
      size,
      cubin,
      0,
  };
  cupti::getCubinCrc<true>(&cubinCrcParams);
  return cubinCrcParams.cubinCrc;
}

size_t getNumStallReasons(CUcontext context) {
  size_t numStallReasons = 0;
  CUpti_PCSamplingGetNumStallReasonsParams numStallReasonsParams = {
      CUpti_PCSamplingGetNumStallReasonsParamsSize,
      NULL,
      context,
      &numStallReasons};
  cupti::pcSamplingGetNumStallReasons<true>(&numStallReasonsParams);
  return numStallReasons;
}

std::tuple<uint32_t, std::string, std::string>
getSassToSourceCorrelation(const char *functionName, uint64_t pcOffset,
                           const char *cubin, size_t cubinSize) {
  CUpti_GetSassToSourceCorrelationParams sassToSourceParams = {
      CUpti_GetSassToSourceCorrelationParamsSize,
      cubin,
      functionName,
      cubinSize,
      0,
      pcOffset,
      NULL,
      NULL,
  };
  // Get source can fail if the line mapping is not available in the cubin so we
  // don't check the return value
  cupti::getSassToSourceCorrelation<false>(&sassToSourceParams);
  auto fileNameStr = sassToSourceParams.fileName
                         ? std::string(sassToSourceParams.fileName)
                         : "";
  auto dirNameStr =
      sassToSourceParams.dirName ? std::string(sassToSourceParams.dirName) : "";
  // It's user's responsibility to free the memory
  if (sassToSourceParams.fileName)
    std::free(sassToSourceParams.fileName);
  if (sassToSourceParams.dirName)
    std::free(sassToSourceParams.dirName);
  return std::make_tuple(sassToSourceParams.lineNumber, fileNameStr,
                         dirNameStr);
}

std::pair<char **, uint32_t *>
getStallReasonNamesAndIndices(CUcontext context, size_t numStallReasons) {
  char **stallReasonNames =
      static_cast<char **>(std::calloc(numStallReasons, sizeof(char *)));
  for (size_t i = 0; i < numStallReasons; i++) {
    stallReasonNames[i] = static_cast<char *>(
        std::calloc(CUPTI_STALL_REASON_STRING_SIZE, sizeof(char)));
  }
  uint32_t *stallReasonIndices =
      static_cast<uint32_t *>(std::calloc(numStallReasons, sizeof(uint32_t)));
  // Initialize the names with 128 characters to avoid buffer overflow
  CUpti_PCSamplingGetStallReasonsParams stallReasonsParams = {
      CUpti_PCSamplingGetStallReasonsParamsSize,
      NULL,
      context,
      numStallReasons,
      stallReasonIndices,
      stallReasonNames,
  };
  cupti::pcSamplingGetStallReasons<true>(&stallReasonsParams);
  return std::make_pair(stallReasonNames, stallReasonIndices);
}

size_t matchStallReasonsToIndices(
    size_t numStallReasons, char **stallReasonNames,
    uint32_t *stallReasonIndices,
    std::map<size_t, size_t> &stallReasonIndexToMetricIndex,
    std::set<size_t> &notIssuedStallReasonIndices) {
  // In case there's any invalid stall reasons, we only collect valid ones.
  // Invalid ones are swapped to the end of the list
  std::vector<bool> validIndex(numStallReasons, false);
  size_t numValidStalls = 0;
  for (size_t i = 0; i < numStallReasons; i++) {
    bool notIssued = std::string(stallReasonNames[i]).find("not_issued") !=
                     std::string::npos;
    std::string cuptiStallName = std::string(stallReasonNames[i]);
    for (size_t j = 0; j < PCSamplingMetric::PCSamplingMetricKind::Count; j++) {
      auto metricName = PCSamplingMetric().getValueName(j);
      if (cuptiStallName.find(metricName) != std::string::npos) {
        if (notIssued)
          notIssuedStallReasonIndices.insert(stallReasonIndices[i]);
        stallReasonIndexToMetricIndex[stallReasonIndices[i]] = j;
        validIndex[i] = true;
        numValidStalls++;
        break;
      }
    }
  }
  int invalidIndex = -1;
  for (size_t i = 0; i < numStallReasons; i++) {
    if (invalidIndex == -1 && !validIndex[i]) {
      invalidIndex = i;
    } else if (invalidIndex != -1 && validIndex[i]) {
      std::swap(stallReasonIndices[invalidIndex], stallReasonIndices[i]);
      std::swap(stallReasonNames[invalidIndex], stallReasonNames[i]);
      validIndex[invalidIndex] = true;
      invalidIndex++;
    }
  }
  return numValidStalls;
}

#define CUPTI_CUDA12_4_VERSION 22
#define CUPTI_CUDA12_4_PC_DATA_PADDING_SIZE sizeof(uint32_t)

CUpti_PCSamplingData allocPCSamplingData(size_t collectNumPCs,
                                         size_t numValidStallReasons) {
  uint32_t libVersion = 0;
  cupti::getVersion<true>(&libVersion);
  size_t pcDataSize = sizeof(CUpti_PCSamplingPCData);
  // Check cupti api version < 12.4 but cupti header version >= 12.4
  // If so, we subtract 4 bytes from the size of CUpti_PCSamplingPCData
  // because it introduces a new field (i.e., correlationId) at the end of the
  // struct, which is not compatible with the previous versions.
  if (libVersion < CUPTI_CUDA12_4_VERSION &&
      CUPTI_API_VERSION >= CUPTI_CUDA12_4_VERSION)
    pcDataSize -= CUPTI_CUDA12_4_PC_DATA_PADDING_SIZE;
  CUpti_PCSamplingData pcSamplingData{
      pcDataSize,
      collectNumPCs,
  };
  pcSamplingData.pPcData = static_cast<CUpti_PCSamplingPCData *>(
          std::calloc(collectNumPCs, sizeof(CUpti_PCSamplingPCData)));
  for (size_t i = 0; i < collectNumPCs; ++i) {
    pcSamplingData.pPcData[i].stallReason =
        static_cast<CUpti_PCSamplingStallReason *>(std::calloc(
            numValidStallReasons, sizeof(CUpti_PCSamplingStallReason)));
  }
  return pcSamplingData;
}

void enablePCSampling(CUcontext context) {
  CUpti_PCSamplingEnableParams params = {
      CUpti_PCSamplingEnableParamsSize,
      NULL,
      context,
  };
  cupti::pcSamplingEnable<true>(&params);
}

void disablePCSampling(CUcontext context) {
  CUpti_PCSamplingDisableParams params = {
      CUpti_PCSamplingDisableParamsSize,
      NULL,
      context,
  };
  cupti::pcSamplingDisable<true>(&params);
}

void startPCSampling(CUcontext context) {
  CUpti_PCSamplingStartParams params = {
      CUpti_PCSamplingStartParamsSize,
      NULL,
      context,
  };
  cupti::pcSamplingStart<true>(&params);
}

void stopPCSampling(CUcontext context) {
  CUpti_PCSamplingStopParams params = {
      CUpti_PCSamplingStopParamsSize,
      NULL,
      context,
  };
  cupti::pcSamplingStop<true>(&params);
}

void getPCSamplingData(CUcontext context,
                       CUpti_PCSamplingData *pcSamplingData) {
  CUpti_PCSamplingGetDataParams params = {
      CUpti_PCSamplingGetDataParamsSize,
      NULL,
      context,
      pcSamplingData,
  };
  cupti::pcSamplingGetData<true>(&params);
}

void setConfigurationAttribute(
    CUcontext context,
    std::vector<CUpti_PCSamplingConfigurationInfo> &configurationInfos) {
  CUpti_PCSamplingConfigurationInfoParams infoParams = {
      CUpti_PCSamplingConfigurationInfoParamsSize,
      NULL,
      context,
      configurationInfos.size(),
      configurationInfos.data(),
  };
  cupti::pcSamplingSetConfigurationAttribute<true>(&infoParams);
}

} // namespace

CUpti_PCSamplingConfigurationInfo ConfigureData::configureStallReasons() {
  numStallReasons = getNumStallReasons(context);
  std::tie(this->stallReasonNames, this->stallReasonIndices) =
      getStallReasonNamesAndIndices(context, numStallReasons);
  numValidStallReasons = matchStallReasonsToIndices(
      numStallReasons, stallReasonNames, stallReasonIndices,
      stallReasonIndexToMetricIndex, notIssuedStallReasonIndices);
  CUpti_PCSamplingConfigurationInfo stallReasonInfo{};
  stallReasonInfo.attributeType =
      CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_STALL_REASON;
  stallReasonInfo.attributeData.stallReasonData.stallReasonCount =
      numValidStallReasons;
  stallReasonInfo.attributeData.stallReasonData.pStallReasonIndex =
      stallReasonIndices;
  return stallReasonInfo;
}

CUpti_PCSamplingConfigurationInfo ConfigureData::configureSamplingPeriod() {
  CUpti_PCSamplingConfigurationInfo samplingPeriodInfo{};
  samplingPeriodInfo.attributeType =
      CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_SAMPLING_PERIOD;
  samplingPeriodInfo.attributeData.samplingPeriodData.samplingPeriod =
      DefaultFrequency;
  return samplingPeriodInfo;
}

CUpti_PCSamplingConfigurationInfo ConfigureData::configureSamplingBuffer() {
  CUpti_PCSamplingConfigurationInfo samplingBufferInfo{};
  samplingBufferInfo.attributeType =
      CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_SAMPLING_DATA_BUFFER;
  this->pcSamplingData =
      allocPCSamplingData(DataBufferPCCount, numValidStallReasons);
  samplingBufferInfo.attributeData.samplingDataBufferData.samplingDataBuffer =
      &this->pcSamplingData;
  return samplingBufferInfo;
}

CUpti_PCSamplingConfigurationInfo ConfigureData::configureScratchBuffer() {
  CUpti_PCSamplingConfigurationInfo scratchBufferInfo{};
  scratchBufferInfo.attributeType =
      CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_SCRATCH_BUFFER_SIZE;
  scratchBufferInfo.attributeData.scratchBufferSizeData.scratchBufferSize =
      ScratchBufferSize;
  return scratchBufferInfo;
}

CUpti_PCSamplingConfigurationInfo ConfigureData::configureHardwareBufferSize() {
  CUpti_PCSamplingConfigurationInfo hardwareBufferInfo{};
  hardwareBufferInfo.attributeType =
      CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_HARDWARE_BUFFER_SIZE;
  hardwareBufferInfo.attributeData.hardwareBufferSizeData.hardwareBufferSize =
      HardwareBufferSize;
  return hardwareBufferInfo;
}

CUpti_PCSamplingConfigurationInfo ConfigureData::configureStartStopControl() {
  CUpti_PCSamplingConfigurationInfo startStopControlInfo{};
  startStopControlInfo.attributeType =
      CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_ENABLE_START_STOP_CONTROL;
  startStopControlInfo.attributeData.enableStartStopControlData
      .enableStartStopControl = true;
  return startStopControlInfo;
}

CUpti_PCSamplingConfigurationInfo ConfigureData::configureCollectionMode() {
  CUpti_PCSamplingConfigurationInfo collectionModeInfo{};
  collectionModeInfo.attributeType =
      CUPTI_PC_SAMPLING_CONFIGURATION_ATTR_TYPE_COLLECTION_MODE;
  collectionModeInfo.attributeData.collectionModeData.collectionMode =
      CUPTI_PC_SAMPLING_COLLECTION_MODE_CONTINUOUS;
  return collectionModeInfo;
}

void ConfigureData::initialize(CUcontext context) {
  this->context = context;
  cupti::getContextId<true>(context, &contextId);
  configurationInfos.emplace_back(configureStallReasons());
  configurationInfos.emplace_back(configureSamplingPeriod());
  configurationInfos.emplace_back(configureHardwareBufferSize());
  configurationInfos.emplace_back(configureScratchBuffer());
  configurationInfos.emplace_back(configureSamplingBuffer());
  configurationInfos.emplace_back(configureStartStopControl());
  configurationInfos.emplace_back(configureCollectionMode());
  setConfigurationAttribute(context, configurationInfos);
}

ConfigureData *CuptiPCSampling::getConfigureData(uint32_t contextId) {
  return &contextIdToConfigureData[contextId];
}

CubinData *CuptiPCSampling::getCubinData(uint64_t cubinCrc) {
  return &(cubinCrcToCubinData[cubinCrc].first);
}

void CuptiPCSampling::initialize(CUcontext context) {
  uint32_t contextId = 0;
  cupti::getContextId<true>(context, &contextId);
  doubleCheckedLock([&]() { return !contextInitialized.contain(contextId); },
                    contextMutex,
                    [&]() {
                      enablePCSampling(context);
                      getConfigureData(contextId)->initialize(context);
                      contextInitialized.insert(contextId);
                    });
}

void CuptiPCSampling::start(CUcontext context) {
  uint32_t contextId = 0;
  cupti::getContextId<true>(context, &contextId);
  doubleCheckedLock([&]() -> bool { return !pcSamplingStarted; },
                    pcSamplingMutex,
                    [&]() {
                      initialize(context);
                      // Ensure all previous operations are completed
                      cuda::ctxSynchronize<true>();
                      startPCSampling(context);
                      pcSamplingStarted = true;
                    });
}

void CuptiPCSampling::processPCSamplingData(ConfigureData *configureData,
                                            uint64_t externId, bool isAPI) {
  auto *pcSamplingData = &configureData->pcSamplingData;
  auto &profiler = CuptiProfiler::instance();
  auto dataSet = profiler.getDataSet();
  // In the first round, we need to call getPCSamplingData to get the unsynced
  // data from the hardware buffer
  bool firstRound = true;
  while (pcSamplingData->totalNumPcs > 0 ||
         pcSamplingData->remainingNumPcs > 0 || firstRound) {
    // Handle data
    for (size_t i = 0; i < pcSamplingData->totalNumPcs; ++i) {
      auto *pcData = pcSamplingData->pPcData + i;
      auto *cubinData = getCubinData(pcData->cubinCrc);
      auto key =
          CubinData::LineInfoKey{pcData->functionIndex, pcData->pcOffset};
      if (cubinData->lineInfo.find(key) == cubinData->lineInfo.end()) {
        auto [lineNumber, fileName, dirName] =
            getSassToSourceCorrelation(pcData->functionName, pcData->pcOffset,
                                       cubinData->cubin, cubinData->cubinSize);
        cubinData->lineInfo.try_emplace(key, lineNumber,
                                        std::string(pcData->functionName),
                                        dirName, fileName);
      }
      auto &lineInfo = cubinData->lineInfo[key];
      for (size_t j = 0; j < pcData->stallReasonCount; ++j) {
        auto *stallReason = &pcData->stallReason[j];
        if (!configureData->stallReasonIndexToMetricIndex.count(
                stallReason->pcSamplingStallReasonIndex))
          throw std::runtime_error("Invalid stall reason index");
        for (auto *data : dataSet) {
          auto scopeId = externId;
          if (isAPI)
            scopeId = data->addScope(externId, lineInfo.functionName);
          if (lineInfo.fileName.size())
            scopeId = data->addScope(
                scopeId, lineInfo.dirName + "/" + lineInfo.fileName + ":" +
                             lineInfo.functionName + "@" +
                             std::to_string(lineInfo.lineNumber));
          auto metricKind = static_cast<PCSamplingMetric::PCSamplingMetricKind>(
              configureData->stallReasonIndexToMetricIndex
                  [stallReason->pcSamplingStallReasonIndex]);
          auto samples = stallReason->samples;
          auto stalledSamples =
              configureData->notIssuedStallReasonIndices.count(
                  stallReason->pcSamplingStallReasonIndex)
                  ? 0
                  : samples;
          auto metric = std::make_shared<PCSamplingMetric>(metricKind, samples,
                                                           stalledSamples);
          data->addMetric(scopeId, metric);
        }
      }
    }
    if (pcSamplingData->remainingNumPcs > 0 || firstRound) {
      getPCSamplingData(configureData->context, pcSamplingData);
      firstRound = false;
    } else
      break;
  }
}

void CuptiPCSampling::stop(CUcontext context, uint64_t externId, bool isAPI) {
  uint32_t contextId = 0;
  cupti::getContextId<true>(context, &contextId);
  doubleCheckedLock([&]() -> bool { return pcSamplingStarted; },
                    pcSamplingMutex,
                    [&]() {
                      auto *configureData = getConfigureData(contextId);
                      stopPCSampling(context);
                      pcSamplingStarted = false;
                      processPCSamplingData(configureData, externId, isAPI);
                    });
}

void CuptiPCSampling::finalize(CUcontext context) {
  uint32_t contextId = 0;
  cupti::getContextId<true>(context, &contextId);
  if (!contextInitialized.contain(contextId))
    return;
  auto *configureData = getConfigureData(contextId);
  contextIdToConfigureData.erase(contextId);
  contextInitialized.erase(contextId);
  disablePCSampling(context);
}

void CuptiPCSampling::loadModule(const char *cubin, size_t cubinSize) {
  auto cubinCrc = getCubinCrc(cubin, cubinSize);
  auto *cubinData = getCubinData(cubinCrc);
  cubinData->cubinCrc = cubinCrc;
  cubinData->cubinSize = cubinSize;
  cubinData->cubin = cubin;
}

void CuptiPCSampling::unloadModule(const char *cubin, size_t cubinSize) {
  // XXX: Unload module is supposed to be called in a thread safe manner
  // i.e., no two threads will be calling unload module the same time
  auto cubinCrc = getCubinCrc(cubin, cubinSize);
  auto count = cubinCrcToCubinData[cubinCrc].second;
  if (count > 1)
    cubinCrcToCubinData[cubinCrc].second = count - 1;
  else
    cubinCrcToCubinData.erase(cubinCrc);
}

} // namespace proton
