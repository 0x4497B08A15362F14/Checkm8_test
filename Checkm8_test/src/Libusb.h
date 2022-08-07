#pragma once

#include "3rdparty/libusb.h"
namespace LibUsb
{
// Struct declarations
//-----------------------------------------------------------------------------

	struct UsbDesc
	{
		uint16_t m_vendorId;
		uint16_t m_productId;
		int m_usbInterface;
		struct libusb_context* m_usbCtxt;
		struct libusb_device_handle* m_deviceHandle;

		UsbDesc();
	};
	/*Describes a usb transfer*/
	struct TransferDesc
	{
		int32_t  m_result;
		uint32_t m_sz;

		TransferDesc();
	};
	/*Universals device descriptor*/
	struct UsbDeviceDescriptor
	{
		uint8_t m_bLen; //length in bytes of this descriptor
		uint8_t m_bDescriptorType; //Specifies the descriptor type. Must be set to USB_DEVICE_DESCRIPTOR_TYPE.
		uint16_t m_bcdUSB; //Identifies the version of the USB specification that this descriptor structure complies with. This value is a binary-coded decimal number.
		uint8_t m_bDeviceClass; //Specifies the class code of the device as assigned by the USB specification group.
		uint8_t m_bDeviceSubClass; //Specifies the subclass code of the device as assigned by the USB specification group.
		uint8_t m_bDeviceProtocol; //Specifies the protocol code of the device as assigned by the USB specification group.
		uint8_t m_bMaxPacketSize0; //Specifies the maximum packet size, in bytes, for endpoint zero of the device. The value must be set to 8, 16, 32, or 64.
		uint16_t m_idVendor; //Specifies the vendor identifier for the device as assigned by the USB specification committee.
		uint16_t m_idProduct; //Specifies the product identifier. This value is assigned by the manufacturer and is device-specific.
		uint16_t m_bcdDevice; //Identifies the version of the device. This value is a binary-coded decimal number
		uint8_t m_iManufacturer; //Specifies a device-defined index of the string descriptor that provides a string containing the name of the manufacturer of this device.
		uint8_t m_iProduct; //Specifies a device-defined index of the string descriptor that provides a string that contains a description of the device.
		uint8_t m_iSerialNumber; //Specifies a device-defined index of the string descriptor that provides a string that contains a manufacturer-determined serial number for the device.
		uint8_t m_bNumConfigurations; //Specifies the total number of possible configurations for the device.
	};
	/*Each supported device has different offset etc..*/
	struct DeviceConfig
	{
		uint16_t	m_cpid;
		uint8_t		m_pad[6];
		size_t		m_cfgHole;
		size_t		m_cfgLargeLeak;
		size_t		m_cfgOverwritePad;
		uint64_t	m_patchAddr;
		uint64_t	m_memcpyAddr;
		uint64_t	m_aesCryptoCmd;
		uint64_t	m_bootTrampEnd;
		uint64_t	m_usbSerialNumber;
		uint64_t	m_dfuHandleRequest;
		uint64_t	m_usbCoreDoTransfer;
		uint64_t	m_insecureMemoryBase;
		uint64_t	m_handleInterfaceRequest;
		uint64_t	m_usbCreateStringDescriptor;
		uint64_t	m_usbSerialNumberStringDescriptor;
	};

//-----------------------------------------------------------------------------
// end: Struct declaration


// Extern gloabal vars declaration
//-----------------------------------------------------------------------------

	extern UsbDeviceDescriptor g_usbDeviceDescriptor;
	extern DeviceConfig g_devConfig;
	extern uint8_t g_usbTimeout;

//-----------------------------------------------------------------------------
// end: Extern gloabal vars declaration


// Function declarations
//-----------------------------------------------------------------------------

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
	bool SendUsbControlTransfer(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char* data, uint16_t wLength, TransferDesc* pTd);
	/*Checks if a device is still connected to the host*/
	bool IsDeviceStillConnected(UsbDesc* pDesc);
	/*Send a control request*/
	int SendControlRequest(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, void* pData, uint16_t wLength, TransferDesc* pTd);
	/*Send a control request in an async way*/
	int SendControlRequestAsync(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char* pData, uint16_t wLength, uint32_t abortTimeout, TransferDesc* pTd);
	/*Send a control request in an async way without data*/
	int SendControlRequestAsyncWnoData(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint32_t abortTimeout, TransferDesc* pTd);

	int SendControlRequestWnoData(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, TransferDesc* pTd);
	/*Callback for libusb_fill_control_transfer*/
	void UsbAsyncCallback(struct libusb_transfer* pTransfer);

//-----------------------------------------------------------------------------
// end: Function declarations

}