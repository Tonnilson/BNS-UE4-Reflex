#include "header.h"

// Credits to doodlum on Github, used his source for Skyrim's first implementation as a starting point and modified to my needs
// https://github.com/doodlum/skyrim-nvidia-reflex

void Reflex::Initialize()
{
	if (InitializeNVAPI()) {
		ConsoleWrite(L"NVAPI initialized\n");
		NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS] = { 0 };
		NvU32 gpuCount = 0;

		// Get all the Physical GPU Handles
		NvAPI_EnumPhysicalGPUs(nvGPUHandle, &gpuCount);
		if (gpuCount > 0) {
			NvAPI_ShortString gpuName;
			NvAPI_GPU_GetFullName(nvGPUHandle[0], gpuName);
			ConsoleWrite(L"Found %S GPU\n", gpuName);

			// Driver version check
			NvU32 DriverVersion;
			NvAPI_ShortString BranchString;
			NvAPI_SYS_GetDriverAndBranchVersion(&DriverVersion, BranchString);
			if (DriverVersion < 45500) {
				ConsoleWrite(L"Current driver version does not support Reflex, update your drivers.\n");
				return;
			}

			bInitalized = true;
			gpuType = GPUType::NVIDIA;
		}
		else {
			ConsoleWrite(L"Did not find an NVIDIA GPU\n");
		}
	}
	else {
		ConsoleWrite(L"Could not initialize NVAPI\n");
	}
}

bool Reflex::InitializeNVAPI()
{
	NvAPI_Status ret = NVAPI_OK;

	ret = NvAPI_Initialize();

	return ret == NVAPI_OK;
};

void Reflex::NVAPI_SetSleepMode()
{
	if (gpuType == GPUType::NVIDIA) {
		NvAPI_Status                status = NVAPI_OK;

		// First make sure it's supported by the hardware?
		NV_GET_SLEEP_STATUS_PARAMS_V1 SleepParams = { 0 };
		SleepParams.version = NV_GET_SLEEP_STATUS_PARAMS_VER1;
		status = NvAPI_D3D_GetSleepStatus(g_Device, &SleepParams);

		if (status != NVAPI_OK) return;

		NV_SET_SLEEP_MODE_PARAMS_V1 params = { 0 };
		params.version = NV_SET_SLEEP_MODE_PARAMS_VER1;
		params.bLowLatencyMode = bLowLatencyMode;
		params.bLowLatencyBoost = bLowLatencyBoost;
		params.minimumIntervalUs = bUseFPSLimit ? (NvU32)((1000.0f / fFPSLimit) * 1000.0f) : 0;
		params.bUseMarkersToOptimize = bUseMarkersToOptimize;

		status = NvAPI_D3D_SetSleepMode(g_Device, &params);

		bReflexEnabled = (status == NVAPI_OK);
		if (status != lastStatus) {
			if (!bReadyToUse)
				bReadyToUse = true;

			lastStatus = status;
		}
	}
}

bool Reflex::SetLatencyMarker(NV_LATENCY_MARKER_TYPE marker)
{
	NvAPI_Status ret = NVAPI_INVALID_CONFIGURATION;

	if (bReflexEnabled) {
		NV_LATENCY_MARKER_PARAMS
			markerParams = {};
		markerParams.version = NV_LATENCY_MARKER_PARAMS_VER;
		markerParams.markerType = marker;
		markerParams.frameID = static_cast<NvU64>(ReadULong64Acquire(&frames_drawn));
		ret = NvAPI_D3D_SetLatencyMarker(g_Device, &markerParams);
	}

	if (marker == PRESENT_END)
		frames_drawn++;

	return (ret == NVAPI_OK);
}

void Reflex::SetLowLatencyMode(const void* value)
{
	Reflex::Instance()->bLowLatencyMode = *static_cast<const bool*>(value);
	Reflex::Instance()->NVAPI_SetSleepMode();
}

void Reflex::SetLowLatencyBoost(const void* value)
{
	Reflex::Instance()->bLowLatencyBoost = *static_cast<const bool*>(value);
	Reflex::Instance()->NVAPI_SetSleepMode();
}

void Reflex::SetUseMarkersToOptimize(const void* value)
{
	Reflex::Instance()->bUseMarkersToOptimize = *static_cast<const bool*>(value);
	Reflex::Instance()->NVAPI_SetSleepMode();
}

void Reflex::SetUseFPSLimit(const void* value)
{
	Reflex::Instance()->bUseFPSLimit = *static_cast<const bool*>(value);
	Reflex::Instance()->NVAPI_SetSleepMode();
}

void SetTargetFPS(const void* value, [[maybe_unused]] void* clientData)
{
	Reflex::Instance()->fFPSLimit = *static_cast<const float*>(value);
	Reflex::Instance()->NVAPI_SetSleepMode();
}

bool Reflex::GetLatencyReport()
{
	if (!Reflex::Instance()->bReflexEnabled)
		return false;

	NV_LATENCY_RESULT_PARAMS LatencyResults = {};
	LatencyResults.version = NV_LATENCY_RESULT_PARAMS_VER;

	NvAPI_Status ret = NvAPI_D3D_GetLatency(g_Device, &LatencyResults);

	FrameId = LatencyResults.frameReport[63].frameID;
	const NvU64 TotalLatencyUs = LatencyResults.frameReport[63].gpuRenderEndTime - LatencyResults.frameReport[63].simStartTime;
	if (TotalLatencyUs != 0)
	{
		AverageTotalLatencyMs = AverageTotalLatencyMs * 0.75f + TotalLatencyUs / 1000.0f * 0.25f;

		AverageGameLatencyMs = AverageGameLatencyMs * 0.75f + (LatencyResults.frameReport[63].driverEndTime - LatencyResults.frameReport[63].simStartTime) / 1000.0f * 0.25f;
		AverageRenderLatencyMs = AverageRenderLatencyMs * 0.75f + (LatencyResults.frameReport[63].gpuRenderEndTime - LatencyResults.frameReport[63].osRenderQueueStartTime) / 1000.0f * 0.25f;

		AverageSimulationLatencyMs = AverageSimulationLatencyMs * 0.75f + (LatencyResults.frameReport[63].simEndTime - LatencyResults.frameReport[63].simStartTime) / 1000.0f * 0.25f;
		AverageRenderSubmitLatencyMs = AverageRenderSubmitLatencyMs * 0.75f + (LatencyResults.frameReport[63].renderSubmitEndTime - LatencyResults.frameReport[63].renderSubmitStartTime) / 1000.0f * 0.25f;
		AveragePresentLatencyMs = AveragePresentLatencyMs * 0.75f + (LatencyResults.frameReport[63].presentEndTime - LatencyResults.frameReport[63].presentStartTime) / 1000.0f * 0.25f;
		AverageDriverLatencyMs = AverageDriverLatencyMs * 0.75f + (LatencyResults.frameReport[63].driverEndTime - LatencyResults.frameReport[63].driverStartTime) / 1000.0f * 0.25f;
		AverageOSRenderQueueLatencyMs = AverageOSRenderQueueLatencyMs * 0.75f + (LatencyResults.frameReport[63].osRenderQueueEndTime - LatencyResults.frameReport[63].osRenderQueueStartTime) / 1000.0f * 0.25f;
		AverageGPURenderLatencyMs = AverageGPURenderLatencyMs * 0.75f + (LatencyResults.frameReport[63].gpuRenderEndTime - LatencyResults.frameReport[63].gpuRenderStartTime) / 1000.0f * 0.25f;

		RenderSubmitOffsetMs = (LatencyResults.frameReport[63].renderSubmitStartTime - LatencyResults.frameReport[63].simStartTime) / 1000.0f;
		PresentOffsetMs = (LatencyResults.frameReport[63].presentStartTime - LatencyResults.frameReport[63].simStartTime) / 1000.0f;
		DriverOffsetMs = (LatencyResults.frameReport[63].driverStartTime - LatencyResults.frameReport[63].simStartTime) / 1000.0f;
		OSRenderQueueOffsetMs = (LatencyResults.frameReport[63].osRenderQueueStartTime - LatencyResults.frameReport[63].simStartTime) / 1000.0f;
		GPURenderOffsetMs = (LatencyResults.frameReport[63].gpuRenderStartTime - LatencyResults.frameReport[63].simStartTime) / 1000.0f;
	}

	return (ret == NVAPI_OK);
}

