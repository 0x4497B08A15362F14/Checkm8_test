#include "Dfu.h"
#include "Libusb.h"
#include <stdint.h>

namespace Dfu
{
	int SetStateWaitReset(LibUsb::UsbDesc* pDesc)
	{
		LibUsb::TransferDesc tDesc;
		//Zero Length DFU_DNLOAD Request 
		//The host sends a DFU_DNLOAD request with the wLength field cleared to 0 to the device to indicate 
		//that it has completed transferring the firmware image file.
		//This is the final payload packet of a download operation.
		return LibUsb::SendControlRequestWnoData(pDesc, 0x21, DFU_DOWNLOAD, 0, 0, 0, &tDesc)
			&& tDesc.m_result == LIBUSB_TRANSFER_COMPLETED
			&& tDesc.m_sz == 0
			//Device has received the final block of firmware from the host 
			//and is waiting for receipt of DFU_GETSTATUS to begin the
			//Manifestation phase; OR device has completed the
			//Manifestation phase and is waiting for receipt of DFU_GETSTATUS
			&& GetStatus(pDesc, DFU_STATUS_OK, DFU_STATE_MANIFEST_SYNC)
			//Device is in the Manifestation phase. 
			&& GetStatus(pDesc, DFU_STATUS_OK, DFU_STATE_MANIFEST)
			//Device has programmed its memories and is waiting for a USB reset or a power on reset.
			&& GetStatus(pDesc, DFU_STATUS_OK, DFU_STATE_MANIFEST_WAIT_RESET);
	}

	int GetStatus(LibUsb::UsbDesc* pDesc, uint8_t status, uint8_t state)
	{
		Dfu_Status dfuStatus;
		LibUsb::TransferDesc tDesc;

		return LibUsb::SendUsbControlTransfer(pDesc, 0xA1, DFU_GET_STATUS, 0, 0, reinterpret_cast<uint8_t*>(&dfuStatus), sizeof(dfuStatus), &tDesc)
			&& tDesc.m_result == LIBUSB_TRANSFER_COMPLETED
			&& tDesc.m_sz == sizeof(dfuStatus)
			&& dfuStatus.m_status == status
			&& dfuStatus.m_state == state;

	}
}