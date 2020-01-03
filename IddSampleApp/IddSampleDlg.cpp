
// IddSampleDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "IddSampleApp.h"
#include "IddSampleDlg.h"
#include "afxdialogex.h"

#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)

#define INITGUID

#include <windows.h>
#include <strsafe.h>
#include <cfgmgr32.h>
#include <stdio.h>
#include <stdlib.h>
#include <winioctl.h>
#include "public.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define MAX_DEVPATH_LENGTH    256
WCHAR G_DevicePath[MAX_DEVPATH_LENGTH];

void LOG(const char* format, ...)
{
    char szBuffer[1024];

    va_list vlArgs;
    va_start(vlArgs, format);
    _vsnprintf(szBuffer, sizeof(szBuffer) - 1, format, vlArgs);
    va_end(vlArgs);

    OutputDebugStringA(szBuffer);
}

//
// 根据GUID获取设备路径
//
BOOL GetDevicePath(
	_In_ LPGUID InterfaceGuid,
	_Out_writes_(BufLen) PWCHAR DevicePath,
	_In_ size_t BufLen
)
{
	CONFIGRET cr = CR_SUCCESS;
	PWSTR deviceInterfaceList = NULL;
	ULONG deviceInterfaceListLength = 0;
	PWSTR nextInterface;
	HRESULT hr = E_FAIL;
	BOOL bRet = TRUE;

	cr = CM_Get_Device_Interface_List_Size(
		&deviceInterfaceListLength,
		InterfaceGuid,
		NULL,
		CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
	if (cr != CR_SUCCESS) {
		LOG("Error 0x%x retrieving device interface list size.\n", cr);
		goto clean0;
	}

	if (deviceInterfaceListLength <= 1) {
		bRet = FALSE;
		LOG("Error: No active device interfaces found.\n"
			" Is the sample driver loaded?");
		goto clean0;
	}

	deviceInterfaceList = (PWSTR)malloc(deviceInterfaceListLength * sizeof(WCHAR));
	if (deviceInterfaceList == NULL) {
		LOG("Error allocating memory for device interface list.\n");
		goto clean0;
	}
	ZeroMemory(deviceInterfaceList, deviceInterfaceListLength * sizeof(WCHAR));

	cr = CM_Get_Device_Interface_List(
		InterfaceGuid,
		NULL,
		deviceInterfaceList,
		deviceInterfaceListLength,
		CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
	if (cr != CR_SUCCESS) {
		LOG("Error 0x%x retrieving device interface list.\n", cr);
		goto clean0;
	}

	nextInterface = deviceInterfaceList + wcslen(deviceInterfaceList) + 1;
	if (*nextInterface != UNICODE_NULL) {
		LOG("Warning: More than one device interface instance found. \n"
			"Selecting first matching device.\n\n");
	}

	hr = StringCchCopy(DevicePath, BufLen, deviceInterfaceList);
	if (FAILED(hr)) {
		bRet = FALSE;
		LOG("Error: StringCchCopy failed with HRESULT 0x%x", hr);
		goto clean0;
	}

clean0:
	if (deviceInterfaceList != NULL) {
		free(deviceInterfaceList);
	}
	if (CR_SUCCESS != cr) {
		bRet = FALSE;
	}

	return bRet;
}

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CIddSampleDlg 对话框



CIddSampleDlg::CIddSampleDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_IDDSAMPLEAPP_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CIddSampleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CIddSampleDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
END_MESSAGE_MAP()


// CIddSampleDlg 消息处理程序

BOOL CIddSampleDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码

	HANDLE  hDevice = INVALID_HANDLE_VALUE;
	HANDLE  th1 = NULL;
	BOOLEAN result = TRUE;

	//
	// 根据GUID获取设备路径
	//
	if (!GetDevicePath(
		(LPGUID)&GUID_DEVINTERFACE_ID,
		G_DevicePath,
		sizeof(G_DevicePath) / sizeof(G_DevicePath[0])))
	{
		return FALSE;
	}

	LOG("DevicePath: %ws\n", G_DevicePath);

	//
	// 建立驱动设备
	//
	hDevice = CreateFile(G_DevicePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (hDevice == INVALID_HANDLE_VALUE) {
		LOG("Failed to open device. Error %d\n", GetLastError());
		return FALSE;
	}

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CIddSampleDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CIddSampleDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CIddSampleDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

