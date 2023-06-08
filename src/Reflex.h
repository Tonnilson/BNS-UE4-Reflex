#pragma once

#include <d3d11.h>
#include <dxgi.h>
#pragma warning(push)
#pragma warning(disable: 4828)
#include <NVAPI/nvapi.h>
#pragma warning(pop)

extern ID3D11DeviceContext* g_DeviceContext;
extern ID3D11Device* g_Device;
extern IDXGISwapChain* g_SwapChain;

class Reflex
{
public:
	static Reflex* Instance()
	{
		static Reflex handler;
		return &handler;
	}

	// Shared

	void Initialize();

	// NVAPI

	void NVAPI_SetSleepMode();
	bool SetLatencyMarker(NV_LATENCY_MARKER_TYPE marker);
	void SetUseMarkersToOptimize(const void* value);
	void SetLowLatencyBoost(const void* value);
	void SetLowLatencyMode(const void* value);
	void SetUseFPSLimit(const void* value);
	bool GetLatencyReport();

	enum class GPUType
	{
		NVIDIA,
		AMD,
		INTEL,
		UNKNOWN
	};

	NvAPI_Status lastStatus = NVAPI_INVALID_CONFIGURATION;

	bool  bReflexEnabled = false;

	bool  bLowLatencyMode = true;
	bool  bLowLatencyBoost = false;
	bool  bUseMarkersToOptimize = false;
	bool  bUseFPSLimit = false;
	float fFPSLimit = 60;
	bool bReadyToUse = false;
	bool bInitalized = false;

	float AverageTotalLatencyMs = 0.0f;
	float AverageGameLatencyMs = 0.0f;
	float AverageRenderLatencyMs = 0.0f;

	float AverageSimulationLatencyMs = 0.0f;
	float AverageRenderSubmitLatencyMs = 0.0f;
	float AveragePresentLatencyMs = 0.0f;
	float AverageDriverLatencyMs = 0.0f;
	float AverageOSRenderQueueLatencyMs = 0.0f;
	float AverageGPURenderLatencyMs = 0.0f;

	float RenderSubmitOffsetMs = 0.0f;
	float PresentOffsetMs = 0.0f;
	float DriverOffsetMs = 0.0f;
	float OSRenderQueueOffsetMs = 0.0f;
	float GPURenderOffsetMs = 0.0f;
	NvU64 FrameId = 0;

protected:


private:
	Reflex() { };

	Reflex(const Reflex&) = delete;
	Reflex(Reflex&&) = delete;

	~Reflex() = default;

	Reflex& operator=(const Reflex&) = delete;
	Reflex& operator=(Reflex&&) = delete;

	DWORD64 frames_drawn = 0;

	// Shared

	GPUType gpuType = GPUType::UNKNOWN;

	// NVAPI

	bool InitializeNVAPI();
};