/*
GpuRamDrive proxy for ImDisk Virtual Disk Driver.

Copyright (C) 2016 Syahmi Azhar.

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include "stdafx.h"
#include "GpuRamDrive.h"
#include "GpuRamGui.h"
#include "resource.h"
#include "TaskManager.h"
#include "DiskUtil.h"


#define GPU_GUI_CLASS L"GPURAMDRIVE_CLASS"
#define SWM_TRAYINTERACTION    WM_APP + 1

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

std::wstring ToWide(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;
	return convert.from_bytes(str);
}


GpuRamGui::GpuRamGui()
	: m_Icon(NULL)
	, m_IconMounted(NULL)
	, m_Instance(NULL)
	, m_hWnd(NULL)
	, m_AutoMount(false)
	, m_CtlGpuList(NULL)
	, m_CtlMountBtn(NULL)
	, m_CtlMemSize(NULL)
	, m_CtlDriveLetter(NULL)
	, m_CtlDriveType(NULL)
	, m_CtlDriveRemovable(NULL)
	, m_CtlDriveLabel(NULL)
	, m_CtlDriveFormat(NULL)
	, m_CtlImageFile(NULL)
	, m_CtlChooseFileBtn(NULL)
	, m_CtlReadOnly(NULL)
	, m_CtlTempFolder(NULL)
	, m_CtlStartOnWindows(NULL)
	, m_UpdateState(false)
	, wszAppName(L"GpuRamDrive")
	, wszTaskJobName(L"GPURAMDRIVE Task")
	, config(wszAppName)
	, diskUtil()
{
	INITCOMMONCONTROLSEX c;
	c.dwSize = sizeof(c);
	c.dwICC = 0;

	InitCommonControlsEx(&c);
}


GpuRamGui::~GpuRamGui()
{
}

bool GpuRamGui::Create(HINSTANCE hInst, const std::wstring& title, int nCmdShow, bool autoMount)
{
	m_Instance = hInst;
	m_AutoMount = autoMount;
	SetProcessDPIAware();

	MyRegisterClass();

	m_hWnd = CreateWindowW(GPU_GUI_CLASS, title.c_str(), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 700, 320, nullptr, nullptr, m_Instance, this);

	if (!m_hWnd) return false;

	ShowWindow(m_hWnd, nCmdShow);
	UpdateWindow(m_hWnd);

	return m_hWnd != NULL;
}

int GpuRamGui::Loop()
{
	MSG msg;

	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

void GpuRamGui::AutoMount()
{
	if (m_AutoMount)
	{
		int gpuSize = m_RamDrive[0].GetGpuDevices().size();
		for (int gpuId = 0; gpuId < gpuSize; gpuId++)
		{
			RestoreGuiParams(gpuId, 256);
			if (config.getStartOnWindows() == 1)
				OnMountClicked();
		}
	}
}

void GpuRamGui::RestoreWindow()
{
	ShowWindow(m_hWnd, SW_RESTORE);
	SetForegroundWindow(m_hWnd);
}

void GpuRamGui::OnCreate()
{
	SetWindowLongPtr(m_hWnd, GWL_STYLE, GetWindowLongPtr(m_hWnd, GWL_STYLE) & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX);

	HFONT FontBold = CreateFontA(-18, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Segoe UI");
	HFONT FontNormal = CreateFontA(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Segoe UI");
	HWND hStatic;

	hStatic = CreateWindow(L"STATIC", L"Select Device:", WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 10, 13, 140, 20, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(hStatic, WM_SETFONT, (WPARAM)FontNormal, TRUE);

	hStatic = CreateWindow(L"STATIC", L"Drive Letter/Type:", WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 10, 53, 140, 20, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(hStatic, WM_SETFONT, (WPARAM)FontNormal, TRUE);

	hStatic = CreateWindow(L"STATIC", L"Format/MB/Label:", WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 10, 93, 140, 20, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(hStatic, WM_SETFONT, (WPARAM)FontNormal, TRUE);

	hStatic = CreateWindow(L"STATIC", L"Image File:", WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 10, 133, 140, 20, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(hStatic, WM_SETFONT, (WPARAM)FontNormal, TRUE);

	m_CtlGpuList = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 150, 10, 150, 25, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(m_CtlGpuList, WM_SETFONT, (WPARAM)FontNormal, TRUE);

	m_CtlDriveLetter = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 150, 50, 150, 25, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(m_CtlDriveLetter, WM_SETFONT, (WPARAM)FontNormal, TRUE);

	m_CtlDriveType = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 315, 50, 150, 25, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(m_CtlDriveType, WM_SETFONT, (WPARAM)FontNormal, TRUE);

	m_CtlDriveRemovable = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 480, 50, 182, 25, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(m_CtlDriveRemovable, WM_SETFONT, (WPARAM)FontNormal, TRUE);

	m_CtlDriveFormat = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 150, 90, 150, 25, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(m_CtlDriveFormat, WM_SETFONT, (WPARAM)FontNormal, TRUE);

	m_CtlMemSize = CreateWindow(L"EDIT", L"1", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_NUMBER, 315, 90, 150, 28, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(m_CtlMemSize, WM_SETFONT, (WPARAM)FontNormal, TRUE);
	SendMessage(m_CtlMemSize, EM_SETCUEBANNER, 0, LPARAM(L"Mem size (MB)"));

	m_CtlDriveLabel = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP, 480, 90, 182, 28, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(m_CtlDriveLabel, WM_SETFONT, (WPARAM)FontNormal, TRUE);
	SendMessage(m_CtlDriveLabel, EM_SETCUEBANNER, 0, LPARAM(L"Volumen label"));

	m_CtlImageFile = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP, 150, 130, 315, 28, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(m_CtlImageFile, WM_SETFONT, (WPARAM)FontNormal, TRUE);
	SendMessage(m_CtlImageFile, EM_SETCUEBANNER, 0, LPARAM(L"Image file"));

	m_CtlChooseFileBtn = CreateWindow(L"BUTTON", L"...", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 480, 130, 25, 28, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(m_CtlChooseFileBtn, WM_SETFONT, (WPARAM)FontBold, TRUE);

	m_CtlReadOnly = CreateWindow(L"BUTTON", L"Read only", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX, 515, 132, 154, 25, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(m_CtlReadOnly, WM_SETFONT, (WPARAM)FontNormal, TRUE);

	m_CtlTempFolder = CreateWindow(L"BUTTON", L"Create TEMP Folder", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX, 515, 180, 154, 25, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(m_CtlTempFolder, WM_SETFONT, (WPARAM)FontNormal, TRUE);

	m_CtlStartOnWindows = CreateWindow(L"BUTTON", L"Start on windows", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX, 515, 220, 154, 25, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(m_CtlStartOnWindows, WM_SETFONT, (WPARAM)FontNormal, TRUE);

	m_CtlMountBtn = CreateWindow(L"BUTTON", L"Mount", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 150, 190, 150, 40, m_hWnd, NULL, m_Instance, NULL);
	SendMessage(m_CtlMountBtn, WM_SETFONT, (WPARAM)FontBold, TRUE);

	ReloadDriveLetterList();

	ComboBox_AddString(m_CtlDriveType, L"Hard Drive");
	ComboBox_AddString(m_CtlDriveType, L"Floppy Drive");
	ComboBox_AddString(m_CtlDriveRemovable, L"Non-Removable");
	ComboBox_AddString(m_CtlDriveRemovable, L"Removable");
	ComboBox_AddString(m_CtlDriveFormat, L"FAT32");
	ComboBox_AddString(m_CtlDriveFormat, L"exFAT");
	ComboBox_AddString(m_CtlDriveFormat, L"NTFS");

	int suggestedRamSize = -1;
	try
	{
		auto v = m_RamDrive[0].GetGpuDevices();
		int index = 0;
		for (auto it = v.begin(); it != v.end(); it++, index++)
		{
			ComboBox_AddString(m_CtlGpuList, ToWide(std::to_string(index) + ": " + it->name + " (" + std::to_string(it->memsize / (1024 * 1024)) + " MB)").c_str());
			if (suggestedRamSize == -1) {
#if GPU_API == GPU_API_HOSTMEM
				suggestedRamSize = 256;
#else
				suggestedRamSize = (int)((it->memsize / 1024 / 1024) - 1024);
#endif
			}

			m_RamDrive[index].SetStateChangeCallback([&]() {
				m_UpdateState = true;
				InvalidateRect(m_hWnd, NULL, FALSE);
				});
		}

		if (config.getGpuList() >= index)
		{
			config.setGpuList(0);
		}
	}
	catch (const std::exception& ex)
	{
		ComboBox_AddString(m_CtlGpuList, ToWide(ex.what()).c_str());
	}

	RestoreGuiParams(config.getGpuList(), suggestedRamSize);

	m_Tray.CreateIcon(m_hWnd, m_Icon, m_IconMounted, SWM_TRAYINTERACTION);
	m_Tray.SetTooltip(wszAppName, false);

	m_UpdateState = true;

	AutoMount();
}

void GpuRamGui::ReloadDriveLetterList()
{
	ComboBox_ResetContent(m_CtlDriveLetter);
	wchar_t szTemp[64];
	wcscpy_s(szTemp, L"X:");
	for (wchar_t c = 'A'; c <= 'Z'; c++)
	{
		wchar_t szTemp2[64];
		_snwprintf_s(szTemp, sizeof(szTemp), (diskUtil.checkDriveIsMounted(c, szTemp2) ? L"%c: - %s" : L"%c:%s"), c, szTemp2);
		ComboBox_AddString(m_CtlDriveLetter, szTemp);
	}
	ComboBox_SetCurSel(m_CtlDriveLetter, config.getDriveLetter());
}

boolean GpuRamGui::isMounted()
{
	boolean isMounted = false;
	int gpuSize = m_RamDrive[0].GetGpuDevices().size();
	for (int gpuId = 0; gpuId < gpuSize; gpuId++)
	{
		isMounted = m_RamDrive[gpuId].IsMounted();
		if (isMounted)
			break;
	}

	return isMounted;
}

void GpuRamGui::RestoreGuiParams(DWORD gpuId, DWORD suggestedRamSize)
{
	config.setGpuList(gpuId);
	m_Tray.SetTooltip(wszAppName, config.getDriveLetter());

	ComboBox_SetCurSel(m_CtlGpuList, gpuId);
	ComboBox_SetCurSel(m_CtlDriveLetter, config.getDriveLetter());
	ComboBox_SetCurSel(m_CtlDriveType, config.getDriveType());
	ComboBox_SetCurSel(m_CtlDriveRemovable, config.getDriveRemovable());
	ComboBox_SetCurSel(m_CtlDriveFormat, config.getDriveFormat());
	Button_SetCheck(m_CtlReadOnly, config.getReadOnly());
	Button_SetCheck(m_CtlTempFolder, config.getTempFolder());
	Button_SetCheck(m_CtlStartOnWindows, config.getStartOnWindows());

	wchar_t szTemp[1024] = {};
	wcscpy_s(szTemp, L"1");
	if (config.getMemSize() > 0) {
		_itow_s(config.getMemSize(), szTemp, 10);
	}
	else {
		_itow_s(suggestedRamSize, szTemp, 10);
	}
	Edit_SetText(m_CtlMemSize, szTemp);

	config.getDriveLabel(szTemp);
	Edit_SetText(m_CtlDriveLabel, szTemp);

	config.getImageFile(szTemp);
	Edit_SetText(m_CtlImageFile, szTemp);

	m_UpdateState = true;
	UpdateState();
}

void GpuRamGui::SaveGuiParams(DWORD gpuId)
{
	config.setGpuList(gpuId);
	config.setDriveLetter(ComboBox_GetCurSel(m_CtlDriveLetter));
	config.setDriveType(ComboBox_GetCurSel(m_CtlDriveType));
	config.setDriveRemovable(ComboBox_GetCurSel(m_CtlDriveRemovable));
	config.setDriveFormat(ComboBox_GetCurSel(m_CtlDriveFormat));
	config.setMemSize(ComboBox_GetCurSel(m_CtlDriveFormat));
	config.setReadOnly(Button_GetCheck(m_CtlReadOnly));
	config.setTempFolder(Button_GetCheck(m_CtlTempFolder));
	config.setStartOnWindows(Button_GetCheck(m_CtlStartOnWindows));

	wchar_t szTemp[64] = { 0 };
	Edit_GetText(m_CtlMemSize, szTemp, sizeof(szTemp) / sizeof(wchar_t));
	config.setMemSize((size_t)_wtoi64(szTemp));

	Edit_GetText(m_CtlDriveLabel, szTemp, sizeof(szTemp) / sizeof(wchar_t));
	config.setDriveLabel(szTemp);

	Edit_GetText(m_CtlImageFile, szTemp, sizeof(szTemp) / sizeof(wchar_t));
	config.setImageFile(szTemp);
}

void GpuRamGui::OnDestroy()
{
	int size = m_RamDrive[0].GetGpuDevices().size();
	for (int gpuId = 0; gpuId < size; gpuId++)
	{
		if (m_RamDrive[gpuId].IsMounted())
		{
			m_RamDrive[gpuId].ImdiskUnmountDevice();
		}
	}
	PostQuitMessage(0);
}

void GpuRamGui::OnEndSession()
{
	m_RamDrive[0].ImdiskUnmountDevice();
}

void GpuRamGui::OnResize(WORD width, WORD height, bool minimized)
{
	MoveWindow(m_CtlGpuList, 150, 10, width - 150 - 20, 20, TRUE);
	MoveWindow(m_CtlMountBtn, width / 2 - 150, height - 90, 300, 70, TRUE);

	if (m_RamDrive[config.getGpuList()].IsMounted() && minimized) {
		ShowWindow(m_hWnd, SW_HIDE);
	}
}

void GpuRamGui::OnMountClicked()
{
	int gpuId = ComboBox_GetCurSel(m_CtlGpuList);
	auto vGpu = m_RamDrive[0].GetGpuDevices();
	if (gpuId >= (int)vGpu.size()) {
		MessageBox(m_hWnd, L"GPU selection is invalid", L"Error while selecting GPU", MB_OK);
		return;
	}

	if (!m_RamDrive[ComboBox_GetCurSel(m_CtlGpuList)].IsMounted())
	{

		SaveGuiParams(ComboBox_GetCurSel(m_CtlGpuList));

		wchar_t szTemp[64] = { 0 };

		Edit_GetText(m_CtlMemSize, szTemp, sizeof(szTemp) / sizeof(wchar_t));
		size_t memSize = (size_t)_wtoi64(szTemp) * 1024 * 1024;

		ComboBox_GetText(m_CtlDriveFormat, szTemp, sizeof(szTemp) / sizeof(wchar_t));
		wchar_t format[64] = { 0 };
		_snwprintf_s(format, sizeof(format), L"/fs:%s /q", szTemp);
		std::wstring formatParam = format;

		Edit_GetText(m_CtlDriveLabel, szTemp, sizeof(szTemp) / sizeof(wchar_t));
		std::wstring labelParam = szTemp;

		bool tempFolderParam = Button_GetCheck(m_CtlTempFolder);

		EGpuRamDriveType driveType = ComboBox_GetCurSel(m_CtlDriveType) == 0 ? EGpuRamDriveType::HD : EGpuRamDriveType::FD;
		bool driveRemovable = ComboBox_GetCurSel(m_CtlDriveRemovable) == 0 ? false : true;

		if (memSize >= vGpu[gpuId].memsize) {
			MessageBox(m_hWnd, L"The memory size you specified is too large", L"Invalid memory size", MB_OK);
			return;
		}

		ComboBox_GetText(m_CtlDriveLetter, szTemp, sizeof(szTemp) / sizeof(wchar_t));
		wchar_t* mountPointParam = szTemp;

		if (diskUtil.checkDriveIsMounted(mountPointParam[0], NULL))
		{
			MessageBox(m_hWnd, L"It is not possible to mount the unit, it is already in use", wszAppName, MB_OK);
			return;
		}

		try
		{
			m_RamDrive[config.getGpuList()].SetDriveType(driveType);
			m_RamDrive[config.getGpuList()].SetRemovable(driveRemovable);
			m_RamDrive[config.getGpuList()].CreateRamDevice(vGpu[gpuId].platform_id, vGpu[gpuId].device_id, L"GpuRamDev_ " + std::to_wstring(mountPointParam[0]), memSize, mountPointParam, formatParam, labelParam, tempFolderParam);

			try
			{
				wchar_t szImageFile[MAX_PATH] = { 0 };
				config.getImageFile(szImageFile);
				if (wcslen(szImageFile) > 0 && diskUtil.fileExists(szImageFile)) {
					wchar_t szDeviceVolumen[MAX_PATH] = { 0 };
					_snwprintf_s(szDeviceVolumen, sizeof(szDeviceVolumen), L"\\\\.\\%s", mountPointParam);
					diskUtil.restore(szImageFile, szDeviceVolumen);
				}
			}
			catch (const std::exception& ex)
			{
				MessageBoxA(m_hWnd, ex.what(), "Error restoring the image file", MB_OK);
			}
		}
		catch (const std::exception& ex)
		{
			MessageBoxA(m_hWnd, ex.what(), "Error while mounting drive", MB_OK);
		}
	}
	else
	{
		try
		{
			if (!Button_GetCheck(m_CtlReadOnly))
			{
				wchar_t szImageFile[MAX_PATH] = { 0 };
				config.getImageFile(szImageFile);
				if (wcslen(szImageFile) > 0) {
					wchar_t szTemp[64] = { 0 };
					ComboBox_GetText(m_CtlDriveLetter, szTemp, sizeof(szTemp) / sizeof(wchar_t));

					wchar_t szDeviceVolumen[MAX_PATH] = { 0 };
					_snwprintf_s(szDeviceVolumen, sizeof(szDeviceVolumen), L"\\\\.\\%s", szTemp);

					diskUtil.save(szDeviceVolumen, szImageFile);
				}
			}
		}
		catch (const std::exception& ex)
		{
			MessageBoxA(m_hWnd, ex.what(), "Error saving the image file", MB_OK);
		}
		m_RamDrive[gpuId].ImdiskUnmountDevice();
	}
	ReloadDriveLetterList();
}

void GpuRamGui::OnTrayInteraction(LPARAM lParam)
{
	switch (lParam)
	{
		case WM_LBUTTONUP:
			if (IsWindowVisible(m_hWnd) && !IsIconic(m_hWnd)) {
				ShowWindow(m_hWnd, SW_HIDE);
			} else {
				ShowWindow(m_hWnd, SW_RESTORE);
				SetForegroundWindow(m_hWnd);
			}
			break;
		case WM_RBUTTONUP:
		case WM_CONTEXTMENU:
			break;
	}
}

void GpuRamGui::UpdateState()
{
	if (!m_UpdateState) return;

	if (m_RamDrive[config.getGpuList()].IsMounted())
	{
		//EnableWindow(m_CtlGpuList, FALSE);
		EnableWindow(m_CtlDriveLetter, FALSE);
		EnableWindow(m_CtlDriveType, FALSE);
		EnableWindow(m_CtlDriveRemovable, FALSE);
		EnableWindow(m_CtlDriveFormat, FALSE);
		EnableWindow(m_CtlMemSize, FALSE);
		EnableWindow(m_CtlDriveLabel, FALSE);
		EnableWindow(m_CtlImageFile, FALSE);
		EnableWindow(m_CtlChooseFileBtn, FALSE);
		EnableWindow(m_CtlReadOnly, FALSE);
		EnableWindow(m_CtlTempFolder, FALSE);
		EnableWindow(m_CtlStartOnWindows, FALSE);
		Edit_SetText(m_CtlMountBtn, L"Unmount");
	}
	else
	{
		//EnableWindow(m_CtlGpuList, TRUE);
		EnableWindow(m_CtlDriveLetter, TRUE);
		EnableWindow(m_CtlDriveType, TRUE);
		EnableWindow(m_CtlDriveRemovable, TRUE);
		EnableWindow(m_CtlDriveFormat, TRUE);
		EnableWindow(m_CtlMemSize, TRUE);
		EnableWindow(m_CtlDriveLabel, TRUE);
		EnableWindow(m_CtlImageFile, TRUE);
		EnableWindow(m_CtlChooseFileBtn, TRUE);
		EnableWindow(m_CtlReadOnly, TRUE);
		EnableWindow(m_CtlTempFolder, TRUE);
		EnableWindow(m_CtlStartOnWindows, TRUE);
		Edit_SetText(m_CtlMountBtn, L"Mount");
	}

	m_UpdateState = false;

	if (isMounted())
	{
		SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)m_IconMounted);
		SendMessage(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)m_IconMounted);
		m_Tray.SetTooltip(wszAppName, true);
	}
	else
	{
		SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)m_Icon);
		SendMessage(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)m_Icon);
		m_Tray.SetTooltip(wszAppName, false);
	}
}

ATOM GpuRamGui::MyRegisterClass()
{
	WNDCLASSEXW wcex;

	m_Icon = LoadIcon(m_Instance, MAKEINTRESOURCE(IDI_ICON1));
	m_IconMounted = LoadIcon(m_Instance, MAKEINTRESOURCE(IDI_ICON2));

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = m_Instance;
	wcex.hIcon = m_Icon;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = GPU_GUI_CLASS;
	wcex.hIconSm = wcex.hIcon;

	return RegisterClassExW(&wcex);
}

LRESULT CALLBACK GpuRamGui::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	GpuRamGui* _this = (GpuRamGui*)(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (message)
	{
		case WM_CREATE:
		{
			LPCREATESTRUCTW pCreateParam = (LPCREATESTRUCTW)lParam;
			if (pCreateParam) {
				_this = (GpuRamGui*)pCreateParam->lpCreateParams;
				SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pCreateParam->lpCreateParams);
			}

			if (_this) {
				_this->m_hWnd = hWnd;
				_this->OnCreate();
			}
			break;
		}
		case WM_CLOSE:
		{
			if (_this->isMounted())
			{
				if (MessageBox(hWnd, L"The drive is mounted, do you really want to exit?", _this->wszAppName, MB_OKCANCEL) == IDOK)
					DestroyWindow(hWnd);
			}
			else
				DestroyWindow(hWnd);
		}
		break;

		case WM_ENDSESSION:
			if (_this) _this->OnEndSession();
			break;

		case WM_DESTROY:
			if (_this) _this->OnDestroy();
			break;

		case WM_SIZE:
			if (_this) _this->OnResize(LOWORD(lParam), HIWORD(lParam), wParam == SIZE_MINIMIZED);
			break;

		case WM_PAINT:
			_this->UpdateState();
			return DefWindowProc(hWnd, message, wParam, lParam);
			break;

		case WM_SYSCOMMAND:
			if ((wParam & 0xFFF0) == SC_MINIMIZE)
			{
				if (_this)
				{
					_this->OnTrayInteraction(WM_LBUTTONUP);
				}
				break;
			}
			else
			{
				return DefWindowProc(hWnd, message, wParam, lParam);
			}

		case WM_COMMAND:
			if (_this) {
				if ((HANDLE)lParam == _this->m_CtlGpuList ||
					(HANDLE)lParam == _this->m_CtlDriveLetter ||
					(HANDLE)lParam == _this->m_CtlDriveFormat ||
					(HANDLE)lParam == _this->m_CtlDriveRemovable ||
					(HANDLE)lParam == _this->m_CtlDriveType) {
					if (HIWORD(wParam) == CBN_SELCHANGE) {
						_this->SaveGuiParams(_this->config.getGpuList());
						_this->m_Tray.SetTooltip(_this->wszAppName, _this->config.getDriveLetter());
					}
				}

				if ((HANDLE)lParam == _this->m_CtlMountBtn) {
					EnableWindow(_this->m_CtlMountBtn, FALSE);
					_this->OnMountClicked();
					EnableWindow(_this->m_CtlMountBtn, TRUE);
				}
				else if ((HANDLE)lParam == _this->m_CtlGpuList) {
					if (HIWORD(wParam) == CBN_SELCHANGE) {
						LRESULT itemIndex = SendMessage((HWND)lParam, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);

						TGPUDevice it = _this->m_RamDrive[0].GetGpuDevices().at(itemIndex);
#if GPU_API == GPU_API_HOSTMEM
						int suggestedRamSize = 256;
#else
						int suggestedRamSize = (int)((it.memsize / 1024 / 1024) - 1024);
#endif
						_this->RestoreGuiParams(itemIndex, suggestedRamSize);
						_this->ReloadDriveLetterList();
					}
				}
				else if ((HANDLE)lParam == _this->m_CtlReadOnly) {
					BOOL checked = Button_GetCheck(_this->m_CtlReadOnly);
					Button_SetCheck(_this->m_CtlReadOnly, !checked);
					_this->SaveGuiParams(_this->config.getGpuList());
				}
				else if ((HANDLE)lParam == _this->m_CtlTempFolder) {
					BOOL checked = Button_GetCheck(_this->m_CtlTempFolder);
					Button_SetCheck(_this->m_CtlTempFolder, !checked);
					_this->SaveGuiParams(_this->config.getGpuList());

					if (!checked) {
						_this->config.saveOriginalTempEnvironment();
					}
				}
				else if ((HANDLE)lParam == _this->m_CtlStartOnWindows) {
					BOOL checked = Button_GetCheck(_this->m_CtlStartOnWindows);
					Button_SetCheck(_this->m_CtlStartOnWindows, !checked);
					_this->SaveGuiParams(_this->config.getGpuList());

					TaskManager taskManager;
					if (checked) {
						taskManager.DeleteTaskJob(_this->wszTaskJobName);
					}
					int gpuSize = _this->m_RamDrive[0].GetGpuDevices().size();
					for (int gpuId = 0; gpuId < gpuSize; gpuId++)
					{
						if (_this->config.getStartOnWindows(gpuId))
						{
							wchar_t nPath[MAX_PATH] = {};
							GetModuleFileName(NULL, nPath, MAX_PATH);
							taskManager.CreateTaskJob(_this->wszTaskJobName, nPath, L"--autoMount --hide");
							break;
						}
					}
				}
				else if ((HANDLE)lParam == _this->m_CtlDriveLabel || (HANDLE)lParam == _this->m_CtlImageFile || (HANDLE)lParam == _this->m_CtlMemSize) {
					if (HIWORD(wParam) == EN_KILLFOCUS) {
						_this->SaveGuiParams(_this->config.getGpuList());
					}
					else {
						return DefWindowProc(hWnd, message, wParam, lParam);
					}
				}
				else if ((HANDLE)lParam == _this->m_CtlChooseFileBtn) {
					std::wstring file = _this->diskUtil.chooserFile(L"Select the image file", L"(*.img) Image file\0*.img\0");
					if (file.length() > 0) {
						Edit_SetText(_this->m_CtlImageFile, file.c_str());
						_this->SaveGuiParams(_this->config.getGpuList());
					}
				}
				else {
					return DefWindowProc(hWnd, message, wParam, lParam);
				}
			}
			break;

		case SWM_TRAYINTERACTION:
			if (_this) _this->OnTrayInteraction(lParam);
			break;

		default:
		{
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	return 0;
}
