//////////
//
// guithread.cpp
//
//////
//
//



#include "stdafx.h"




//////////
//
// Called to create an interface as either master or slave to/for the remote process.
// Returns hidden message hwnd used by the DLL
//
//////
	u32 guithread_create_interface(u32 tnFormHwnd, u8* tcPipeName, u32 tlIsMaster)
	{
		u32				lnResult;
		SInterface*		li;


		// Make sure our environment is sane
		lnResult = -1;

		// Create a new process
		li = iigt_createNewSInterface(&gsInterfaces);
		if (li)
		{
			// Store the hwnds
			li->hwndBound	= tnFormHwnd;

			// Store information for creating the pipe
			iigt_createSPipe(&li->pipe);
			if (li->pipe)
			{
				iigt_copyString(&li->pipe->name, &li->pipe->nameLength, tcPipeName, strlen((s8*)tcPipeName), true);
				li->pipe->isOwner = ((!tlIsMaster) ? false : true);
			}

			// Return the interface id
			lnResult = li->interfaceId;
		}

		// Indicate our status
		return(lnResult);
	}




//////////
//
// Launches a remote process using the indicated command line
//
//////
	u32 guithread_launch_remote_using_interface(u32 tnInterfaceId, u8* tcCommandLine)
	{
		u32			lnResult;
		SInterface*	li;


		// Make sure our environment is sane
		lnResult = -1;

		// Create a new process
		li = iigt_FindSInterface(&gsInterfaces, tnInterfaceId);
		if (li)
		{
			// Store the parameters
			iigt_copyString(&li->commandLine,	NULL, tcCommandLine,	strlen((s8*)tcCommandLine),		true);

			// Create the worker thread
			li->threadHandle = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&igt_launchUsingInterfaceWorkerThread, li, NULL, &li->threadId);

			// Return the interfaceId
			lnResult = li->interfaceId;
		}

		// Indicate our status
		return(lnResult);
	}




//////////
//
// Called to delete a previously created interface.
// Used to shut down the interface if required (should be called when a process is terminated)
//
//////
	u32 guithread_delete_interface(u32 tnInterfaceId)
	{
		SInterface*	li;


		// Create a new process
		if (tnInterfaceId)
		{
			li = iigt_FindSInterface(&gsInterfaces, tnInterfaceId);
			if (li)
			{
				//////////
				// They want to make this a non-viable interface
				//////
					li->interfaceId = 0;
					li->isRunning	= false;


				//////////
				// Delete the message window
				//////
					if (li->hwndLocalMessage)
					{
						DestroyWindow((HWND)li->hwndLocalMessage);
						li->hwndLocalMessage = NULL;
					}


				//////////
				// Delete the pipe
				//////
					if (li->pipe->isOwner)
					{
						// No longer a valid pipe
						CloseHandle((HANDLE)li->pipe->handleRead);
						CloseHandle((HANDLE)li->pipe->handleWrite);
					}


				//////////
				// Indicate success
				//////
					return(0);
					// Note:  We leave everything else in memory (unreceived mail, allocated strings, etc).
					// Note:  It will be automatically cleaned up when the process terminates.
			} 
		}
		// Failure
		return(-1);
	}




//////////
//
// Called to retrieve a message that's been notified is ready
// Called twice, once to find out how long the message is, the second time to actually retrieve the message
//
//////
	u32 guithread_get_message(u32 tnInterfaceId, u32 tnIdentifier, u8* tcMessage, u32 tnMessageLength)
	{
		u32				lnLength;
		SParcel*		mail;
		SInterface*		li;


		// Find our interface
		lnLength = -1;
		li = iigt_FindSInterface(&gsInterfaces, tnInterfaceId);
		if (li)
		{
			// Grab our mail
			mail = iigt_findMailInInterface(li, tnIdentifier);
			if (mail && mail->data)
			{
				// Copy it
				if (tcMessage && tnMessageLength != 0)
					iigt_copyToShortestStringLength(tcMessage, tnMessageLength, mail->data, mail->dataLength, false, false, 0);

				// Grab the length for return
				lnLength = mail->dataLength;

				// Delete it if we copied it
				if (tnMessageLength >= mail->dataLength)
					iigt_deleteMailParcel(li, tnIdentifier);
			}
		}
		// Indicate the length of the string
		return(lnLength);
	}




//////////
//
// Called to send a message.
// If cMessage is NULL, then a simple message is sent with only nValue and nExtra being used (nIdentifier is ignored, as is nMessageLength)
//
//////
	u32 guithread_send_message(u32 tnInterfaceId, u32 tnIdentifier, u8* tcMessage, u32 tnMessageLength)
	{
		// Indicate failure
		return(-4);
	}




//////////
//
// Called to change the style of a window, to set it as either a tool window or not.
// Used to show or hide a window's placement on the taskbar (useful for creating background windows
// that do not "consume" taskbar space, but are used for conveying messages).
//
// Returns:
//		-1		-- Failure (not a valid hwnd)
//		others	-- The current extended style in use
//////
	u32 guithread_hwnd_on_taskbar(u32 tnHwnd, u32 tnShow)
	{
		u32 lnStyle;


		// Is it a valid window?
		if (IsWindow((HWND)tnHwnd))
		{
			//////////
			// Get the extended style
			//////
				lnStyle = GetWindowLong((HWND)tnHwnd, GWL_EXSTYLE);


			//////////
			// Update the tool window setting appropriately
			//////
				if (!tnShow)
				{
					// They DO NOT want it to show on the taskbar
					lnStyle = lnStyle | WS_EX_TOOLWINDOW;		// Turn on the tool window bit

				} else {
					// They DO want to see it on the taskbar
					lnStyle = lnStyle & ~WS_EX_TOOLWINDOW;
				}


			//////////
			// Update the style
			//////
				ShowWindow((HWND)tnHwnd, SW_HIDE);
				SetWindowLong((HWND)tnHwnd, GWL_EXSTYLE, lnStyle);
				ShowWindow((HWND)tnHwnd, SW_SHOW);


			//////////
			// Indicate success
			//////
				return(lnStyle);

		} else {
			// Indicate failure
			return(-1);
		}
	}




//////////
//
// Worker thread to conduct the launch-remote-process work
//
//////
	DWORD WINAPI igt_launchUsingInterfaceWorkerThread(LPVOID lpThreadParameter)
	{
		SInterface*		li;
		MSG				msg;


		// Grab our interface
		li = (SInterface*)lpThreadParameter;
		if (!li)
			return(-1);		// Hmmm...


		//////////
		// Create window
		//////
			li->hwndLocalMessage = iigt_createMessageWindow(li);
			if (li->hwndLocalMessage == 0)
			{
				// Failure creating the message window
				CloseHandle(li->threadHandle);
				ExitThread(-1);
			}
			// We're good


		//////////
		// Attempt to connect to the pipe
		//////
			// We've now received the pipe name
			// Create the pipe name
			li->pipe = iigt_connectPipe(li->pipe->name, li->pipe->isOwner);
			if (!li->pipe || li->pipe->handleRead == (int)INVALID_HANDLE_VALUE)
			{
				// Failure connecting to a necessary component, the named pipe
				CloseHandle(li->threadHandle);
				ExitThread(-1);
			}
			// We're good


		//////////
		// Launch process
		//////
			li->procId = iigt_launchRemoteProcess(li);
			if (li->procId < 0)
			{
				// Failure creating a necessary component, the named pipe
				CloseHandle(li->threadHandle);
				ExitThread(-1);
			}


		//////////
		// Read messages until we are told to shut down
		//////
			while (li->isRunning && GetMessage(&msg, NULL, 0, 0))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				Sleep(1);
			}
			CloseHandle(li->threadHandle);
			ExitThread(0);
	}




//////////
//
// Called to find the existing interface
//
//////
	SInterface* iigt_FindSInterface(SInterface** root, u32 tnInterfaceId)
	{
		SInterface*		li;


		// Iterate through every item to find the matching one
		li = *root;
		while (li)
		{
			// Is this it?
			if (li->interfaceId == tnInterfaceId)
				return(li);	// Yes, we found it

			// Move to next item
			li = li->next;
		}
		// If we get here, not found
		return(NULL);
	}




//////////
//
// Called to find the existing interface
//
//////
	SInterface* iigt_FindSInterfaceByRemoteMessageHwnd(SInterface** root, u32 tnRemoteMessageHwnd)
	{
		SInterface*		li;


		// Iterate through every item to find the matching one
		li = *root;
		while (li)
		{
			// Is this it?
			if (li->hwndRemoteMessage == tnRemoteMessageHwnd)
				return(li);	// Yes, we found it

			// Move to next item
			li = li->next;
		}
		// If we get here, not found
		return(NULL);
	}




//////////
//
// Guarantees sequential, unique access to ids
//
//////
	u32 iigt_getNextUniqueId(void)
	{
		u32 lnUniqueId;


		// Lock it
		EnterCriticalSection(&gsemUniqueIdAccess);

		// Grab it
		lnUniqueId = gnNextUniqueId++;

		// Unlock it
		LeaveCriticalSection(&gsemUniqueIdAccess);

		// Return it
		return(lnUniqueId);
	}




//////////
//
// Copy the string over (if null-terminated, returns the length without the null termination
// character, even though it exists as well.
//
//////
	void iigt_copyString(u8** tcDestination, u32* tnDestinationLength, u8* tcSource, u32 tnSourceLength, bool tlNullTerminate)
	{
		u32 lnLength;


		// Create a copy of the memory area
		lnLength		= tnSourceLength + ((tlNullTerminate) ? 1 : 0);
		*tcDestination	= (u8*)malloc(lnLength);
		if (*tcDestination)
		{
			// Copy over, set the length
			if (tlNullTerminate)
				(*tcDestination)[tnSourceLength] = 0;				// NULL terminate the string

			// Copy the string portion
			memcpy(*tcDestination, tcSource, tnSourceLength);

			// Store the destination length if they want it
			if (tnDestinationLength)
				*tnDestinationLength = tnSourceLength;

		} else {
			// Unable to copy
			if (tnDestinationLength)
				*tnDestinationLength = 0;		// Indicate our failure
		}
	}




//////////
//
// Called to create the message window
//
//////
	u32 iigt_createMessageWindow(SInterface* ti)
	{
		WNDCLASSEXA wcex;


		// If we haven't yet registered the class, attempt to register it
		if (gnAtom == NULL)
		{
			// First see if the class is already registered
			if (!GetClassInfoExA(GetModuleHandle(NULL), (LPCSTR)cgcGuiThreadMessageWindowClassName, &wcex))
			{
				// If we get here, not yet registered
				memset(&wcex, 0, sizeof(wcex));
				wcex.cbSize         = sizeof(wcex);
				wcex.style          = CS_NOCLOSE;
				wcex.lpfnWndProc    = (WNDPROC)igt_interfaceWndProc;
				wcex.hInstance      = GetModuleHandle(NULL);
				wcex.lpszClassName  = (LPCSTR)cgcGuiThreadMessageWindowClassName;
				gnAtom				= RegisterClassExA(&wcex);

				// Was it registered?
				if (gnAtom == NULL)
					return(NULL);		// Nope ... when we get here, failure
			}
			// When we get here, we're good
		}

		// Register the class if not registered
		return((u32)CreateWindowA((LPCSTR)cgcGuiThreadMessageWindowClassName, NULL, WS_POPUP, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), ti));
	}




//////////
//
// Creates or connects to an existing pipe by name
//
//////
	SPipe* iigt_connectPipe(u8* tcPipeName, bool tlIsPipeOwner)
	{
		SPipe*	pipe;
		u8		pipeName[_MAX_PATH];


		// Make sure our environment is sane
		pipe = NULL;
		if (tcPipeName)
		{
			// Create the pipe
			pipe = iigt_createNewPipeStructure(tcPipeName);
			if (pipe)
			{
				// Get the fully qualified pipe name for the local machine
				sprintf_s((s8*)pipeName, sizeof(pipeName), "\\\\.\\pipe\\\0");
				iigt_copyToShortestStringLength(pipeName + iigt_strlen(pipeName), sizeof(pipeName) - iigt_strlen(pipeName), tcPipeName, iigt_strlen(tcPipeName), true, false, 0);

				// Connect to or create the pipe
				if (tlIsPipeOwner)
				{
					// Pipe owners create their pipe
					pipe->handleRead = (u32)CreateNamedPipeA((s8*)pipeName, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 2, _PIPE_OUT_BUFFER_SIZE, _PIPE_IN_BUFFER_SIZE, 0, NULL);

				} else {
					// Non-owners connect to an existing pipe
					pipe->handleRead = (u32)CreateFileA((s8*)pipeName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				}

				// Based on our open operation, we were successful?
				if ((HANDLE)pipe->handleRead == INVALID_HANDLE_VALUE)
				{
					// An error creating the pipe
					pipe->error			= GetLastError();
					pipe->handleRead	= NULL;
				}
				pipe->handleWrite = pipe->handleRead;
			}
		}
		// Indicate our status
		return(pipe);
	}





//////////
//
// Writes data to the mail pipe
//
//////
	u32 iigt_writeToPipe(SInterface* ti, u8* tcData, u32 tnDataLength)
	{
		u32 lnNumWritten;


		// Make sure our environment is sane
		lnNumWritten = 0;
		if (ti->pipe && tnDataLength != 0)
			WriteFile((HANDLE)ti->pipe->handleWrite, tcData, tnDataLength, &lnNumWritten, NULL);

		// Indicate how many bytes were written
		return(lnNumWritten);
	}




//////////
//
// Reads data from the mail pipe, returns that data as a pointer
//
//////
	u8* iigt_readFromPipe(SInterface* ti, u32 tnReadSize, u32* tnActuallyRead)
	{
		u32		lnNumRead;
		u8*		lcData;


		// Make sure our environment is sane
		lcData = NULL;
		if (ti->pipe->handleRead && tnReadSize != 0)
		{
			// Allocate that much space
			lcData = (u8*)malloc(tnReadSize);
			if (lcData)
			{
				// Initialize our buffer to indicate failure should we fail
				memset(lcData, 0, tnReadSize);

				// Attempt to read that many bytes
				// Note:  If there isn't this much data already in the pipe, it will block the thread
				if (tnActuallyRead)		ReadFile((HANDLE)ti->pipe->handleRead, lcData, tnReadSize, tnActuallyRead,	NULL);
				else					ReadFile((HANDLE)ti->pipe->handleRead, lcData, tnReadSize, &lnNumRead,		NULL);
			}
		}
		// Return the pointer to our data
		return(lcData);
	}




//////////
//
// Create a pipe structure
//
//////
	SPipe* iigt_createNewPipeStructure(u8* tcPipeName)
	{
		SPipe*	pipe;


		// Make sure they specified a pipe name
		pipe = NULL;
		if (tcPipeName)
		{
			// Create the process
			pipe = (SPipe*)malloc(sizeof(SPipe));
			if (pipe)
			{
				// Initialize the buffer
				memset(pipe, 0, sizeof(SPipe));

				// Copy the name
				iigt_copyString(&pipe->name, &pipe->nameLength, tcPipeName, iigt_strlen(tcPipeName), true);
			}
		}
		// Indicate success or failure
		return(pipe);
	}




//////////
//
// Called to get the length of an unsigned character string
//
//////
	int iigt_strlen(u8* tcData)
	{
		u32 lnI;

		// Count every character to the next null
		for (lnI = 0; ; lnI++)
		{
			if (tcData[lnI] == 0)
				return(lnI);
		}
	}




//////////
//
// Copies the source (src) to the destination (dst) until we reach the maximum length of
// either the source or destination string lengths
//
//////
	void iigt_copyToShortestStringLength(u8* tcDestination, u32 tnDestinationLength, u8* tcSource, u32 tnSourceLength, bool tlNullTerminate, bool tlPad, u8 tcPadChar)
	{
		u32 lnI, lnJ;


		// Make sure we have something to do
		if (tnDestinationLength != 0 && tnSourceLength != 0)
		{
			// Copy over every byte
			for (lnI = 0; lnI < tnDestinationLength && lnI < tnSourceLength; lnI++)
				tcDestination[lnI] = tcSource[lnI];

			// Pad if we need to
			if (tlPad)
			{
				for (lnJ = lnI; lnJ < tnDestinationLength; lnJ++)
					tcDestination[lnJ] = tcPadChar;
			}

			// NULL-terminate if we are supposed to
			if (tlNullTerminate)
				tcDestination[min(lnI, tnDestinationLength -1)] = 0;	// NULL-terminate this item
		}
	}




//////////
//
// Receives the text form of a message that is a general message to be conveyed unto the
// bound machine.
//
//////
	void iigt_receiveAndProcessMessage(SInterface* ti, u8* tcGeneralMessage, u32 tnGeneralMessageLength, u32 tnHwnd)
	{
		u32					lnMailId;
		SParcel*			mail;
		SParcelDelivery*	lpd;


		// Make sure our environment is sane
		lnMailId = -1;
		if (tnGeneralMessageLength >= sizeof(SParcelDelivery))
		{
			// See if it has the text
			lpd = (SParcelDelivery*)tcGeneralMessage;
			lpd->messageType	= tcGeneralMessage + lpd->totalMessageLength - lpd->contentLength - lpd->messageTypeLength;
			lpd->content		= tcGeneralMessage + lpd->totalMessageLength - lpd->contentLength;


			//////////
			// Validate it's a valid message
			//////
				if (iigt_validateParcelDeliverySha1s(lpd))
				{
					// See if the message is an up-and-running, if so they have included their remote hwnd which we need
					if (iigt_equalEqual(lpd->messageType, lpd->messageTypeLength, (u8*)cgcUpAndRunning, sizeof(cgcUpAndRunning) - 1))
					{
						// When they notify they're up and running, we have some additional information we need to update
						ti->hwndRemoteMessage	= lpd->nExtra;		// They are reporting being up and running, store the hwnd
						ti->isRunning			= true;
					}

					// Store the message locally
					lnMailId = iigt_stowMail(gsInterfaceSelf, (u8*)lpd, lpd->totalMessageLength, &mail);

					// If it was stored properly, post it, if not then delete it
					if (lnMailId != -1)
						PostMessage((HWND)ti->hwndBound, WMGT_PARCEL_DELIVERY, ti->interfaceId, lnMailId);
				}


			//////////
			// When we get here, the message was either stored or not (we'll know by the lnMailId)
			//////
				if (lnMailId == -1)
				{
					// Store the message locally
					lnMailId = iigt_stowMail(gsInterfaceSelf, (u8*)lpd, lpd->totalMessageLength, &mail);

					// If it was stored properly, post it, if not then delete it
					if (lnMailId != -1)
						PostMessage((HWND)ti->hwndBound, WMGT_PARCEL_DELIVERY_FAILURE, ti->interfaceId, lnMailId);
				}
		}
	}




//////////
//
// Stow the message in the mailbag for retrieval by the remote process
//
//////
	u32 iigt_stowMail(SInterface* ti, u8* tcText, u32 tnTextLength, SParcel** tsMail)
	{
		SParcel* mail;


		// Make sure we have something to do
		if (tcText)
		{
			// Store the mail
			mail = iigt_appendMail(&ti->mailbag, tcText, tnTextLength, true);
			if (mail)
			{
				// If they want the mail parcel also, give it to them
				if (tsMail)
					*tsMail = mail;

				// Return the mail id
				return(mail->uniqueId);
			}
		}
		// Indicate failure
		return(-1);
	}




//////////
//
// Append the indicated message to the mailbag, and return the unique id for this mail
//
//////
	SParcel* iigt_appendMail(SParcel** mailbagRoot, u8* tcText, u32 tnTextLength, bool tlCopyString)
	{
		SParcel*	mailNew;
		SParcel*	mail;
		SParcel**	mailPrev;


		// Make sure we have a proper pointer
		if (mailbagRoot && tcText && tnTextLength != 0)
		{
			if (!*mailbagRoot)
			{
				// This is the first mail item
				mailPrev = mailbagRoot;

			} else {
				// append to the end of the chain
				mail = *mailbagRoot;
				while (mail->next)
					mail = mail->next;
				mailPrev = &mail->next;
			}

			// Create the new mailbag entry
			mailNew = (SParcel*)malloc(sizeof(SParcel));
			if (mailNew)
			{
				// Initialize the entry
				memset(mailNew, 0, sizeof(SParcel));

				// Update the back-link
				*mailPrev = mailNew;

				// Store our settings
				if (tlCopyString)
				{
					// Spin off a copy
					iigt_copyString(&mailNew->data, &mailNew->dataLength, tcText, tnTextLength, false);

				} else {
					// Just store it
					mailNew->data			= tcText;
					mailNew->dataLength		= tnTextLength;
				}

				// Store the unique id, encryption
				mailNew->uniqueId	= iigt_getNextUniqueId();

				// All done
				return(mailNew);
			}
		}
		// Indicate failure
		return(NULL);
	}




//////////
//
// Called to search for the indicated mail parcel.
// Note:  We only search our own mailbag
//
//////
	SParcel* iigt_findMail(u32 tnMailId)
	{
		return(iigt_findMailInInterface(gsInterfaceSelf, tnMailId));
	}




//////////
//
// Called to search for the indicated mail parcel.
// Note:  We only search the indicated mailbag
//
//////
	SParcel* iigt_findMailInInterface(SInterface* ti, u32 tnMailId)
	{
		SParcel* mail;


		if (ti)
		{
			// Iterate through all parcels
			mail = ti->mailbag;
			while (mail)
			{
				// See if this is our man
				if (mail->uniqueId == tnMailId)
					break;		// We found it

				// Move to next parcel
				mail = mail->next;
			}
			// Indicate our status
			return(mail);
		}
		// Not a valid process
		return(NULL);
	}




//////////
//
// Called to delete the indicated mail parcel from the interface's mailbag chain
//
//////
	void iigt_deleteMailParcel(SInterface* ti, u32 tnMailId)
	{
	}




//////////
//
// Append a new interface to the chain
//
//////
	SInterface* iigt_createNewSInterface(SInterface** root)
	{
		SInterface*		liNew;
		SInterface*		li;
		SInterface**	liLast;


		// Make sure we have a proper pointer
		liNew = NULL;
		if (root)
		{
			if (!*root)
			{
				// This is the first one
				liLast = root;

			} else {
				// We need to add to the chain
				li = *root;
				while (li->next)
					li = li->next;
				liLast = &li->next;
			}

			// Create the new item
			liNew = (SInterface*)malloc(sizeof(SInterface));
			if (liNew)
			{
				// Initialize the new entry
				memset(liNew, 0, sizeof(SInterface));

				// Initialize the semaphore
				InitializeCriticalSection(&liNew->cs_mail);
				InitializeCriticalSection(&liNew->cs_pipe);
				InitializeCriticalSection(&liNew->cs_generalMessage);

				// Create the process id
				liNew->interfaceId = iigt_getNextUniqueId();

				// Update the forward link
				*liLast = liNew;

				// All done!
			}
		}
		return(liNew);
	}




//////////
//
// Creates a new pipe structure, but does not connect to the pipe
//
//////
	void iigt_createSPipe(SPipe** pipe)
	{
		*pipe = (SPipe*)malloc(sizeof(SPipe));
		if (*pipe)
			memset(*pipe, 0, sizeof(SPipe));
	}




//////////
//
// Called to launch the indicated remote process (ti->params has all the return variables)
//
//////
	u32 iigt_launchRemoteProcess(SInterface* ti)
	{
		//////////
		// Initialize memory blocks
		//////
			memset(&ti->params.si, 0, sizeof(STARTUPINFO));
			memset(&ti->params.pi, 0, sizeof(PROCESS_INFORMATION));


		//////////
		// Create a process
		//////
			// Initialize our startup info, specifically we want the window to be hidden
			ti->params.si.cb			= sizeof(STARTUPINFO);
			ti->params.si.dwFlags		= STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
			ti->params.si.wShowWindow	= SW_NORMAL;

			// Run it
			if(!CreateProcessA(0, (s8*)ti->commandLine, NULL, NULL, true, 0, NULL, NULL, &ti->params.si, &ti->params.pi))
				return -1;			// Failure launching the process to run


		//////////
		// When we get here, the process is running
		//////
			return((u32)ti->params.pi.dwProcessId);
	}




//////////
//
// Called to validate a parcel delivery to make sure it is properly validated
//
//////
	bool iigt_validateParcelDeliverySha1s(SParcelDelivery* tsPd)
	{
		// Check some header info
		if (!iigt_validateParcelDeliverylSha1_32_lftve_mt_c(tsPd))
			return(false);		// Failure 

		// Check messageType text
		if (!iigt_validateParcelDeliverySha1_32_messageType(tsPd))
			return(false);		// Failure 

		// Check content body
		if (!iigt_validateParcelDeliverySha1_32_content(tsPd))
			return(false);		// Failure 

		// Check in its entirety
		if (!iigt_validateParcelDeliverySha1_32_all(tsPd))
			return(false);		// Failure 

		// We're good
		return(true);
	}




//////////
//
// Called to compute the SHA1-32 value on the indicated portion of the SParcelForBind header
//
//////
	bool iigt_validateParcelDeliverylSha1_32_lftve_mt_c(SParcelDelivery* tsBindMail)
	{
		u32		lnSha1_32;


		// Compute the sha1-32 value
		lnSha1_32	= 0;
		lnSha1_32	+=	iigt_computeSha1_32((u8*)&tsBindMail->totalMessageLength,	4);
		lnSha1_32	+=	iigt_computeSha1_32((u8*)&tsBindMail->fromId,				4);
		lnSha1_32	+=	iigt_computeSha1_32((u8*)&tsBindMail->toId,					4);
		lnSha1_32	+=	iigt_computeSha1_32((u8*)&tsBindMail->nValue,				4);
		lnSha1_32	+=	iigt_computeSha1_32((u8*)&tsBindMail->nExtra,				4);
		lnSha1_32	+=	iigt_computeSha1_32((u8*)&tsBindMail->messageTypeLength,	4);
		lnSha1_32	+=	iigt_computeSha1_32((u8*)&tsBindMail->contentLength,		4);
		return(tsBindMail->sha1_32_lftve_mt_c == lnSha1_32);
	}




//////////
//
// For now we don't use a true SHA-1 algorithm, but just add up the bytes
//
//////
	bool iigt_validateParcelDeliverySha1_32_messageType(SParcelDelivery* tsPd)
	{
		return(tsPd->messageTypeSha1_32 == iigt_computeSha1_32(tsPd->messageType, tsPd->messageTypeLength));
	}




//////////
//
// For now we don't use a true SHA-1 algorithm, but just add up the bytes
//
//////
	bool iigt_validateParcelDeliverySha1_32_content(SParcelDelivery* tsPd)
	{
		return(tsPd->contentLengthSha1_32 == iigt_computeSha1_32(tsPd->content, tsPd->contentLength));
	}




//////////
//
// For now we don't use a true SHA-1 algorithm, but just add up the bytes
//
//////
	bool iigt_validateParcelDeliverySha1_32_all(SParcelDelivery* tsPd)
	{
		u32		lnSha1_32;


		// Compute the sha1-32 value
		lnSha1_32	= 0;
		lnSha1_32	+=	iigt_computeSha1_32((u8*)tsPd,					sizeof(SParcelDelivery) - sizeof(s8*)/*content*/ - sizeof(s8*)/*messageType*/ - sizeof(u32)/*sha1_32_all*/);
		lnSha1_32	+=	iigt_computeSha1_32((u8*)tsPd->messageType,		tsPd->messageTypeLength);
		lnSha1_32	+=	iigt_computeSha1_32((u8*)tsPd->content,			tsPd->contentLength);
		return(tsPd->sha1_32_all == lnSha1_32);
	}




//////////
//
// Called to compute the SHA1-32 bit value
// For now we don't use a true SHA-1 algorithm, but just add up the bytes.
//
//////
	u32 iigt_computeSha1_32(u8* tcData, u32 tnDataLength)
	{
		u32 lnI, lnValue;


		// Value
		lnValue = 0;
		if (tcData)
		{
			// We iterate through each byte an do a computation
			for (lnI = 0; lnI < tnDataLength; lnI++)
			{
				lnValue +=	(u32)tcData[lnI]										+		/* base character */
							iigt_shiftLeft((u32)iigt_swapBits(tcData[lnI]),			8)		+		/* reverse bit order of base character */
							iigt_shiftLeft((u32)         (255 - tcData[lnI]),	16)		+		/* 255 - base character */
							iigt_shiftLeft((u32)iigt_swapBits(255 - tcData[lnI]),	24);			/* reverse bit order of (255 - base character) */
			}
		}
		// Indicate our value
		return(lnValue);
	}




//////////
//
// Called to swap the bits of an 8-bit character
//
//////
	u8 iigt_swapBits(u8 tcChar)
	{
		u8 lc;


		// Swap the bits
		lc	=	((tcChar & 0x1)  << 7) |		/* bit 1 to bit 8 */
				((tcChar & 0x2)  << 5) |		/* bit 2 to bit 7 */
				((tcChar & 0x4)  << 3) |		/* bit 3 to bit 6 */
				((tcChar & 0x8)  << 1) |		/* bit 4 to bit 5 */
				((tcChar & 0x10) >> 1) |		/* bit 5 to bit 4 */
				((tcChar & 0x20) >> 3) |		/* bit 6 to bit 3 */
				((tcChar & 0x40) >> 5) |		/* bit 7 to bit 2 */
				((tcChar & 0x80) >> 7);			/* bit 8 to bit 1 */

		// Indicate the new value
		return(lc);
	}




//////////
//
// Called to shift the 32-bit value left, and wrap the most significant bit around
// to the least significant bit.
//
//////
	u32 iigt_shiftLeft(u32 tnValue, u32 tnBits)
	{
		u32 lnI;


		// Rotate the bits around off the end back to the beginning
		for (lnI = 0; lnI < tnBits; lnI++)
			tnValue = ((tnValue & 0x8000000) != 0 ? 1 : 0) | (tnValue << 1);

		// Indicate the new result
		return(tnValue);
	}




//////////
//
// Called to do the exactly equals function as in VFP
//
//////
	bool iigt_equalEqual(u8* tcLeft, u32 tnLeftLength, u8* tcRight, u32 tnRightLength)
	{
		// Must be the same length to be exactly equal
		if (tnLeftLength == tnRightLength)
			return(_memicmp(tcLeft, tcRight, tnLeftLength) == 0);		// Returns true or false if equal

		// If we get here, does not equal
		return(false);
	}




//////////
//
// Called to copy as much of the string as will fit
//
//////
	void iigt_copyToShortestLengthNoPad(u8* tcDst, u32 tnDstLength, u8* tcSrc, u32 tnSrcLength)
	{
		if (tnDstLength != 0 && tnSrcLength != 0)
			memcpy(tcDst, tcSrc, min(tnDstLength, tnSrcLength));
	}




//////////
//
// Callback to intercept the HWND to draw the overlain color image
//
//////
	LRESULT CALLBACK igt_interfaceWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		u32				lnNumread;
		u8*				lcData;
		SInterface*		li;


		// Find out what message it is
		li = iigt_FindSInterfaceByRemoteMessageHwnd(&gsInterfaces, (u32)hwnd);
		if (li)
		{
			switch (uMsg)
			{
				case WMGT_DATA_IN_PIPE:
					// There is some invalid data in the pipe (an incomplete write operation) that needs to be burned
					if (lParam != 0)
					{
						// Load the data
						lcData = iigt_readFromPipe(li, (u32)lParam, &lnNumread);
						if (lcData && lnNumread == (u32)lParam)
						{
							// Receive and consume the message (releases its memory after receiving)
							iigt_receiveAndProcessMessage(li, lcData, lnNumread, (u32)hwnd);
							return(TRUE);
						}
						// Indicate failure
						return(FALSE);
					}
					return(0);		// A syntax that we don't know about

				case WMGT_BURN_DATA_IN_PIPE:
					// There is some invalid data in the pipe (an incomplete write operation) that needs to be burned
					if (lParam != 0)
					{
						// Load the data
						lcData = iigt_readFromPipe(li, (u32)lParam, &lnNumread);

						// Burn the data
						if (lcData)
							free(lcData);

						// Indicate success or failure
						return(lcData && lnNumread == (u32)lParam);
					}
					return(0);		// A syntax that we don't know about
			}
		}
		return(DefWindowProc(hwnd, uMsg, wParam, lParam));
	}
