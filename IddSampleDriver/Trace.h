/*++

Module Name:

    Trace.h

Abstract:

    This module contains the local type definitions for the driver.
    该模块包含驱动程序的本地类型定义。

Environment:

    Windows User-Mode Driver Framework 2
    Windows用户模式驱动程序框架2

--*/

//
// Define the tracing flags.
// 定义跟踪标志。
//
// Tracing GUID - b254994f-46e6-4718-80a0-0a3aa50d6ce4
//

#define WPP_CONTROL_GUIDS                                              \
    WPP_DEFINE_CONTROL_GUID(                                           \
        MyDriver1TraceGuid, (b254994f,46e6,4718,80a0,0a3aa50d6ce4),    \
                                                                       \
        WPP_DEFINE_BIT(MYDRIVER_ALL_INFO)                              \
        WPP_DEFINE_BIT(TRACE_DRIVER)                                   \
        WPP_DEFINE_BIT(TRACE_DEVICE)                                   \
        WPP_DEFINE_BIT(TRACE_QUEUE)                                    \
        )                             

#define WPP_FLAG_LEVEL_LOGGER(flag, level)                             \
    WPP_LEVEL_LOGGER(flag)

#define WPP_FLAG_LEVEL_ENABLED(flag, level)                            \
    (WPP_LEVEL_ENABLED(flag) &&                                        \
     WPP_CONTROL(WPP_BIT_ ## flag).Level >= level)

#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags)                              \
           WPP_LEVEL_LOGGER(flags)
               
#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags)                            \
           (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= lvl)

//
// This comment block is scanned by the trace preprocessor to define our
// Trace function.
// 跟踪预处理器将扫描此注释块，以定义我们的跟踪功能。
//
// begin_wpp config
// FUNC Trace{FLAG=MYDRIVER_ALL_INFO}(LEVEL, MSG, ...);
// FUNC TraceEvents(LEVEL, FLAGS, MSG, ...);
// end_wpp

//
//
// Driver specific #defines
//
#if UMDF_VERSION_MAJOR == 2 && UMDF_VERSION_MINOR == 0
    // TODO: Update the name of the tracing provider
    #define MYDRIVER_TRACING_ID      L"Microsoft\\UMDF2.0\\IddSampleDriver V1.0"
#endif

