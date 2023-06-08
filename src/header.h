#pragma once

#pragma comment(lib, "d3d11.lib")

#include <pe/module.h>
#include <xorstr/include/xorstr.hpp>
#include <pluginsdk.h>
#include <searchers.h>
#include <map>
#include <wil/include/wil/stl.h>
#include <wil/include/wil/win32_helpers.h>
#include "detours.h"

#include "Reflex.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_internal.h" // for free drawing

void ConsoleWrite(const wchar_t* msg, ...);
uintptr_t GetAddress(uintptr_t AddressOfCall, int index, int length);

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class BInputKey {
public:
	int Key;
	bool bCtrlPressed;
	bool bShiftPressed;
	bool bAltPressed;
	bool bDoubleClicked;
	bool bTpsModeKey;
};

enum class EngineKeyStateType {
	EKS_PRESSED = 0,
	EKS_RELEASED = 1,
	EKS_REPEAT = 2,
	EKS_DOUBLECLICK = 3,
	EKS_AXIS = 4
};

class EInputKeyEvent {
public:
	char padding[0x18];
	char _vKey;
	char padd_2[0x2];
	EngineKeyStateType KeyState;
	bool bCtrlPressed;
	bool bShiftPressed;
	bool bAltPressed;
};

class DeviceList {
private:
	ID3D11Device* _Device;
	ID3D11DeviceContext* _ImmediateContext;

public:
	DeviceList(ID3D11Device* device, ID3D11DeviceContext* context) {
		_Device = device;
		_ImmediateContext = context;
	}

	ID3D11Device* Device() {
		return _Device;
	}

	ID3D11DeviceContext* DeviceContext() {
		return _ImmediateContext;
	}
};