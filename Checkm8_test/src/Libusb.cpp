#include <stdio.h>
#include <stdint.h>
#include "Libusb.h"

#define APPLE_VID 0x05AC
#define DFU_PID 0x1227

static const char* pwnd_str = " PWND:[";

namespace LibUsb
{
	UsbDesc::UsbDesc()
	{
		m_vendorId = APPLE_VID;
		m_productId = DFU_PID;
		m_usbInterface = 0;
		m_usbCtxt = NULL;
		m_deviceHandle = NULL;
	};

	TransferDesc::TransferDesc()
	{
		m_result = 0;
		m_sz = 0;
	}

	UsbDeviceDescriptor g_usbDeviceDescriptor;
	DeviceConfig g_devConfig;
	uint8_t g_usbTimeout = 5; 

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
				if (handle != NULL)
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
				Sleep(g_usbTimeout);
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
				g_devConfig.m_cpid = 0x8960;
				g_devConfig.m_cfgHole = 0;	//unused for this version
				g_devConfig.m_cfgLargeLeak = 7936;
				g_devConfig.m_cfgOverwritePad = 0x5C0;
				g_devConfig.m_patchAddr = 0x100005CE0;
				g_devConfig.m_memcpyAddr = 0x10000ED50;
				g_devConfig.m_aesCryptoCmd = 0x10000B9A8;
				g_devConfig.m_bootTrampEnd = 0x1800E1000;
				g_devConfig.m_usbSerialNumber = 0x180086CDC;
				g_devConfig.m_dfuHandleRequest = 0x180086C70;
				g_devConfig.m_usbCoreDoTransfer = 0x10000CC78;
				g_devConfig.m_insecureMemoryBase = 0x180380000;
				g_devConfig.m_handleInterfaceRequest = 0x10000CFB4;
				g_devConfig.m_usbCreateStringDescriptor = 0x10000BFEC;
				g_devConfig.m_usbSerialNumberStringDescriptor = 0x180080562;
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
		if (SendUsbControlTransfer(pDesc, 0x80, 0x6, 1U << 8U, 0, reinterpret_cast<uint8_t*>(&g_usbDeviceDescriptor), sizeof(UsbDeviceDescriptor), &tDesc)
			&& tDesc.m_result == 0
			&& tDesc.m_sz == sizeof(UsbDeviceDescriptor)
			&& SendUsbControlTransfer(pDesc, 0x80, 0x6, (3U << 8U) | g_usbDeviceDescriptor.m_iSerialNumber, 0x409, buf, sizeof(buf), &tDesc)
			&& tDesc.m_sz == buf[0]
			&& (sz = buf[0] / 2) != 0
			&& ((str = reinterpret_cast<char*>(malloc(sz))) != NULL)
			)
		{
			for (i = 0; i < sz; ++i) {
				str[i] = (char)buf[2 * (i + 1)];
			}
			str[sz - 1] = '\0';
		}
		return str;
	}

	bool SendUsbControlTransfer(UsbDesc* pDesc, uint8_t reqType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char* data, uint16_t wLength, TransferDesc* pTd)
	{
		/*Perform a USB control transfer.*/
		/*returns on success, the number of bytes actually transferred*/
		int ret = libusb_control_transfer(pDesc->m_deviceHandle, reqType, bRequest, wValue, wIndex, data, wLength, g_usbTimeout);
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

	bool IsDeviceStillConnected(UsbDesc* pDesc)
	{
		struct libusb_device** deviceList;
		ssize_t count = libusb_get_device_list(pDesc->m_usbCtxt, &deviceList);
		bool bFound = false;
		for (int i = 0; i < count; i++)
		{
			struct libusb_device* pDevice = deviceList[i];
			struct libusb_device_descriptor desc;
			libusb_get_device_descriptor(pDevice, &desc);
			if (desc.idVendor == APPLE_VID && desc.idProduct == DFU_PID)
			{
				bFound = true;
				break;
			}
		}
		libusb_free_device_list(deviceList, 1);
		return bFound;
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
				libusb_fill_control_transfer(pTransfer, pDesc->m_deviceHandle, pBuf, UsbAsyncCallback, &done, g_usbTimeout);
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
		int ret = libusb_control_transfer(pDesc->m_deviceHandle, reqType, bRequest, wValue, wIndex, reinterpret_cast<uint8_t*>(pData), wLength, g_usbTimeout);
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