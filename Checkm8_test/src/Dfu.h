#pragma once

#include "Libusb.h"
#include <stdint.h>
namespace Dfu
{
// Macros & constants
//-----------------------------------------------------------------------------

//https://usb.org/sites/default/files/DFU_1.1.pdf
#define DFU_DETACH      0x00        //DFU Detach request 
#define DFU_DOWNLOAD    0x01        //DFU Download request 
#define DFU_UPLOAD      0x02        //DFU Upload request 
#define DFU_GET_STATUS  0x03        //DFU Get Status request
#define DFU_CLR_STATUS  0x04        //DFU Clear Status request 
#define DFU_GET_STATE   0x05        //DFU Get State request
#define DFU_ABORT       0x06        //DFU Abort request

#define DFU_STATUS_OK					0x0
#define DFU_STATE_MANIFEST_SYNC			0x6		// DFU manifest synchronization
#define DFU_STATE_MANIFEST				0x7		// DFU manifest mode 
#define DFU_STATE_MANIFEST_WAIT_RESET	0x8		// DFU manifest wait for CPU reset

//-----------------------------------------------------------------------------
// end: Macros & constant


// Struct declarations
//-----------------------------------------------------------------------------

	//The device responds to the DFU_GETSTATUS request with a payload packet 
	//containing the following data
	struct Dfu_Status
	{
		//An indication of the status resulting from the execution of the most recent request.
		uint8_t m_status;
		//Minimum time, in milliseconds, that the host should wait before sending a subsequent DFU_GETSTATUS request.
		uint8_t m_pollTimeout[0x3]; 
		//An indication of the state that the device is going to enter immediately following transmission of this
		//response. (By the time the host receives this information, this is the current state of the device.)
		uint8_t m_state;
		//Index of status description in string table.
		uint8_t m_stringIdx;
	};

//-----------------------------------------------------------------------------
// end: Struct declaration

// Function declarations
//-----------------------------------------------------------------------------

	int SetStateWaitReset(LibUsb::UsbDesc* pDesc);
	/*Check the dfu status*/
	int GetStatus(LibUsb::UsbDesc* pDesc, uint8_t status, uint8_t state);

//-----------------------------------------------------------------------------
// end: Function declarations

}