#pragma once
#include "3rdparty/libusb.h"

namespace LibUsb
{
	struct UsbDesc;
	/*Describes a usb transfer*/
	struct TransferDesc;
	/*Universals */
	struct UsbDeviceDescriptor;
	/*Init LibUsb*/
	int Init(UsbDesc* pDesc);
	/*Shutdown LibUsb*/
	void Shutdown(UsbDesc* pDesc);
	/*Wait for a specific usb*/
	int WaitForUsb(UsbDesc* pDesc, int usbInterface0, int usbInterface1, bool& hasAlreadyBeenPwn3d, bool& bAlreadyBeenFound);
	/**/
	int CheckUsbDevice(UsbDesc* pDesc, bool* res);
	/*returns a ptr to the converted string*/
	char* GetUsbSerialNumber(UsbDesc* pDesc);
	/*performs a usb transfer with the specified reqTypes etc*/
	bool PerformUsbControlTransfer(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char* data, uint16_t wLength, TransferDesc* pTd);
	/**/
	bool IsDeviceStillConnected(UsbDesc* pDesc);
	/*Send a control request in an async way*/
	int SendControlRequestAsync(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char* pData, uint16_t wLength, uint32_t abortTimeout, TransferDesc* pTd);
	/*Callback for libusb_fill_control_transfer*/
	void UsbAsyncCallback(struct libusb_transfer* pTransfer);
	/*Send a control request in an async way without data*/
	int SendControlRequestAsyncWnoData(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint32_t abortTimeout, TransferDesc* pTd);

	int SendControlRequestWnoData(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, TransferDesc* pTd);

	void RunTest();

	int SprayStage(UsbDesc* pDesc);
}

namespace Checkm8
{
	int Stall(LibUsb::UsbDesc* pDesc);

	int NoLeak(LibUsb::UsbDesc* pDesc);

	int UsbRequestLeak(LibUsb::UsbDesc* pDesc);

	int UsbRequestNoLeak(LibUsb::UsbDesc* pDesc);

	int UsbRequestStall(LibUsb::UsbDesc* pDesc);
}