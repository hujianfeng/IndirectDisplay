/*

Copyright (c) Microsoft Corporation

Abstract:

    This module contains a sample implementation of an indirect display driver. See the included README.md file and the
    various TODO blocks throughout this file and all accompanying files for information on building a production driver.
    ��ģ����������ʾ���������ʾ��ʵ�֡�������渽��README.md�ļ��Լ����ļ��������渽�ļ��еĸ���TODO�飬�Ի�ȡ�йع������������������Ϣ��

    MSDN documentation on indirect displays can be found at https://msdn.microsoft.com/en-us/library/windows/hardware/mt761968(v=vs.85).aspx.
    ����������λ���ҵ��йؼ����ʾ��MSDN�ĵ���https://msdn.microsoft.com/en-us/library/windows/hardware/mt761968(v=vs.85).aspx.

Environment:

    User Mode, UMDF
    �û�ģʽ��UMDF

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
// �˺괴�����ڽ�IndirectDeviceContextWrapper��ΪWDF����������ķ��ʵķ���
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
// ����������ں���
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

    // ע���豸�ϵ��Ļص�����IddSampleDeviceAdd��
    WDF_DRIVER_CONFIG_INIT(
        &config,
        IddSampleDeviceAdd
    );

    // ���������������
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
    // ע���Դ�ص�-�ڴ�ʾ���У�����Ҫ������
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDeviceD0Entry = IddSampleDeviceD0Entry;
    WdfDeviceInitSetPnpPowerEventCallbacks(deviceInit, &pnpPowerCallbacks);

    IDD_CX_CLIENT_CONFIG iddConfig;
    IDD_CX_CLIENT_CONFIG_INIT(&iddConfig);

    // If the driver wishes to handle custom IoDeviceControl requests, it's necessary to use this callback since IddCx
    // redirects IoDeviceControl requests to an internal queue. This sample does not need this.
    // �����������ϣ�������Զ���IoDeviceControl���������ʹ�ô˻ص�����ΪIddCx��IoDeviceControl�����ض����ڲ����С� ��ʾ������Ҫ�ˡ�

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
        // WDF���󼴽�ɾ��ʱ�Զ����������
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Object);
        if (pContext)
        {
            pContext->Cleanup();
        }
    };

    WDFDEVICE device = nullptr;

    // ��������豸����
    LOG("IndirectDisplay, WdfDeviceCreate\n");
    status = WdfDeviceCreate(&deviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status))
    {
        LOG("IndirectDisplay, WdfDeviceCreate Error\n");
        return status;
    }

    //
    // Create a device interface so that application can find and talk to us.
    // ����һ���豸�ӿڣ��Ա�Ӧ�ó�������ҵ����ǲ������ǽ�̸��
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
    // ����һ���µ��豸�����Ķ��󲢽��丽�ӵ�WDF�豸����
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
    // WDF���ô˹���������ȫͨ��״̬�������豸��
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
    // ���Ի���DXGI�������������ϵͳ�ϳ����µ���Ⱦ������������Ҫ����һ���¹�����
    // �����Ҫ���棬��ÿ�μ��DxgiFactory->IsCurrent�����������IsCurrent�����´���������
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&DxgiFactory));
    if (FAILED(hr))
    {
        return hr;
    }

    // Find the specified render adapter
    // ����ָ������Ⱦ������
    hr = DxgiFactory->EnumAdapterByLuid(AdapterLuid, IID_PPV_ARGS(&Adapter));
    if (FAILED(hr))
    {
        return hr;
    }

    // Create a D3D device using the render adapter. BGRA support is required by the WHQL test suite.
    // ʹ����Ⱦ����������D3D�豸�� WHQL�����׼���ҪBGRA֧�֡�
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
        // �������D3D�豸ʧ�ܣ�����ȾGPU���ܶ�ʧ������ɲ�ж��GPU��������ϵͳ���ڹ���״̬��
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
    // �������������н����������̣߳������� this����Ϊ�̲߳�������
    m_hThread.Attach(CreateThread(nullptr, 0, RunThread, this, 0, nullptr));
}

SwapChainProcessor::~SwapChainProcessor()
{
    LOG("IndirectDisplay, SwapChainProcessor::~SwapChainProcessor\n");

    // Alert the swap-chain processing thread to terminate
    // ���ѽ����������߳���ֹ
    SetEvent(m_hTerminateEvent.Get());

    if (m_hThread.Get())
    {
        // Wait for the thread to terminate
        // �ȴ��߳���ֹ
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
    // Ϊ��������ܣ���ʹ�ö�ý������ȳ�����񣬸÷������ܵضԸ��߳̽������ȼ�����
    // ���ڸ�CPU��������������������
    DWORD AvTask = 0;
    HANDLE AvTaskHandle = AvSetMmThreadCharacteristics(L"Distribution", &AvTask);

    RunCore();

    // Always delete the swap-chain object when swap-chain processing loop terminates in order to kick the system to
    // provide a new swap-chain if necessary.
    // ����������ѭ����ֹʱ�������ɾ���������������ڱ�Ҫʱ�߳�ϵͳ���ṩ�µĽ�������
    WdfObjectDelete((WDFOBJECT)m_hSwapChain);
    m_hSwapChain = nullptr;

    AvRevertMmThreadCharacteristics(AvTaskHandle);
}

void SwapChainProcessor::RunCore()
{
    LOG("IndirectDisplay, SwapChainProcessor::RunCore\n");

    // Get the DXGI device interface
    // ��ȡDXGI�豸�ӿ�
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
    // ѭ����ȡ���ͷŻ�����
    for (;;)
    {
        ComPtr<IDXGIResource> acquiredBuffer;

        // Ask for the next buffer from the producer
        // ��������������һ��������
        IDARG_OUT_RELEASEANDACQUIREBUFFER buffer = {};
        hr = IddCxSwapChainReleaseAndAcquireBuffer(m_hSwapChain, &buffer);

        // AcquireBuffer immediately returns STATUS_PENDING if no buffer is yet available
        if (hr == E_PENDING)
        {
            // We must wait for a new buffer
            // �ȴ�һ���µĻ�����
            HANDLE waitHandles [] =
            {
                m_hAvailableBufferEvent,
                m_hTerminateEvent.Get()
            };
            DWORD WaitResult = WaitForMultipleObjects(ARRAYSIZE(waitHandles), waitHandles, FALSE, 16);
            if (WaitResult == WAIT_OBJECT_0 || WaitResult == WAIT_TIMEOUT)
            {
                // We have a new buffer, so try the AcquireBuffer again
                // ��һ���µĻ�������������ٴγ���AcquireBuffer
                continue;
            }
            else if (WaitResult == WAIT_OBJECT_0 + 1)
            {
                // We need to terminate
                // ��Ҫ��ֹ
                break;
            }
            else
            {
                // The wait was cancelled or something unexpected happened
                // �ȴ���ȡ���������������
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
            // ��������ڴ˴�����֡
            // ����IddCx����������������ؼ��Ĳ��֡� ��Ҫ���ǣ�������ɶԻ�ȡ�ı����������κβ����� 
            // �ò��������ǣ�
            //  * ��GPU���Ƶ���һ�������������Խ��к���������������ӳ�䵽CPU�ڴ����ʱ���棩
            //  * GPU�������
            //  * ��GPU VPBlt���ӵ���һ������
            //  * GPU�Զ��������ɫ���������
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
            // ��������첽����/���͹�����ɺ󣬱���֡ͳ����Ϣ
            // ����Ӧ�����й���֡��ʱ����Ϣ���������ʱ�䣬����ʱ��ȡ�
            // ==============================
            // IddCxSwapChainReportFrameStatistics(m_hSwapChain, ...);
        }
        else
        {
            // The swap-chain was likely abandoned (e.g. DXGI_ERROR_ACCESS_LOST), so exit the processing loop
            // �����������ѱ�����������DXGI_ERROR_ACCESS_LOST��������˳�����ѭ����
            break;
        }
    }
}

#pragma endregion

#pragma region IndirectDeviceContext

const UINT64 MHZ = 1000000;
const UINT64 KHZ = 1000;

// A list of modes exposed by the sample monitor EDID - FOR SAMPLE PURPOSES ONLY
// ���Ӽ�����EDID������ģʽ�б� - ������Ʒʹ��
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
// ����һ��ʾ��������EDID-������ʾ��
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
    // �����������Ŀ��Ӳ���������������Ϣ�� �ַ����Ͱ汾������ң�⣬��ĳЩ����¿��ܻ���ʾ���û���
    //
    // ��Ҳ��ȷ����̬ÿ�����������ܵĵط���
    //
    // ==============================

    IDDCX_ADAPTER_CAPS AdapterCaps = {};
    AdapterCaps.Size = sizeof(AdapterCaps);

    // Declare basic feature support for the adapter (required)
    // �������������Ļ������ܵ�֧�֣����裩
    AdapterCaps.MaxMonitorsSupported = 1;
    AdapterCaps.EndPointDiagnostics.Size = sizeof(AdapterCaps.EndPointDiagnostics);
    AdapterCaps.EndPointDiagnostics.GammaSupport = IDDCX_FEATURE_IMPLEMENTATION_NONE;
    AdapterCaps.EndPointDiagnostics.TransmissionType = IDDCX_TRANSMISSION_TYPE_WIRED_OTHER;

    // Declare your device strings for telemetry (required)
    // �����豸�ַ����Խ���ң�⣨���
    AdapterCaps.EndPointDiagnostics.pEndPointFriendlyName = L"IddSample Device";
    AdapterCaps.EndPointDiagnostics.pEndPointManufacturerName = L"Microsoft";
    AdapterCaps.EndPointDiagnostics.pEndPointModelName = L"IddSample Model";

    // Declare your hardware and firmware versions (required)
    // ��������Ӳ���͹̼��汾�����
    IDDCX_ENDPOINT_VERSION Version = {};
    Version.Size = sizeof(Version);
    Version.MajorVer = 1;
    AdapterCaps.EndPointDiagnostics.pFirmwareVersion = &Version;
    AdapterCaps.EndPointDiagnostics.pHardwareVersion = &Version;

    // Initialize a WDF context that can store a pointer to the device context object
    // ��ʼ�����Դ洢ָ���豸�����Ķ����ָ���WDF������
    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);

    IDARG_IN_ADAPTER_INIT AdapterInit = {};
    AdapterInit.WdfDevice = m_WdfDevice;
    AdapterInit.pCaps = &AdapterCaps;
    AdapterInit.ObjectAttributes = &Attr;

    // Start the initialization of the adapter, which will trigger the AdapterFinishInit callback later
    // ��ʼ�������ĳ�ʼ�����Ժ󽫴���AdapterFinishInit�ص�
    IDARG_OUT_ADAPTER_INIT AdapterInitOut;
    NTSTATUS Status = IddCxAdapterInitAsync(&AdapterInit, &AdapterInitOut);

    if (NT_SUCCESS(Status))
    {
        // Store a reference to the WDF adapter handle
        // �洢��WDF���������������
        m_Adapter = AdapterInitOut.AdapterObject;

        // Store the device context object into the WDF object context
        // ���豸�����Ķ���洢��WDF������������
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
    // ���������ʵ�����������У�Ӧ�ô����ӵ�����������ж�̬����EDID�� 
    // �˴��ṩ��EDID��������ʾ����Ϊ����������640x480 @ 60 Hz��800x600 @ 60 Hz�� 
    // ��������������Ҫ��ȷ��д������������ԣ����������ϵͳ�Ż�����鿴����ͱ�������֮������á�
    // �����̻�Ӧ��ÿ���豸��ʹ��Ψһ�����кţ���ȷ������ϵͳ����������ʾ����
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
    // �����������������������õظ��ӵ���ʾ�������豸�����ϣ��������������IDӦ�롰�ˡ��豸������ID��ͬ�� 
    // ͨ��������ID����ÿ������������Ψһ�ģ����ҿ����ڽ��������������豸��������Ƶ�������豸�������� 
    // �ڴ�ʾ���У�����������һ�����������ID GUID���������ʵ����ΪΨһ�ļ�����ѡ��һ���ȶ�������ID��
    // ����Ϊ����/���ɼ�����ʹ�á��ˡ��豸������ID��
    //
    // ==============================

    // Create a container ID
    // ����һ������ID
    CoCreateGuid(&MonitorInfo.MonitorContainerId);

    IDARG_IN_MONITORCREATE MonitorCreate = {};
    MonitorCreate.ObjectAttributes = &Attr;
    MonitorCreate.pMonitorInfo = &MonitorInfo;

    // Create a monitor object with the specified monitor descriptor
    // ʹ��ָ���ļ�������������������������
    IDARG_OUT_MONITORCREATE MonitorCreateOut;
    NTSTATUS Status = IddCxMonitorCreate(m_Adapter, &MonitorCreate, &MonitorCreateOut);
    if (NT_SUCCESS(Status))
    {
        m_Monitor = MonitorCreateOut.MonitorObject;

        // Associate the monitor with this device context
        // ������������豸�����Ĺ���
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorCreateOut.MonitorObject);
        pContext->pContext = this;

        // Tell the OS that the monitor has been plugged in
        // ���߲���ϵͳ��ʾ���Ѳ���
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
        // ���D3D��ʼ��ʧ�ܣ���ɾ������������һ�����Ҫ���Ա�OS֪�������µĽ����������ԡ�
        WdfObjectDelete(SwapChain);
    }
    else
    {
        // Create a new swap-chain processing thread
        // ����һ���µĽ����������߳�
        m_ProcessingThread.reset(new SwapChainProcessor(SwapChain, Device, NewFrameEvent));
    }
}

void IndirectDeviceContext::UnassignSwapChain()
{
    LOG("IndirectDisplay, IndirectDeviceContext::UnassignSwapChain\n");

    // Stop processing the last swap-chain
    // ֹͣ�������һ��������
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
    // ������ϵͳ��������������Թ�IddCx��������ʹ��ʱ�������ô˷����� ���ڿ��Ա��������ӵļ�������

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
    // ������������ѡ��ģʽʱ��ִ���κβ���-��������IddCx����

    // ==============================
    // TODO: In a real driver, this function would be used to reconfigure the device to commit the new modes. Loop
    // through pInArgs->pPaths and look for IDDCX_PATH_FLAGS_ACTIVE. Any path not active is inactive (e.g. the monitor
    // should be turned off).
    //
    // ���������ʵ�����������У��˹��ܽ��������������豸���ύ��ģʽ�� ����pInArgs->pPaths������IDDCX_PATH_FLAGS_ACTIVE�� 
    // �κ�δ�����·�������ڷǼ���״̬�����磬Ӧ�رռ���������
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
    // ���������ʵ�ʵ����������У������ô˺�����ͨ����������EDID�ļ���ģʽ�� 
    // �ڴ�ʾ�����������У����Ƕ�EDID������Ӳ���룬��˴˺�������������֪ģʽ��
    //
    // ==============================

    pOutArgs->MonitorModeBufferOutputCount = ARRAYSIZE(IndirectDeviceContext::s_KnownMonitorModes);

    if (pInArgs->MonitorModeBufferInputCount < ARRAYSIZE(IndirectDeviceContext::s_KnownMonitorModes))
    {
        // Return success if there was no buffer, since the caller was only asking for a count of modes
        // ���û�л��������򷵻سɹ�����Ϊ���÷�ֻҪ�����ģʽ
        return (pInArgs->MonitorModeBufferInputCount > 0) ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
    }
    else
    {
        // Copy the known modes to the output buffer
        // ����֪ģʽ���Ƶ����������
        for (DWORD ModeIndex = 0; ModeIndex < ARRAYSIZE(IndirectDeviceContext::s_KnownMonitorModes); ModeIndex++)
        {
            pInArgs->pMonitorModes[ModeIndex].Size = sizeof(IDDCX_MONITOR_MODE);
            pInArgs->pMonitorModes[ModeIndex].Origin = IDDCX_MONITOR_MODE_ORIGIN_MONITORDESCRIPTOR;
            pInArgs->pMonitorModes[ModeIndex].MonitorVideoSignalInfo = IndirectDeviceContext::s_KnownMonitorModes[ModeIndex];
        }

        // Set the preferred mode as represented in the EDID
        // ����EDID�б�ʾ����ѡģʽ
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
    // ��Զ�������������Ϊ�����ڴ�ʾ�����������д�����һ��������֪EDID�ļ�������

    // ==============================
    // TODO: In a real driver, this function would be called to generate monitor modes for a monitor with no EDID.
    // Drivers should report modes that are guaranteed to be supported by the transport protocol and by nearly all
    // monitors (such 640x480, 800x600, or 1024x768). If the driver has access to monitor modes from a descriptor other
    // than an EDID, those modes would also be reported here.
    //
    // ���������ʵ�ʵ����������У������ô˺���Ϊû��EDID�ļ��������ɼ�����ģʽ�� 
    // ��������Ӧ���洫��Э��ͼ������м�������֧�ֵ�ģʽ������640x480��800x600��1024x768���� 
    // ��������������ͨ����EDID֮������������ʼ���ģʽ������ЩģʽҲ���ڴ˴����档
    //
    // ==============================

    return STATUS_NOT_IMPLEMENTED;
}

/// <summary>
/// Creates a target mode from the fundamental mode attributes.
/// �ӻ���ģʽ���Դ���Ŀ��ģʽ��
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
    // ����һ��֧��֡�����ɨ���ģʽ�� ��Щͨ�����ǻ��ڼ������������������ǻ����豸�ľ�̬����������
    // ����ϵͳ�������������Ŀ���ģʽ������Ϊ�����ģʽ��Ŀ��ģʽ�Ľ�����

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

