// dllmain.cpp : Defines the entry point for the DLL application.
#include "header.h"

#define LDR_DLL_NOTIFICATION_REASON_LOADED 1
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 0
typedef NTSTATUS(NTAPI* tLdrRegisterDllNotification)(ULONG, PVOID, PVOID, PVOID);

// These are not used but being left as a reference if you opted for this method
extern "C" {
	__declspec() void PresentInject_1_ASM();
	__declspec() void PresentInject_2_ASM();
	uintptr_t o_presentinject_1;
	uintptr_t o_presentinject_2;
}

EXTERN_C void PresentLatencyMarkerStart() {
	Reflex::Instance()->SetLatencyMarker(PRESENT_START);
}

EXTERN_C void PresentLatencyMarkerEnd() {
	Reflex::Instance()->SetLatencyMarker(PRESENT_END);
}

void(__fastcall* PresentInject_1)();
void(__fastcall* PresentInject_2)();

bool bImGui_init = false;
bool bImGui_ShowMenu = true;

void ConsoleWrite(const wchar_t* msg, ...) {
	wchar_t szBuffer[1024];
	va_list args;
	va_start(args, msg);
	vswprintf(szBuffer, 1024, msg, args);
	WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), szBuffer, wcslen(szBuffer), NULL, NULL);
	va_end(args);
}

uintptr_t GetAddress(uintptr_t AddressOfCall, int index, int length)
{
	if (!AddressOfCall)
		return 0;

	long delta = *(long*)(AddressOfCall + index);
	return (AddressOfCall + delta + length);
}

__int64(__fastcall* oAppWndProc)(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
__int64 __fastcall hkAppWndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg == WM_KEYDOWN && Reflex::Instance()->bReflexEnabled)
		Reflex::Instance()->SetLatencyMarker(PC_LATENCY_PING);

	if (bImGui_init && bImGui_ShowMenu)
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

	return oAppWndProc(hWnd, uMsg, wParam, lParam);
}

// Not currently used but I'll still hook it
__int64(__fastcall* oFViewport_BeginRenderFrame)(__int64, __int64);
__int64 __fastcall hkFViewport_BeginRenderFrame(__int64 This, __int64 RHICmdList)
{
	if (Reflex::Instance()->bReflexEnabled) {
		Reflex::Instance()->SetLatencyMarker(RENDERSUBMIT_START);
	}
	return oFViewport_BeginRenderFrame(This, RHICmdList);
}

void* (__fastcall* FRHICommandList_BeginFrame)(__int64);
void* __fastcall hkFRHICommandList_BeginFrame(__int64 This) {
	if (Reflex::Instance()->bReflexEnabled) {
		Reflex::Instance()->SetLatencyMarker(RENDERSUBMIT_START);
	}
	return FRHICommandList_BeginFrame(This);
}

// Not currently used but I'll still hook it
__int64(__fastcall* oFViewport_EndRenderFrame)(__int64, __int64, bool, bool);
__int64 __fastcall hkFViewport_EndRenderFrame(__int64 This, __int64 RHICmdList, bool bPresent, bool bLockToVsync)
{
	if (Reflex::Instance()->bReflexEnabled) {
		Reflex::Instance()->SetLatencyMarker(RENDERSUBMIT_END);

	}
	return oFViewport_EndRenderFrame(This, RHICmdList, bPresent, bLockToVsync);
}

__int64(__fastcall* FRHICommandList_EndFrame)(__int64);
__int64 __fastcall hkFRHICommandList_EndFrame(__int64 This) {
	if (Reflex::Instance()->bReflexEnabled) {
		Reflex::Instance()->SetLatencyMarker(RENDERSUBMIT_END);
	}
	return FRHICommandList_EndFrame(This);
}

unsigned __int64(__fastcall* oFEngineLoop_Tick)(__int64);
unsigned __int64 __fastcall hkFEngineLoop_Tick(__int64 This) {
	if (Reflex::Instance()->bReflexEnabled) {
		Reflex::Instance()->SetLatencyMarker(SIMULATION_START);
		Reflex::Instance()->SetLatencyMarker(INPUT_SAMPLE);
	}

	auto ret = oFEngineLoop_Tick(This);
	if (Reflex::Instance()->bReflexEnabled)
		Reflex::Instance()->SetLatencyMarker(SIMULATION_END);

	return ret;
}

ID3D11Device* g_Device;
ID3D11DeviceContext* g_DeviceContext;
IDXGISwapChain* g_SwapChain;

decltype(&IDXGISwapChain::Present)         ptrPresent;
decltype(&D3D11CreateDeviceAndSwapChain)   ptrD3D11CreateDeviceAndSwapChain;

HWND window = NULL;
ID3D11RenderTargetView* mainRenderTargetView = NULL;
std::vector<DeviceList*> currentDevices;

HRESULT WINAPI hkIDXGISwapChain_Present(IDXGISwapChain* This, UINT SyncInterval, UINT Flags)
{
	if (!bImGui_init) {
		if (g_Device && g_DeviceContext) {
			DXGI_SWAP_CHAIN_DESC sd;
			This->GetDesc(&sd);

			window = sd.OutputWindow;

			ID3D11Texture2D* pBackBuffer;
			This->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
			g_Device->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);

			pBackBuffer->Release();

			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			ImGui_ImplWin32_Init(window);
			ImGui_ImplDX11_Init(g_Device, g_DeviceContext);
			bImGui_init = true;
		}
		else
			goto EndOfPresent;
	}

	/*
		IMGUI STUFF
	*/
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	if (Reflex::Instance()->bReadyToUse) 
	{
		if (bImGui_ShowMenu) 
		{
			Reflex::Instance()->GetLatencyReport();

			ImGui::Begin("Reflex", &bImGui_ShowMenu);
			ImGui::SetWindowSize(ImVec2(220, 235), ImGuiCond_Always);
			ImGui::Checkbox("Enable Reflex", &Reflex::Instance()->bReflexEnabled);

			if (ImGui::Checkbox("Limit FPS", &Reflex::Instance()->bUseFPSLimit)) {
				Reflex::Instance()->SetUseFPSLimit(&Reflex::Instance()->bUseFPSLimit);
			}

			if (ImGui::Checkbox("Low Latency Mode", &Reflex::Instance()->bLowLatencyMode)) {
				Reflex::Instance()->SetLowLatencyMode(&Reflex::Instance()->bLowLatencyMode);
			}

			if (ImGui::Checkbox("Boost", &Reflex::Instance()->bLowLatencyBoost)) {
				Reflex::Instance()->SetLowLatencyBoost(&Reflex::Instance()->bLowLatencyBoost);
			}

			ImGui::Separator();
			ImGui::Text("FrameId: %d", Reflex::Instance()->FrameId);
			ImGui::Text("Total Latency: %.2f ms", Reflex::Instance()->AverageTotalLatencyMs);
			ImGui::Text("Game Latency: %.2f ms", Reflex::Instance()->AverageGameLatencyMs);
			ImGui::Text("Render Latency: %.2f ms", Reflex::Instance()->AverageRenderLatencyMs);
			ImGui::Text("GPU Render Latency: %.2f ms", Reflex::Instance()->AverageGPURenderLatencyMs);
			ImGui::Text("Present Latency: %.2f ms", Reflex::Instance()->AveragePresentLatencyMs);
			ImGui::End();
		}
	}

	ImGui::EndFrame();
	ImGui::Render();
	g_DeviceContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

EndOfPresent:
	Reflex::Instance()->SetLatencyMarker(PRESENT_START);
	auto hr = (This->*ptrPresent)(SyncInterval, Flags);
	Reflex::Instance()->SetLatencyMarker(PRESENT_END);
	if (Reflex::Instance()->bReflexEnabled && g_Device) {
		NvAPI_D3D_Sleep(g_Device);
	}

	return hr;
}

// There is a easier way and that's just to hook the swapchain::present and get the device and devicecontext from there
// but I'm sticking to this for w/e reason
HRESULT WINAPI hk_D3D11CreateDeviceAndSwapChain(
	IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE             DriverType,
	HMODULE                     Software,
	UINT                        Flags,
	const D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT                        FeatureLevels,
	UINT                        SDKVersion,
	const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
	IDXGISwapChain** ppSwapChain,
	ID3D11Device** ppDevice,
	D3D_FEATURE_LEVEL* pFeatureLevel,
	ID3D11DeviceContext** ppImmediateContext)
{
	// Call original to create the device and swap chain
	auto hr = (*ptrD3D11CreateDeviceAndSwapChain)(pAdapter,
		DriverType,
		Software,
		Flags,
		pFeatureLevels,
		FeatureLevels,
		SDKVersion,
		pSwapChainDesc,
		ppSwapChain,
		ppDevice,
		pFeatureLevel,
		ppImmediateContext);
	
	// We store this in a vector because there are multiple devices created, the amount changes based on various factors
	// But we typically want the most recently created one
	DeviceList* device = new DeviceList(*ppDevice, *ppImmediateContext);
	currentDevices.push_back(device);

	ConsoleWrite(L"Device: %p | Context: %p | SwapChain: %p\n", *ppDevice, *ppImmediateContext, ppSwapChain);

	return hr;
}

// Callback for when a DLL is loaded or unloaded, we use this as a execution point when something like the swapchain is available
// I don't know about other UE4 games because BnS is very unique compared to most as it has an entirely separate game layer on top of UE4 systems
// So the swapchain is not available when CreateDeviceAndSwapChain is called so I have to dig later on for the vtable
// It's probably available at the start but I can't remember.. Anyways also setting our pointer for Device & DeviceContext
void NTAPI DllNotification(ULONG notification_reason, const LDR_DLL_NOTIFICATION_DATA* notification_data, PVOID context)
{
	if (notification_reason == LDR_DLL_NOTIFICATION_REASON_LOADED) {
		//ConsoleWrite(L"DLL Loaded: %s\n", notification_data->Loaded.BaseDllName->Buffer);
		if (wcsncmp(notification_data->Loaded.BaseDllName->Buffer, xorstr_(L"wlanapi"), 7) == 0) {
			long response;
			DetourTransactionBegin();
			DetourUpdateThread(NtCurrentThread());

			// We want the last created device as this will be the main device for the game (hopefully)
			// This just depends on the type of software running on the users system, sometimes there's 3 devices, sometimes there's 4-5 with NVIDIA Overlay
			// You can also just like... you know get the device and deviceContext from the swapchain
			if (!currentDevices.empty()) {
				g_Device = currentDevices.back()->Device();
				g_DeviceContext = currentDevices.back()->DeviceContext();
			}

			// tl;dr I have to find the swapchain vtable this way instead of the dummy device method because for a reason idfc to figure out,I can't create a d3d device in this plugin specifically, can in others no problem.
			if (!g_SwapChain) {
				if (const auto module = pe::get_module(xorstr_(L"dxgi.dll"))) {
					uintptr_t handle = module->handle();
					const auto sections = module->segments();
					const auto& s1 = std::find_if(sections.begin(), sections.end(), [](const IMAGE_SECTION_HEADER& x) {
						return x.Characteristics & IMAGE_SCN_CNT_CODE;
						});
					const auto data = s1->as_bytes();

					// Credits to mambda on GH for this little method
					ConsoleWrite(L"[reflex] Searching for swapchain\n");
					auto result = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("90 48 8D 05 ?? ?? ?? ?? 48 89 07")));
					if (result != data.end()) {
						void** swapchainVTable = (void**)&result[0];
						char* curAddr = (char*)swapchainVTable + 1;
						long intermediate = *(long*)(curAddr + 3);
						swapchainVTable = (void**)(curAddr + intermediate + 7);

						ptrPresent = *reinterpret_cast<decltype(ptrPresent)*>(&swapchainVTable[8]);
						ConsoleWrite(L"[reflex] hooking IDXGISwapChain::Present\n");
						response = DetourAttach(&(PVOID&)ptrPresent, hkIDXGISwapChain_Present);
						if (response != NO_ERROR)
							ConsoleWrite(xorstr_(L"[reflex] Error hooking IDXGISwapChain::Present\n"));
					}
				}
			}

			auto result = DetourTransactionCommit();
			if (result == 0) {
				ConsoleWrite(xorstr_(L"[reflex] TransactionCommit GOOD\n"));
			}
			else
				ConsoleWrite(xorstr_(L"[reflex] TransactionCommit FAILED | 0x%08X\n"), result);

			Reflex::Instance()->NVAPI_SetSleepMode();
		}
	}
	return;
}

bool bPressed = false;
void(__fastcall* oBInputKey)(BInputKey* thisptr, EInputKeyEvent* InputKeyEvent);
void __fastcall hkBInputKey(BInputKey* thisptr, EInputKeyEvent* InputKeyEvent) {
	if (InputKeyEvent->_vKey == VK_HOME) {
		if (InputKeyEvent->KeyState == EngineKeyStateType::EKS_PRESSED && !bPressed) {
			bPressed = true;
			bImGui_ShowMenu = !bImGui_ShowMenu;
		}
		else if (bPressed && InputKeyEvent->KeyState == EngineKeyStateType::EKS_RELEASED)
			bPressed = false;
	}

	return oBInputKey(thisptr, InputKeyEvent);
}

bool __cdecl init([[maybe_unused]] const Version client_version)
{
	AllocConsole();
	ConsoleWrite(L"Initializing NVAPI\n");
	Reflex::Instance()->Initialize();

	DetourTransactionBegin();
	DetourUpdateThread(NtCurrentThread());

	if (const auto module = pe::get_module()) {
		uintptr_t handle = module->handle();
		const auto sections = module->segments();
		const auto& s1 = std::find_if(sections.begin(), sections.end(), [](const IMAGE_SECTION_HEADER& x) {
			return x.Characteristics & IMAGE_SCN_CNT_CODE;
			});
		const auto data = s1->as_bytes();
		auto response = DetourAttach("d3d11.dll", "D3D11CreateDeviceAndSwapChain", ptrD3D11CreateDeviceAndSwapChain, hk_D3D11CreateDeviceAndSwapChain);

		if (response != NO_ERROR)
			ConsoleWrite(L"[reflex] Failed to hook D3D11CreateDeviceAndSwapChain\n");

		// Should be obvious but we hook the main engine loop and set latency markers for SIMULATION_START and SIMULATION_END
		// The "official" implemention is done inside this but we do it "slightly" different by setting the first marker->execute the loop->set second marker
		// This isn't too big of a deal, at worst it should be a difference of 0.20-0.30ms
		// If you want a 1:1 identical implementation you can look at where the official plugin sets the markers inside the engine loop on github and do mid-function hooks
		auto search = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("48 8B C4 55 41 54 41 55 41 56 41 57 48 8D A8 B8 FC FF FF 48 81 EC 20 04 00 00 48 C7 85 30 01 00 00 FE FF FF FF")));
		if (search != data.end()) {
			oFEngineLoop_Tick = module->rva_to<std::remove_pointer_t<decltype(oFEngineLoop_Tick)>>((uintptr_t)&search[0] - handle);
			DetourAttach(&(PVOID&)oFEngineLoop_Tick, &hkFEngineLoop_Tick);
		}

		// This is FWindowsApplication::AppWndProc
		// We hook this to set a latency marker for PC_LATENCY_PING when a key is pressed
		// We're also hooking this for Dear ImGui input handling
		search = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 30 80 3D ?? ?? ?? ?? 00 49 8B D9")));
		if (search != data.end()) {
			oAppWndProc = module->rva_to<std::remove_pointer_t<decltype(oAppWndProc)>>((uintptr_t)&search[0] - handle);
			DetourAttach(&(PVOID&)oAppWndProc, &hkAppWndProc);
		}

		// Same concept as below but different endpoint, this is not used in our version but I left it here
		search = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("40 53 48 83 EC 30 48 C7 44 24 20 FE FF FF FF 48 8B C2 48 8B D9 48 C7 44 24 40 00 00 00 00")));
		if (search != data.end()) {
			oFViewport_BeginRenderFrame = module->rva_to<std::remove_pointer_t<decltype(oFViewport_BeginRenderFrame)>>((uintptr_t)&search[0] - handle);
			DetourAttach(&(PVOID&)oFViewport_BeginRenderFrame, &hkFViewport_BeginRenderFrame);

			oFViewport_EndRenderFrame = module->rva_to<std::remove_pointer_t<decltype(oFViewport_EndRenderFrame)>>((uintptr_t)&search[0] + 0x80 - handle);
			DetourAttach(&(PVOID&)oFViewport_EndRenderFrame, &hkFViewport_EndRenderFrame);
		}

		// Similiar to official UE4 implementation
		search = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("40 57 48 83 EC 50 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 60 48 8B F9 48 8B 51 30 48 83 C2 07 48 83 E2 F8 48 8D 42 10 48 3B 41 38")));
		if (search != data.end()) {
			// We hook this and set a latencyMarker for RENDERSUBMIT_START then let execution flow as normal for ::BeginFrame
			FRHICommandList_BeginFrame = module->rva_to<std::remove_pointer_t<decltype(FRHICommandList_BeginFrame)>>((uintptr_t)&search[0] - handle);
			DetourAttach(&(PVOID&)FRHICommandList_BeginFrame, &hkFRHICommandList_BeginFrame);

			// We hook this and set a latencyMarker for RENDERSUBMIT_END then let execution flow as normal for ::EndFrame
			FRHICommandList_EndFrame = module->rva_to<std::remove_pointer_t<decltype(FRHICommandList_EndFrame)>>((uintptr_t)&search[0] + 0x110 - handle);
			DetourAttach(&(PVOID&)FRHICommandList_EndFrame, &hkFRHICommandList_EndFrame);
		}

		// This is... sketchy.. I don't recommend this but I'm leaving the code in for reference
		// We search for where FSlateRHIRenderer::DrawWindow_Renderthread calls FRHICommandList::EndDrawingViewport
		// Instead of doing this I am doing a some-what safer approach and setting the latency markers for present inside the IDXGISwapChain::Present hook
		// this is generally safer and stays consistent give or take a 0.15ms difference which for this game is nothing
		search = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("44 0F B6 4B 24 41 B0 01 49 8B 55 50")));
		if (search != data.end()) {
			// This is where arguments are being setup for FRHICommandList::EndDrawingViewport call
			// We want to intercept this point and set a latencymarker for PRESENT_START, At least that is how the official UE4 implementation is setup
			PresentInject_1 = module->rva_to<std::remove_pointer_t<decltype(PresentInject_1)>>((uintptr_t)&search[0] - handle); 

			// This is after the call to FRHICommandList::EndDrawingViewport is done and it's going to call a queryperformancecounter for internal measuresments
			// We want to intercept this point and set a latencymarker for PRESENT_END and then let the rest of the code for queryperformancecounter continue
			PresentInject_2 = module->rva_to<std::remove_pointer_t<decltype(PresentInject_2)>>((uintptr_t)&search[0] + 0x14 - handle);

			// Re-entry points to resume execution flow
			o_presentinject_1 = (uintptr_t)&search[0] + 0x5;
			o_presentinject_2 = (uintptr_t)&search[0] + 0x1B;

			//DetourAttach(&(PVOID&)PresentInject_1, &PresentInject_1_ASM);
			//DetourAttach(&(PVOID&)PresentInject_2, &PresentInject_2_ASM);
		}

		// This is just an InputKey handler for this game specifically
		auto sBinput = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("0F B6 47 18 48 8D 4C 24 30 89 03")));
		if (sBinput != data.end()) {
			uintptr_t aBinput = (uintptr_t)&sBinput[0] - 0x38;
			oBInputKey = module->rva_to<std::remove_pointer_t<decltype(oBInputKey)>>(aBinput - handle);
			response = DetourAttach(&(PVOID&)oBInputKey, &hkBInputKey);
		}

		auto result = DetourTransactionCommit();
		if (result == 0)
			ConsoleWrite(xorstr_(L"[reflex] TransactionCommit GOOD\n"));
		else
			ConsoleWrite(xorstr_(L"[reflex] TransactionCommit FAILED | 0x%08X\n"), result);
	}
	static PVOID cookie;
	if (tLdrRegisterDllNotification LdrRegisterDllNotification = reinterpret_cast<tLdrRegisterDllNotification>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "LdrRegisterDllNotification")))
		LdrRegisterDllNotification(0, DllNotification, NULL, &cookie); //Set a callback for when Dll's are loaded/unloaded

	return TRUE;
}

BOOL WINAPI DllMain(HMODULE hInstance, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hInstance);
	}

	return TRUE;
}

extern "C" __declspec(dllexport) PluginInfo GPluginInfo = {
  .hide_from_peb = true,
  .erase_pe_header = true,
  .init = init,
  .priority = 1,
  .target_apps = L"BNSR.exe"
};