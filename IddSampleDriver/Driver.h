#pragma once

#define NOMINMAX
#include <windows.h>
#include <bugcodes.h>
#include <wudfwdm.h>
#include <wdf.h>
#include <iddcx.h>

#include <dxgi1_5.h>
#include <d3d11_2.h>
#include <avrt.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "Trace.h"
#include "public.h"

namespace Microsoft
{
    namespace WRL
    {
        namespace Wrappers
        {
            // Adds a wrapper for thread handles to the existing set of WRL handle wrapper classes
            // 将线程句柄的包装器添加到现有的WRL句柄包装器类集中
            typedef HandleT<HandleTraits::HANDLENullTraits> Thread;
        }
    }
}

namespace Microsoft
{
    namespace IndirectDisp
    {
        /// <summary>
        /// Manages the creation and lifetime of a Direct3D render device.
        /// 管理Direct3D渲染设备的创建和生命周期。
        /// </summary>
        struct Direct3DDevice
        {
            Direct3DDevice(LUID adapterLuid);
            Direct3DDevice();
            HRESULT Init();

            LUID AdapterLuid;
            Microsoft::WRL::ComPtr<IDXGIFactory5> DxgiFactory;
            Microsoft::WRL::ComPtr<IDXGIAdapter1> Adapter;
            Microsoft::WRL::ComPtr<ID3D11Device> Device;
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
        };

        /// <summary>
        /// Manages a thread that consumes buffers from an indirect display swap-chain object.
        /// 管理使用来自间接显示交换链对象的缓冲区的线程。
        /// </summary>
        class SwapChainProcessor
        {
        public:
            SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain, std::shared_ptr<Direct3DDevice> Device, HANDLE NewFrameEvent);
            ~SwapChainProcessor();

        private:
            static DWORD CALLBACK RunThread(LPVOID argument);

            void Run();
            void RunCore();

        public:
            IDDCX_SWAPCHAIN m_hSwapChain;
            std::shared_ptr<Direct3DDevice> m_Device;
            HANDLE m_hAvailableBufferEvent;
            Microsoft::WRL::Wrappers::Thread m_hThread;
            Microsoft::WRL::Wrappers::Event m_hTerminateEvent;
        };

        /// <summary>
        /// Provides a sample implementation of an indirect display driver.
        /// 提供间接显示驱动程序的示例实现。
        /// </summary>
        class IndirectDeviceContext
        {
        public:
            IndirectDeviceContext(_In_ WDFDEVICE wdfDevice);
            virtual ~IndirectDeviceContext();

            void InitAdapter();
            void FinishInit();

            void AssignSwapChain(IDDCX_SWAPCHAIN swapChain, LUID renderAdapter, HANDLE newFrameEvent);
            void UnassignSwapChain();

        protected:

            WDFDEVICE m_WdfDevice;
            IDDCX_ADAPTER m_Adapter;
            IDDCX_MONITOR m_Monitor;

            std::unique_ptr<SwapChainProcessor> m_ProcessingThread;

        public:
            static const DISPLAYCONFIG_VIDEO_SIGNAL_INFO s_KnownMonitorModes[];
            static const BYTE s_KnownMonitorEdid[];
        };
    }
}