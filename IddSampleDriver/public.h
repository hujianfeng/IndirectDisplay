/*++
Copyright (c) 1990-2000    Microsoft Corporation All Rights Reserved

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.
    ��ģ���������������û�Ӧ�ó�������ͨ��������

Environment:

    user and kernel
    �û����ں�

--*/

#pragma once

#include <Initguid.h>

#define WHILE(a) \
__pragma(warning(suppress:4127)) while(a)

//
// Define an Interface Guid so that app can find the device and talk to it.
// ����GUID���Ա�Ӧ�ó�������ҵ��豸����֮�Ի���
//

DEFINE_GUID (GUID_DEVINTERFACE_ID,
    0xcdc35b6e, 0xbe4, 0x4936, 0xbf, 0x5f, 0x55, 0x37, 0x38, 0xa, 0x7c, 0x1b);
// {CDC35B6E-0BE4-4936-BF5F-5537380A7C1B}
