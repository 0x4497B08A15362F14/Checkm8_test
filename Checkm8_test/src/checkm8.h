#pragma once

#include "Libusb.h"

namespace Checkm8
{
// Function declarations
//-----------------------------------------------------------------------------

	void RunTest();

	int SprayStage(LibUsb::UsbDesc* pDesc);

	int Reset(LibUsb::UsbDesc* pDesc);

	int Stall(LibUsb::UsbDesc* pDesc);

	int NoLeak(LibUsb::UsbDesc* pDesc);

	int UsbRequestLeak(LibUsb::UsbDesc* pDesc);

	int UsbRequestNoLeak(LibUsb::UsbDesc* pDesc);

	int UsbRequestStall(LibUsb::UsbDesc* pDesc);

//-----------------------------------------------------------------------------
// end: Function declarations
}