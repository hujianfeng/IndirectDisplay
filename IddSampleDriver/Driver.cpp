/*

Copyright (c) Microsoft Corporation

Abstract:

    This module contains a sample implementation of an indirect display driver. See the included README.md file and the
    various TODO blocks throughout this file and all accompanying files for information on building a production driver.
    此模块包含间接显示驱动程序的示例实现。请参阅随附的README.md文件以及此文件及所有随附文件中的各种TODO块，以获取有关构建生产驱动程序的信息。

    MSDN documentation on indirect displays can be found at https://msdn.microsoft.com/en-us/library/windows/hardware/mt761968(v=vs.85).aspx.
    可以在以下位置找到有关间接显示的MSDN文档：https://msdn.microsoft.com/en-us/library/windows/hardware/mt761968(v=vs.85).aspx.

Environment:

    User Mode, UMDF
    用户模式，UMDF

*/

#include "Driver.h"
#include "Driver.tmh"

using namespace std;
using namespace Microsoft::IndirectDisp;
using namespace Microsoft::WRL;

extern "C" DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD IddSampleDeviceAdd;
EVT_WDF_DEVICE_D0_ENTRY IddSampleDeviceD0Entry;

EVT_IDD_CX_DEVICE_IO_CONTROL IddSampleIoDeviceControl;
EVT_IDD_CX_ADAPTER_INIT_FINISHED IddSampleAdapterInitFinished;
EVT_IDD_CX_ADAPTER_COMMIT_MODES IddSampleAdapterCommitModes;

EVT_IDD_CX_PARSE_MONITOR_DESCRIPTION IddSampleParseMonitorDescription;
EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES IddSampleMonitorGetDefaultModes;
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES IddSampleMonitorQueryModes;

EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN IddSampleMonitorAssignSwapChain;
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN IddSampleMonitorUnassignSwapChain;

struct IndirectDeviceContextWrapper
{
    IndirectDeviceContext* pContext;

    void Cleanup()
    {
        delete pContext;
        pContext = nullptr;
    }
};

void LOG(const char* format, ...)
{
    char szBuffer[1024];

    va_list vlArgs;
    va_start(vlArgs, format);
    _vsnprintf(szBuffer, sizeof(szBuffer) - 1, format, vlArgs);
    va_end(vlArgs);

    OutputDebugStringA(szBuffer);
}

// This macro creates the methods for accessing an IndirectDeviceContextWrapper as a context for a WDF object
// 此宏创建用于将IndirectDeviceContextWrapper作为WDF对象的上下文访问的方法
WDF_DECLARE_CONTEXT_TYPE(IndirectDeviceContextWrapper);

extern "C" BOOL WINAPI DllMain(
    _In_ HINSTANCE hInstance,
    _In_ UINT dwReason,
    _In_opt_ LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);
    UNREFERENCED_PARAMETER(dwReason);

    LOG("IndirectDisplay, DllMain\n");

    return TRUE;
}

//
// 驱动程序入口函数
//
_Use_decl_annotations_
extern "C" NTSTATUS DriverEntry(
    PDRIVER_OBJECT  driverObject,
    PUNICODE_STRING registryPath
)
{
    LOG("IndirectDisplay, DriverEntry\n");

    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    // 注册设备上电后的回调函数IddSampleDeviceAdd。
    WDF_DRIVER_CONFIG_INIT(
        &config,
        IddSampleDeviceAdd
    );

    // 创建驱动程序对象
    LOG("IndirectDisplay, WdfDriverCreate\n");
    status = WdfDriverCreate(
        driverObject, 
        registryPath, 
        WDF_NO_OBJECT_ATTRIBUTES, //&attributes, 
        &config, 
        WDF_NO_HANDLE);
    if (!NT_SUCCESS(status))
    {
        LOG("Error: WdfDriverCreate failed 0x%x\n", status);
        return status;
    }

    return status;
}

//_Use_decl_annotations_
NTSTATUS IddSampleDeviceAdd(WDFDRIVER driver, PWDFDEVICE_INIT deviceInit)
{
    LOG("IndirectDisplay, IddSampleDeviceAdd\n");

    NTSTATUS status = STATUS_SUCCESS;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;

    UNREFERENCED_PARAMETER(driver);

    // Register for power callbacks - in this sample only power-on is needed
    // 注册电源回调-在此示例中，仅需要开机。
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDeviceD0Entry = IddSampleDeviceD0Entry;
    WdfDeviceInitSetPnpPowerEventCallbacks(deviceInit, &pnpPowerCallbacks);

    IDD_CX_CLIENT_CONFIG iddConfig;
    IDD_CX_CLIENT_CONFIG_INIT(&iddConfig);

    // If the driver wishes to handle custom IoDeviceControl requests, it's necessary to use this callback since IddCx
    // redirects IoDeviceControl requests to an internal queue. This sample does not need this.
    // 如果驱动程序希望处理自定义IoDeviceControl请求，则必须使用此回调，因为IddCx将IoDeviceControl请求重定向到内部队列。 该示例不需要此。

    iddConfig.EvtIddCxDeviceIoControl = IddSampleIoDeviceControl;
    iddConfig.EvtIddCxAdapterInitFinished = IddSampleAdapterInitFinished;
    iddConfig.EvtIddCxParseMonitorDescription = IddSampleParseMonitorDescription;
    iddConfig.EvtIddCxMonitorGetDefaultDescriptionModes = IddSampleMonitorGetDefaultModes;
    iddConfig.EvtIddCxMonitorQueryTargetModes = IddSampleMonitorQueryModes;
    iddConfig.EvtIddCxAdapterCommitModes = IddSampleAdapterCommitModes;
    iddConfig.EvtIddCxMonitorAssignSwapChain = IddSampleMonitorAssignSwapChain;
    iddConfig.EvtIddCxMonitorUnassignSwapChain = IddSampleMonitorUnassignSwapChain;

    status = IddCxDeviceInitConfig(deviceInit, &iddConfig);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, IndirectDeviceContextWrapper);
    deviceAttributes.EvtCleanupCallback = [](WDFOBJECT Object)
    {
        // Automatically cleanup the context when the WDF object is about to be deleted
        // WDF对象即将删除时自动清除上下文
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Object);
        if (pContext)
        {
            pContext->Cleanup();
        }
    };

    WDFDEVICE device = nullptr;

    // 创建框架设备对象。
    LOG("IndirectDisplay, WdfDeviceCreate\n");
    status = WdfDeviceCreate(&deviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status))
    {
        LOG("IndirectDisplay, WdfDeviceCreate Error\n");
        return status;
    }

    //
    // Create a device interface so that application can find and talk to us.
    // 创建一个设备接口，以便应用程序可以找到我们并与我们交谈。
    //
    LOG("IndirectDisplay, WdfDeviceCreateDeviceInterface\n");
    status = WdfDeviceCreateDeviceInterface(
        device,
        &GUID_DEVINTERFACE_ID,
        NULL // ReferenceString
    );

    if (!NT_SUCCESS(status))
    {
        LOG("IndirectDisplay, WdfDeviceCreateDeviceInterface Error\n");
        return status;
    }

    status = IddCxDeviceInitialize(device);

    // Create a new device context object and attach it to the WDF device object
    // 创建一个新的设备上下文对象并将其附加到WDF设备对象
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(device);
    pContext->pContext = new IndirectDeviceContext(device);

    return status;
}

_Use_decl_annotations_
NTSTATUS IddSampleDeviceD0Entry(WDFDEVICE device, WDF_POWER_DEVICE_STATE previousState)
{
    LOG("IndirectDisplay, IddSampleDeviceD0Entry\n");

    UNREFERENCED_PARAMETER(previousState);

    // This function is called by WDF to start the device in the fully-on power state.
    // WDF调用此功能以在完全通电状态下启动设备。
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(device);
    pContext->pContext->InitAdapter();

    return STATUS_SUCCESS;
}

#pragma region Direct3DDevice

Direct3DDevice::Direct3DDevice(LUID adapterLuid) : AdapterLuid(adapterLuid)
{
    LOG("IndirectDisplay, Direct3DDevice::Direct3DDevice\n");
}

Direct3DDevice::Direct3DDevice()
{
    LOG("IndirectDisplay, Direct3DDevice::Direct3DDevice\n");
}

HRESULT Direct3DDevice::Init()
{
    LOG("IndirectDisplay, Direct3DDevice::Init\n");

    // The DXGI factory could be cached, but if a new render adapter appears on the system, a new factory needs to be
    // created. If caching is desired, check DxgiFactory->IsCurrent() each time and recreate the factory if !IsCurrent.
    // 可以缓存DXGI工厂，但是如果系统上出现新的渲染适配器，则需要创建一个新工厂。
    // 如果需要缓存，则每次检查DxgiFactory->IsCurrent（），如果！IsCurrent则重新创建工厂。
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&DxgiFactory));
    if (FAILED(hr))
    {
        return hr;
    }

    // Find the specified render adapter
    // 查找指定的渲染适配器
    hr = DxgiFactory->EnumAdapterByLuid(AdapterLuid, IID_PPV_ARGS(&Adapter));
    if (FAILED(hr))
    {
        return hr;
    }

    // Create a D3D device using the render adapter. BGRA support is required by the WHQL test suite.
    // 使用渲染适配器创建D3D设备。 WHQL测试套件需要BGRA支持。
    hr = D3D11CreateDevice(Adapter.Get(), 
        D3D_DRIVER_TYPE_UNKNOWN, 
        nullptr, 
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, 
        nullptr, 
        0, 
        D3D11_SDK_VERSION, 
        &Device, 
        nullptr, 
        &DeviceContext);
    if (FAILED(hr))
    {
        // If creating the D3D device failed, it's possible the render GPU was lost (e.g. detachable GPU) or else the
        // system is in a transient state.
        // 如果创建D3D设备失败，则渲染GPU可能丢失（例如可拆卸的GPU），否则系统处于过渡状态。
        return hr;
    }

    return S_OK;
}

#pragma endregion

#pragma region SwapChainProcessor

SwapChainProcessor::SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain, shared_ptr<Direct3DDevice> device, HANDLE newFrameEvent)
    : m_hSwapChain(hSwapChain), m_Device(device), m_hAvailableBufferEvent(newFrameEvent)
{
    LOG("IndirectDisplay, SwapChainProcessor::SwapChainProcessor\n");

    m_hTerminateEvent.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));

    // Immediately create and run the swap-chain processing thread, passing 'this' as the thread parameter
    // 立即创建并运行交换链处理线程，并将“ this”作为线程参数传递
    m_hThread.Attach(CreateThread(nullptr, 0, RunThread, this, 0, nullptr));
}

SwapChainProcessor::~SwapChainProcessor()
{
    LOG("IndirectDisplay, SwapChainProcessor::~SwapChainProcessor\n");

    // Alert the swap-chain processing thread to terminate
    // 提醒交换链处理线程终止
    SetEvent(m_hTerminateEvent.Get());

    if (m_hThread.Get())
    {
        // Wait for the thread to terminate
        // 等待线程终止
        WaitForSingleObject(m_hThread.Get(), INFINITE);
    }
}

DWORD CALLBACK SwapChainProcessor::RunThread(LPVOID argument)
{
    LOG("IndirectDisplay, SwapChainProcessor::RunThread\n");

    reinterpret_cast<SwapChainProcessor*>(argument)->Run();
    return 0;
}

void SwapChainProcessor::Run()
{
    LOG("IndirectDisplay, SwapChainProcessor::Run\n");

    // For improved performance, make use of the Multimedia Class Scheduler Service, which will intelligently
    // prioritize this thread for improved throughput in high CPU-load scenarios.
    // 为了提高性能，请使用多媒体类调度程序服务，该服务将智能地对该线程进行优先级排序，
    // 以在高CPU负载情况下提高吞吐量。
    DWORD AvTask = 0;
    HANDLE AvTaskHandle = AvSetMmThreadCharacteristics(L"Distribution", &AvTask);

    RunCore();

    // Always delete the swap-chain object when swap-chain processing loop terminates in order to kick the system to
    // provide a new swap-chain if necessary.
    // 交换链处理循环终止时，请务必删除交换链对象，以在必要时踢出系统以提供新的交换链。
    WdfObjectDelete((WDFOBJECT)m_hSwapChain);
    m_hSwapChain = nullptr;

    AvRevertMmThreadCharacteristics(AvTaskHandle);
}

void SwapChainProcessor::RunCore()
{
    LOG("IndirectDisplay, SwapChainProcessor::RunCore\n");

    // Get the DXGI device interface
    // 获取DXGI设备接口
    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = m_Device->Device.As(&dxgiDevice);
    if (FAILED(hr))
    {
        return;
    }

    IDARG_IN_SWAPCHAINSETDEVICE setDevice = {};
    setDevice.pDevice = dxgiDevice.Get();

    hr = IddCxSwapChainSetDevice(m_hSwapChain, &setDevice);
    if (FAILED(hr))
    {
        return;
    }

    // Acquire and release buffers in a loop
    // 循环获取和释放缓冲区
    for (;;)
    {
        ComPtr<IDXGIResource> acquiredBuffer;

        // Ask for the next buffer from the producer
        // 向生产者请求下一个缓冲区
        IDARG_OUT_RELEASEANDACQUIREBUFFER buffer = {};
        hr = IddCxSwapChainReleaseAndAcquireBuffer(m_hSwapChain, &buffer);

        // AcquireBuffer immediately returns STATUS_PENDING if no buffer is yet available
        if (hr == E_PENDING)
        {
            // We must wait for a new buffer
            // 等待一个新的缓冲区
            HANDLE waitHandles [] =
            {
                m_hAvailableBufferEvent,
                m_hTerminateEvent.Get()
            };
            DWORD WaitResult = WaitForMultipleObjects(ARRAYSIZE(waitHandles), waitHandles, FALSE, 16);
            if (WaitResult == WAIT_OBJECT_0 || WaitResult == WAIT_TIMEOUT)
            {
                // We have a new buffer, so try the AcquireBuffer again
                // 有一个新的缓冲区，因此请再次尝试AcquireBuffer
                continue;
            }
            else if (WaitResult == WAIT_OBJECT_0 + 1)
            {
                // We need to terminate
                // 需要终止
                break;
            }
            else
            {
                // The wait was cancelled or something unexpected happened
                // 等待已取消或发生了意外情况
                hr = HRESULT_FROM_WIN32(WaitResult);
                break;
            }
        }
        else if (SUCCEEDED(hr))
        {
            acquiredBuffer.Attach(buffer.MetaData.pSurface);

            // ==============================
            // TODO: Process the frame here
            //
            // This is the most performance-critical section of code in an IddCx driver. It's important that whatever
            // is done with the acquired surface be finished as quickly as possible. This operation could be:
            //  * a GPU copy to another buffer surface for later processing (such as a staging surface for mapping to CPU memory)
            //  * a GPU encode operation
            //  * a GPU VPBlt to another surface
            //  * a GPU custom compute shader encode operation
            //
            // 待办事项：在此处处理帧
            // 这是IddCx驱动程序中性能最关键的部分。 重要的是，尽快完成对获取的表面所做的任何操作。 
            // 该操作可能是：
            //  * 将GPU复制到另一个缓冲区表面以进行后续处理（例如用于映射到CPU内存的临时表面）
            //  * GPU编码操作
            //  * 将GPU VPBlt连接到另一个表面
            //  * GPU自定义计算着色器编码操作
            //
            // ==============================

            acquiredBuffer.Reset();
            hr = IddCxSwapChainFinishedProcessingFrame(m_hSwapChain);
            if (FAILED(hr))
            {
                break;
            }

            // ==============================
            // TODO: Report frame statistics once the asynchronous encode/send work is completed
            // Drivers should report information about sub-frame timings, like encode time, send time, etc.
            //
            // 待办事项：异步编码/发送工作完成后，报告帧统计信息
            // 驱动应报告有关子帧计时的信息，例如编码时间，发送时间等。
            // ==============================
            // IddCxSwapChainReportFrameStatistics(m_hSwapChain, ...);
        }
        else
        {
            // The swap-chain was likely abandoned (e.g. DXGI_ERROR_ACCESS_LOST), so exit the processing loop
            // 交换链可能已被废弃（例如DXGI_ERROR_ACCESS_LOST），因此退出处理循环。
            break;
        }
    }
}

#pragma endregion

#pragma region IndirectDeviceContext

const UINT64 MHZ = 1000000;
const UINT64 KHZ = 1000;

// A list of modes exposed by the sample monitor EDID - FOR SAMPLE PURPOSES ONLY
// 例子监视器EDID公开的模式列表 - 仅供样品使用
const DISPLAYCONFIG_VIDEO_SIGNAL_INFO IndirectDeviceContext::s_KnownMonitorModes[] =
{
    // 800 x 600 @ 60Hz
    {
          40 * MHZ,                                      // pixel clock rate [Hz]
        { 40 * MHZ, 800 + 256 },                         // fractional horizontal refresh rate [Hz]
        { 40 * MHZ, (800 + 256) * (600 + 28) },          // fractional vertical refresh rate [Hz]
        { 800, 600 },                                    // (horizontal, vertical) active pixel resolution
        { 800 + 256, 600 + 28 },                         // (horizontal, vertical) total pixel resolution
        { { 255, 0 }},                                   // video standard and vsync divider
        DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE
    },
    // 640 x 480 @ 60Hz
    {
          25175 * KHZ,                                   // pixel clock rate [Hz]
        { 25175 * KHZ, 640 + 160 },                      // fractional horizontal refresh rate [Hz]
        { 25175 * KHZ, (640 + 160) * (480 + 46) },       // fractional vertical refresh rate [Hz]
        { 640, 480 },                                    // (horizontal, vertical) active pixel resolution
        { 640 + 160, 480 + 46 },                         // (horizontal, vertical) blanking pixel resolution
        { { 255, 0 } },                                  // video standard and vsync divider
        DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE
    },
};

// This is a sample monitor EDID - FOR SAMPLE PURPOSES ONLY
// 这是一个示例监视器EDID-仅用于示例
const BYTE IndirectDeviceContext::s_KnownMonitorEdid[] =
{
    0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x79,0x5E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xA6,0x01,0x03,0x80,0x28,
    0x1E,0x78,0x0A,0xEE,0x91,0xA3,0x54,0x4C,0x99,0x26,0x0F,0x50,0x54,0x20,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x01,
    0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0xA0,0x0F,0x20,0x00,0x31,0x58,0x1C,0x20,0x28,0x80,0x14,0x00,
    0x90,0x2C,0x11,0x00,0x00,0x1E,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x6E
};

IndirectDeviceContext::IndirectDeviceContext(_In_ WDFDEVICE WdfDevice) :
    m_WdfDevice(WdfDevice)
{
    LOG("IndirectDisplay, IndirectDeviceContext::IndirectDeviceContext\n");
}

IndirectDeviceContext::~IndirectDeviceContext()
{
    LOG("IndirectDisplay, IndirectDeviceContext::~IndirectDeviceContext\n");

    m_ProcessingThread.reset();
}

void IndirectDeviceContext::InitAdapter()
{
    LOG("IndirectDisplay, IndirectDeviceContext::InitAdapter\n");

    // ==============================
    // TODO: Update the below diagnostic information in accordance with the target hardware. The strings and version
    // numbers are used for telemetry and may be displayed to the user in some situations.
    //
    // This is also where static per-adapter capabilities are determined.
    //
    // 待办事项：根据目标硬件更新以下诊断信息。 字符串和版本号用于遥测，在某些情况下可能会显示给用户。
    //
    // 这也是确定静态每个适配器功能的地方。
    //
    // ==============================

    IDDCX_ADAPTER_CAPS AdapterCaps = {};
    AdapterCaps.Size = sizeof(AdapterCaps);

    // Declare basic feature support for the adapter (required)
    // 声明对适配器的基本功能的支持（必需）
    AdapterCaps.MaxMonitorsSupported = 1;
    AdapterCaps.EndPointDiagnostics.Size = sizeof(AdapterCaps.EndPointDiagnostics);
    AdapterCaps.EndPointDiagnostics.GammaSupport = IDDCX_FEATURE_IMPLEMENTATION_NONE;
    AdapterCaps.EndPointDiagnostics.TransmissionType = IDDCX_TRANSMISSION_TYPE_WIRED_OTHER;

    // Declare your device strings for telemetry (required)
    // 声明设备字符串以进行遥测（必填）
    AdapterCaps.EndPointDiagnostics.pEndPointFriendlyName = L"IddSample Device";
    AdapterCaps.EndPointDiagnostics.pEndPointManufacturerName = L"Microsoft";
    AdapterCaps.EndPointDiagnostics.pEndPointModelName = L"IddSample Model";

    // Declare your hardware and firmware versions (required)
    // 声明您的硬件和固件版本（必填）
    IDDCX_ENDPOINT_VERSION Version = {};
    Version.Size = sizeof(Version);
    Version.MajorVer = 1;
    AdapterCaps.EndPointDiagnostics.pFirmwareVersion = &Version;
    AdapterCaps.EndPointDiagnostics.pHardwareVersion = &Version;

    // Initialize a WDF context that can store a pointer to the device context object
    // 初始化可以存储指向设备上下文对象的指针的WDF上下文
    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);

    IDARG_IN_ADAPTER_INIT AdapterInit = {};
    AdapterInit.WdfDevice = m_WdfDevice;
    AdapterInit.pCaps = &AdapterCaps;
    AdapterInit.ObjectAttributes = &Attr;

    // Start the initialization of the adapter, which will trigger the AdapterFinishInit callback later
    // 开始适配器的初始化，稍后将触发AdapterFinishInit回调
    IDARG_OUT_ADAPTER_INIT AdapterInitOut;
    NTSTATUS Status = IddCxAdapterInitAsync(&AdapterInit, &AdapterInitOut);

    if (NT_SUCCESS(Status))
    {
        // Store a reference to the WDF adapter handle
        // 存储对WDF适配器句柄的引用
        m_Adapter = AdapterInitOut.AdapterObject;

        // Store the device context object into the WDF object context
        // 将设备上下文对象存储到WDF对象上下文中
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(AdapterInitOut.AdapterObject);
        pContext->pContext = this;
    }
}

void IndirectDeviceContext::FinishInit()
{
    LOG("IndirectDisplay, IndirectDeviceContext::FinishInit\n");

    // ==============================
    // TODO: In a real driver, the EDID should be retrieved dynamically from a connected physical monitor. The EDID
    // provided here is purely for demonstration, as it describes only 640x480 @ 60 Hz and 800x600 @ 60 Hz. Monitor
    // manufacturers are required to correctly fill in physical monitor attributes in order to allow the OS to optimize
    // settings like viewing distance and scale factor. Manufacturers should also use a unique serial number every
    // single device to ensure the OS can tell the monitors apart.
    //
    // 待办事项：在实际驱动程序中，应该从连接的物理监视器中动态检索EDID。 
    // 此处提供的EDID仅用于演示，因为它仅描述了640x480 @ 60 Hz和800x600 @ 60 Hz。 
    // 监视器制造商需要正确填写物理监视器属性，以允许操作系统优化诸如查看距离和比例因子之类的设置。
    // 制造商还应在每个设备上使用唯一的序列号，以确保操作系统可以区分显示器。
    //
    // ==============================

    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);

    IDDCX_MONITOR_INFO MonitorInfo = {};
    MonitorInfo.Size = sizeof(MonitorInfo);
    MonitorInfo.MonitorType = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI;
    MonitorInfo.ConnectorIndex = 0;
    MonitorInfo.MonitorDescription.Size = sizeof(MonitorInfo.MonitorDescription);
    MonitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;
    MonitorInfo.MonitorDescription.DataSize = sizeof(s_KnownMonitorEdid);
    MonitorInfo.MonitorDescription.pData = const_cast<BYTE*>(s_KnownMonitorEdid);

    // ==============================
    // TODO: The monitor's container ID should be distinct from "this" device's container ID if the monitor is not
    // permanently attached to the display adapter device object. The container ID is typically made unique for each
    // monitor and can be used to associate the monitor with other devices, like audio or input devices. In this
    // sample we generate a random container ID GUID, but it's best practice to choose a stable container ID for a
    // unique monitor or to use "this" device's container ID for a permanent/integrated monitor.
    //
    // 待办事项：如果监视器不是永久地附加到显示适配器设备对象上，则监视器的容器ID应与“此”设备的容器ID不同。 
    // 通常，容器ID对于每个监视器都是唯一的，并且可用于将监视器与其他设备（例如音频或输入设备）关联。 
    // 在此示例中，我们生成了一个随机的容器ID GUID，但是最佳实践是为唯一的监视器选择一个稳定的容器ID，
    // 或者为永久/集成监视器使用“此”设备的容器ID。
    //
    // ==============================

    // Create a container ID
    // 建立一个容器ID
    CoCreateGuid(&MonitorInfo.MonitorContainerId);

    IDARG_IN_MONITORCREATE MonitorCreate = {};
    MonitorCreate.ObjectAttributes = &Attr;
    MonitorCreate.pMonitorInfo = &MonitorInfo;

    // Create a monitor object with the specified monitor descriptor
    // 使用指定的监视器描述符创建监视器对象
    IDARG_OUT_MONITORCREATE MonitorCreateOut;
    NTSTATUS Status = IddCxMonitorCreate(m_Adapter, &MonitorCreate, &MonitorCreateOut);
    if (NT_SUCCESS(Status))
    {
        m_Monitor = MonitorCreateOut.MonitorObject;

        // Associate the monitor with this device context
        // 将监视器与此设备上下文关联
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorCreateOut.MonitorObject);
        pContext->pContext = this;

        // Tell the OS that the monitor has been plugged in
        // 告诉操作系统显示器已插入
        IDARG_OUT_MONITORARRIVAL ArrivalOut;
        Status = IddCxMonitorArrival(m_Monitor, &ArrivalOut);
    }
}

void IndirectDeviceContext::AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID RenderAdapter, HANDLE NewFrameEvent)
{
    LOG("IndirectDisplay, IndirectDeviceContext::AssignSwapChain\n");

    m_ProcessingThread.reset();

    auto Device = make_shared<Direct3DDevice>(RenderAdapter);
    if (FAILED(Device->Init()))
    {
        // It's important to delete the swap-chain if D3D initialization fails, so that the OS knows to generate a new
        // swap-chain and try again.
        // 如果D3D初始化失败，请删除交换链，这一点很重要，以便OS知道生成新的交换链并重试。
        WdfObjectDelete(SwapChain);
    }
    else
    {
        // Create a new swap-chain processing thread
        // 创建一个新的交换链处理线程
        m_ProcessingThread.reset(new SwapChainProcessor(SwapChain, Device, NewFrameEvent));
    }
}

void IndirectDeviceContext::UnassignSwapChain()
{
    LOG("IndirectDisplay, IndirectDeviceContext::UnassignSwapChain\n");

    // Stop processing the last swap-chain
    // 停止处理最后一个交换链
    m_ProcessingThread.reset();
}

#pragma endregion

#pragma region DDI Callbacks

_Use_decl_annotations_
VOID IddSampleIoDeviceControl(
    WDFDEVICE Device,
    WDFREQUEST Request,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode)
{
    LOG("IndirectDisplay, IddSampleIoDeviceControl\n");

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(IoControlCode);

}

_Use_decl_annotations_
NTSTATUS IddSampleAdapterInitFinished(IDDCX_ADAPTER AdapterObject, const IDARG_IN_ADAPTER_INIT_FINISHED* pInArgs)
{
    LOG("IndirectDisplay, IddSampleAdapterInitFinished\n");

    // This is called when the OS has finished setting up the adapter for use by the IddCx driver. It's now possible
    // to report attached monitors.
    // 当操作系统完成设置适配器以供IddCx驱动程序使用时，将调用此方法。 现在可以报告已连接的监视器。

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(AdapterObject);
    if (NT_SUCCESS(pInArgs->AdapterInitStatus))
    {
        pContext->pContext->FinishInit();
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleAdapterCommitModes(IDDCX_ADAPTER AdapterObject, const IDARG_IN_COMMITMODES* pInArgs)
{
    LOG("IndirectDisplay, IddSampleAdapterCommitModes\n");

    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(pInArgs);

    // For the sample, do nothing when modes are picked - the swap-chain is taken care of by IddCx
    // 对于样本，在选择模式时不执行任何操作-交换链由IddCx处理

    // ==============================
    // TODO: In a real driver, this function would be used to reconfigure the device to commit the new modes. Loop
    // through pInArgs->pPaths and look for IDDCX_PATH_FLAGS_ACTIVE. Any path not active is inactive (e.g. the monitor
    // should be turned off).
    //
    // 待办事项：在实际驱动程序中，此功能将用于重新配置设备以提交新模式。 遍历pInArgs->pPaths并查找IDDCX_PATH_FLAGS_ACTIVE。 
    // 任何未激活的路径均处于非激活状态（例如，应关闭监视器）。
    //
    // ==============================

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleParseMonitorDescription(const IDARG_IN_PARSEMONITORDESCRIPTION* pInArgs, 
    IDARG_OUT_PARSEMONITORDESCRIPTION* pOutArgs)
{
    LOG("IndirectDisplay, IddSampleParseMonitorDescription\n");

    // ==============================
    // TODO: In a real driver, this function would be called to generate monitor modes for an EDID by parsing it. In
    // this sample driver, we hard-code the EDID, so this function can generate known modes.
    //
    // 待办事项：在实际的驱动程序中，将调用此函数以通过解析生成EDID的监视模式。 
    // 在此示例驱动程序中，我们对EDID进行了硬编码，因此此函数可以生成已知模式。
    //
    // ==============================

    pOutArgs->MonitorModeBufferOutputCount = ARRAYSIZE(IndirectDeviceContext::s_KnownMonitorModes);

    if (pInArgs->MonitorModeBufferInputCount < ARRAYSIZE(IndirectDeviceContext::s_KnownMonitorModes))
    {
        // Return success if there was no buffer, since the caller was only asking for a count of modes
        // 如果没有缓冲区，则返回成功，因为调用方只要求计数模式
        return (pInArgs->MonitorModeBufferInputCount > 0) ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
    }
    else
    {
        // Copy the known modes to the output buffer
        // 将已知模式复制到输出缓冲区
        for (DWORD ModeIndex = 0; ModeIndex < ARRAYSIZE(IndirectDeviceContext::s_KnownMonitorModes); ModeIndex++)
        {
            pInArgs->pMonitorModes[ModeIndex].Size = sizeof(IDDCX_MONITOR_MODE);
            pInArgs->pMonitorModes[ModeIndex].Origin = IDDCX_MONITOR_MODE_ORIGIN_MONITORDESCRIPTOR;
            pInArgs->pMonitorModes[ModeIndex].MonitorVideoSignalInfo = IndirectDeviceContext::s_KnownMonitorModes[ModeIndex];
        }

        // Set the preferred mode as represented in the EDID
        // 设置EDID中表示的首选模式
        pOutArgs->PreferredMonitorModeIdx = 0;

        return STATUS_SUCCESS;
    }
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorGetDefaultModes(IDDCX_MONITOR MonitorObject, 
    const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* pInArgs, 
    IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* pOutArgs)
{
    LOG("IndirectDisplay, IddSampleMonitorGetDefaultModes\n");

    UNREFERENCED_PARAMETER(MonitorObject);
    UNREFERENCED_PARAMETER(pInArgs);
    UNREFERENCED_PARAMETER(pOutArgs);

    // Should never be called since we create a single monitor with a known EDID in this sample driver.
    // 永远不会调用它，因为我们在此示例驱动程序中创建了一个具有已知EDID的监视器。

    // ==============================
    // TODO: In a real driver, this function would be called to generate monitor modes for a monitor with no EDID.
    // Drivers should report modes that are guaranteed to be supported by the transport protocol and by nearly all
    // monitors (such 640x480, 800x600, or 1024x768). If the driver has access to monitor modes from a descriptor other
    // than an EDID, those modes would also be reported here.
    //
    // 待办事项：在实际的驱动程序中，将调用此函数为没有EDID的监视器生成监视器模式。 
    // 驱动程序应报告传输协议和几乎所有监视器都支持的模式（例如640x480、800x600或1024x768）。 
    // 如果驱动程序可以通过除EDID之外的描述符访问监视模式，则这些模式也将在此处报告。
    //
    // ==============================

    return STATUS_NOT_IMPLEMENTED;
}

/// <summary>
/// Creates a target mode from the fundamental mode attributes.
/// 从基本模式属性创建目标模式。
/// </summary>
void CreateTargetMode(DISPLAYCONFIG_VIDEO_SIGNAL_INFO& Mode, 
    UINT Width, 
    UINT Height, 
    UINT VSync)
{
    LOG("IndirectDisplay, CreateTargetMode\n");

    Mode.totalSize.cx = Mode.activeSize.cx = Width;
    Mode.totalSize.cy = Mode.activeSize.cy = Height;
    Mode.AdditionalSignalInfo.vSyncFreqDivider = 1;
    Mode.AdditionalSignalInfo.videoStandard = 255;
    Mode.vSyncFreq.Numerator = VSync;
    Mode.vSyncFreq.Denominator = Mode.hSyncFreq.Denominator = 1;
    Mode.hSyncFreq.Numerator = VSync * Height;
    Mode.scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE;
    Mode.pixelRate = VSync * Width * Height;
}

void CreateTargetMode(IDDCX_TARGET_MODE& Mode, UINT Width, UINT Height, UINT VSync)
{
    LOG("IndirectDisplay, CreateTargetMode\n");

    Mode.Size = sizeof(Mode);
    CreateTargetMode(Mode.TargetVideoSignalInfo.targetVideoSignalInfo, Width, Height, VSync);
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorQueryModes(IDDCX_MONITOR MonitorObject, 
    const IDARG_IN_QUERYTARGETMODES* pInArgs, 
    IDARG_OUT_QUERYTARGETMODES* pOutArgs)
{
    LOG("IndirectDisplay, IddSampleMonitorQueryModes\n");

    UNREFERENCED_PARAMETER(MonitorObject);

    vector<IDDCX_TARGET_MODE> TargetModes(4);

    // Create a set of modes supported for frame processing and scan-out. These are typically not based on the
    // monitor's descriptor and instead are based on the static processing capability of the device. The OS will
    // report the available set of modes for a given output as the intersection of monitor modes with target modes.
    // 创建一组支持帧处理和扫描的模式。 这些通常不是基于监视器的描述符，而是基于设备的静态处理能力。
    // 操作系统将报告给定输出的可用模式集，作为监控器模式与目标模式的交集。

    CreateTargetMode(TargetModes[0], 1920, 1080, 60);
    CreateTargetMode(TargetModes[1], 1024, 768, 60);
    CreateTargetMode(TargetModes[2], 800, 600, 60);
    CreateTargetMode(TargetModes[3], 640, 480, 60);

    pOutArgs->TargetModeBufferOutputCount = (UINT)TargetModes.size();

    if (pInArgs->TargetModeBufferInputCount >= TargetModes.size())
    {
        copy(TargetModes.begin(), TargetModes.end(), pInArgs->pTargetModes);
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorAssignSwapChain(IDDCX_MONITOR MonitorObject, 
    const IDARG_IN_SETSWAPCHAIN* pInArgs)
{
    LOG("IndirectDisplay, IddSampleMonitorAssignSwapChain\n");

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorObject);
    pContext->pContext->AssignSwapChain(pInArgs->hSwapChain, pInArgs->RenderAdapterLuid, pInArgs->hNextSurfaceAvailable);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorUnassignSwapChain(IDDCX_MONITOR MonitorObject)
{
    LOG("IndirectDisplay, IddSampleMonitorUnassignSwapChain\n");

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorObject);
    pContext->pContext->UnassignSwapChain();
    return STATUS_SUCCESS;
}

#pragma endregion

