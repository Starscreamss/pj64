#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include "audio.h"
#include "sound_out.h"

#define AI_STATUS_FIFO_FULL	0x80000000		/* Bit 31: full */
#define AI_STATUS_DMA_BUSY	0x40000000		/* Bit 30: busy */
#define MI_INTR_AI			0x04			/* Bit 2: AI intr */
#define NUMCAPTUREEVENTS	3
#define BufferSize			0x5000

#define Buffer_Empty		0
#define Buffer_Playing		1
#define Buffer_HalfFull		2
#define Buffer_Full			3

void FillBuffer            ( int buffer );
BOOL FillBufferWithSilence ( LPDIRECTSOUNDBUFFER lpDsb );
void FillSectionWithSilence( int buffer );
void SetupDSoundBuffers    ( void );
void Soundmemcpy           ( void * dest, const void * src, size_t count );

DWORD Frequency, Dacrate = 0, Snd1Len, SpaceLeft, SndBuffer[3], Playing;
AUDIO_INFO AudioInfo;
BYTE *Snd1ReadPos;

LPDIRECTSOUNDBUFFER  lpdsbuf;
LPDIRECTSOUND        lpds;
LPDIRECTSOUNDNOTIFY  lpdsNotify;
HANDLE               rghEvent[NUMCAPTUREEVENTS];
DSBPOSITIONNOTIFY    rgdscbpn[NUMCAPTUREEVENTS];


EXPORT void CALL AiDacrateChanged (int SystemType) {
	if (Dacrate != *AudioInfo.AI_DACRATE_REG) {
		Dacrate = *AudioInfo.AI_DACRATE_REG;
		switch (SystemType) {
		case SYSTEM_NTSC: Frequency = 48681812 / (Dacrate + 1); break;
		case SYSTEM_PAL:  Frequency = 49656530 / (Dacrate + 1); break;
		case SYSTEM_MPAL: Frequency = 48628316 / (Dacrate + 1); break;
		}
		SetupDSoundBuffers();
	}
}

EXPORT void CALL AiLenChanged (void) {
	int count, offset, temp;
	DWORD dwStatus;

	if (*AudioInfo.AI_LEN_REG == 0) { return; }
	*AudioInfo.AI_STATUS_REG |= AI_STATUS_FIFO_FULL;
	Snd1Len = (*AudioInfo.AI_LEN_REG & 0x3FFF8);
	temp = Snd1Len;
	Snd1ReadPos = AudioInfo.RDRAM + (*AudioInfo.AI_DRAM_ADDR_REG & 0x00FFFFF8);
	if (Playing) {
		for (count = 0; count < 3; count ++) {
			if (SndBuffer[count] == Buffer_Playing) {
				offset = (count + 1) & 3;
			}
		}
	} else {
		offset = 0;
	}

	for (count = 0; count < 3; count ++) {
		if (SndBuffer[(count + offset) & 3] == Buffer_HalfFull) {
			FillBuffer((count + offset) & 3);
			count = 3;
		}
	}
	for (count = 0; count < 3; count ++) {
		if (SndBuffer[(count + offset) & 3] == Buffer_Full) {
			FillBuffer((count + offset + 1) & 3);
			FillBuffer((count + offset + 2) & 3);
			count = 20;
		}
	}
	if (count < 10) {
		FillBuffer((0 + offset) & 3);
		FillBuffer((1 + offset) & 3);
		FillBuffer((2 + offset) & 3);
	}

	if (!Playing) {
		for (count = 0; count < 3; count ++) {
			if (SndBuffer[count] == Buffer_Full) {
				Playing = TRUE;
				IDirectSoundBuffer_Play(lpdsbuf, 0, 0, 0 );
				return;
			}
		}
	} else {
		IDirectSoundBuffer_GetStatus(lpdsbuf,&dwStatus);
		if ((dwStatus & DSBSTATUS_PLAYING) == 0) {
			IDirectSoundBuffer_Play(lpdsbuf, 0, 0, 0 );
		}
	}
}

EXPORT DWORD CALL AiReadLength (void) {
	return Snd1Len;
}

EXPORT void CALL AiUpdate (BOOL Wait) {
	DWORD dwEvt;

	if (Wait) {
		dwEvt = MsgWaitForMultipleObjects(NUMCAPTUREEVENTS,rghEvent,FALSE,
			INFINITE,QS_ALLINPUT);
	} else {
		dwEvt = MsgWaitForMultipleObjects(NUMCAPTUREEVENTS,rghEvent,FALSE,
			0,QS_ALLINPUT);
	}
	dwEvt -= WAIT_OBJECT_0;
	if (dwEvt == NUMCAPTUREEVENTS) {
		return;
	}

	switch (dwEvt) {
	case 0: 
		SndBuffer[0] = Buffer_Empty;
		FillSectionWithSilence(0);
		SndBuffer[1] = Buffer_Playing;
		FillBuffer(2);
		FillBuffer(0);
		break;
	case 1: 
		SndBuffer[1] = Buffer_Empty;
		FillSectionWithSilence(1);
		SndBuffer[2] = Buffer_Playing;
		FillBuffer(0);
		FillBuffer(1);
		break;
	case 2: 
		SndBuffer[2] = Buffer_Empty;
		FillSectionWithSilence(2);
		SndBuffer[0] = Buffer_Playing;
		FillBuffer(1);
		FillBuffer(2);		
	    IDirectSoundBuffer_Play(lpdsbuf, 0, 0, 0 );
		break;
	}
}

EXPORT void CALL CloseDLL (void) {
    if (lpdsbuf) { 
		IDirectSoundBuffer_Release(lpdsbuf);
	}
    if ( lpds ) {
		IDirectSound_Release(lpds);
	}
}

void DisplayError (char * Message, ...) {
	char Msg[400];
	va_list ap;

	va_start( ap, Message );
	vsprintf( Msg, Message, ap );
	va_end( ap );
	MessageBox(NULL,Msg,"Error",MB_OK|MB_ICONERROR);
}

EXPORT void CALL DllAbout ( HWND hParent ) {
	DisplayError ("Basic Audio plugin by zilmar");
}

void FillBuffer ( int buffer ) {
    DWORD dwBytesLocked;
    VOID *lpvData;

	if (Snd1Len == 0) { return; }
	if (SndBuffer[buffer] == Buffer_Empty) {
		if (Snd1Len >= BufferSize) {
			if (FAILED( IDirectSoundBuffer_Lock(lpdsbuf, BufferSize * buffer,BufferSize, &lpvData, &dwBytesLocked,
				NULL, NULL, 0  ) ) )
			{
				IDirectSoundBuffer_Unlock(lpdsbuf, lpvData, dwBytesLocked, NULL, 0 );
				DisplayError("FAILED lock");
				return;
			}
			Soundmemcpy(lpvData,Snd1ReadPos,dwBytesLocked);
			SndBuffer[buffer] = Buffer_Full;
			Snd1ReadPos += dwBytesLocked;
			Snd1Len -= dwBytesLocked;
			IDirectSoundBuffer_Unlock(lpdsbuf, lpvData, dwBytesLocked, NULL, 0 );
		} else {
			if (FAILED( IDirectSoundBuffer_Lock(lpdsbuf, BufferSize * buffer,Snd1Len, &lpvData, &dwBytesLocked,
				NULL, NULL, 0  ) ) )
			{
				IDirectSoundBuffer_Unlock(lpdsbuf, lpvData, dwBytesLocked, NULL, 0 );
				DisplayError("FAILED lock");
				return;
			}
			Soundmemcpy(lpvData,Snd1ReadPos,dwBytesLocked);
			SndBuffer[buffer] = Buffer_HalfFull;
			Snd1ReadPos += dwBytesLocked;
			SpaceLeft = BufferSize - Snd1Len;
			Snd1Len = 0;
			IDirectSoundBuffer_Unlock(lpdsbuf, lpvData, dwBytesLocked, NULL, 0 );
		}
	} else if (SndBuffer[buffer] == Buffer_HalfFull) {
		if (Snd1Len >= SpaceLeft) {
			if (FAILED( IDirectSoundBuffer_Lock(lpdsbuf, (BufferSize * (buffer + 1)) - SpaceLeft ,SpaceLeft, &lpvData,
				&dwBytesLocked, NULL, NULL, 0  ) ) )
			{
				IDirectSoundBuffer_Unlock(lpdsbuf, lpvData, dwBytesLocked, NULL, 0 );
				DisplayError("FAILED lock");
				return;
			}
			Soundmemcpy(lpvData,Snd1ReadPos,dwBytesLocked);
			SndBuffer[buffer] = Buffer_Full;
			Snd1ReadPos += dwBytesLocked;
			Snd1Len -= dwBytesLocked;
			IDirectSoundBuffer_Unlock(lpdsbuf, lpvData, dwBytesLocked, NULL, 0 );
		} else {
			if (FAILED( IDirectSoundBuffer_Lock(lpdsbuf, (BufferSize * (buffer + 1)) - SpaceLeft,Snd1Len, &lpvData, &dwBytesLocked,
				NULL, NULL, 0  ) ) )
			{
				IDirectSoundBuffer_Unlock(lpdsbuf, lpvData, dwBytesLocked, NULL, 0 );
				DisplayError("FAILED lock");
				return;
			}
			Soundmemcpy(lpvData,Snd1ReadPos,dwBytesLocked);
			SndBuffer[buffer] = Buffer_HalfFull;
			Snd1ReadPos += dwBytesLocked;
			SpaceLeft = SpaceLeft - Snd1Len;
			Snd1Len = 0;
			IDirectSoundBuffer_Unlock(lpdsbuf, lpvData, dwBytesLocked, NULL, 0 );
		}
	}
	if (Snd1Len == 0) {
		*AudioInfo.AI_STATUS_REG &= ~AI_STATUS_FIFO_FULL;
		*AudioInfo.MI_INTR_REG |= MI_INTR_AI;
		AudioInfo.CheckInterrupts();
	}
}

BOOL FillBufferWithSilence( LPDIRECTSOUNDBUFFER lpDsb ) {
    WAVEFORMATEX    wfx;
    DWORD           dwSizeWritten;

    PBYTE   pb1;
    DWORD   cb1;

    if ( FAILED( IDirectSoundBuffer_GetFormat(lpDsb, &wfx, sizeof( WAVEFORMATEX ), &dwSizeWritten ) ) ) {
        return FALSE;
	}

    if ( SUCCEEDED( IDirectSoundBuffer_Lock(lpDsb,0,0,(LPVOID*)&pb1,&cb1,NULL,NULL,DSBLOCK_ENTIREBUFFER))) {
        FillMemory( pb1, cb1, ( wfx.wBitsPerSample == 8 ) ? 128 : 0 );

        IDirectSoundBuffer_Unlock(lpDsb, pb1, cb1, NULL, 0 );
        return TRUE;
    }

    return FALSE;
}

void FillSectionWithSilence( int buffer ) {
    DWORD dwBytesLocked;
    VOID *lpvData;

	if (FAILED( IDirectSoundBuffer_Lock(lpdsbuf, BufferSize * buffer,BufferSize, &lpvData, &dwBytesLocked,
		NULL, NULL, 0  ) ) )
	{
		IDirectSoundBuffer_Unlock(lpdsbuf, lpvData, dwBytesLocked, NULL, 0 );
		DisplayError("FAILED lock");
		return;
	}
    FillMemory( lpvData, dwBytesLocked, 0 );
	IDirectSoundBuffer_Unlock(lpdsbuf, lpvData, dwBytesLocked, NULL, 0 );
}

EXPORT void CALL GetDllInfo ( PLUGIN_INFO * PluginInfo ){ 
	PluginInfo->Version = 0x0101;
	PluginInfo->Type    = PLUGIN_TYPE_AUDIO;
	sprintf(PluginInfo->Name,"Basic Audio Plugin");
	PluginInfo->NormalMemory  = TRUE;
	PluginInfo->MemoryBswaped = TRUE;
}

EXPORT BOOL CALL InitiateAudio (AUDIO_INFO Audio_Info) {
	HRESULT hr;
	int count;

	AudioInfo = Audio_Info;
 
    if ( FAILED( hr = DirectSoundCreate( NULL, &lpds, NULL ) ) ) {
        return FALSE;
	}

    if ( FAILED( hr = IDirectSound_SetCooperativeLevel(lpds, AudioInfo.hwnd, DSSCL_PRIORITY   ))) {
        return FALSE;
	}
    for ( count = 0; count < NUMCAPTUREEVENTS; count++ ) {
        rghEvent[count] = CreateEvent( NULL, FALSE, FALSE, NULL );
        if (rghEvent[count] == NULL ) { return FALSE; }
    }
	Dacrate = 0;
	Playing = FALSE;	
	SndBuffer[0] = Buffer_Empty;
	SndBuffer[1] = Buffer_Empty;
	SndBuffer[2] = Buffer_Empty;
	return TRUE;
}

EXPORT void CALL ProcessAList(void) {
}

EXPORT void CALL RomClosed (void) {
}

void SetupDSoundBuffers(void) {
	LPDIRECTSOUNDBUFFER lpdsb;
    DSBUFFERDESC        dsPrimaryBuff, dsbdesc;
    WAVEFORMATEX        wfm;
    HRESULT             hr;

    if (lpdsbuf) { IDirectSoundBuffer_Release(lpdsbuf); }
	memset( &dsPrimaryBuff, 0, sizeof( DSBUFFERDESC ) ); 
    
	dsPrimaryBuff.dwSize        = sizeof( DSBUFFERDESC ); 
    dsPrimaryBuff.dwFlags       = DSBCAPS_PRIMARYBUFFER; 
    dsPrimaryBuff.dwBufferBytes = 0;  
    dsPrimaryBuff.lpwfxFormat   = NULL; 
    memset( &wfm, 0, sizeof( WAVEFORMATEX ) ); 

	wfm.wFormatTag = WAVE_FORMAT_PCM;
	wfm.nChannels = 2;
	wfm.nSamplesPerSec = 44100;
	wfm.wBitsPerSample = 16;
	wfm.nBlockAlign = wfm.wBitsPerSample / 8 * wfm.nChannels;
	wfm.nAvgBytesPerSec = wfm.nSamplesPerSec * wfm.nBlockAlign;

	hr = IDirectSound_CreateSoundBuffer(lpds,&dsPrimaryBuff, &lpdsb, NULL);
	
	if (SUCCEEDED ( hr ) ) {
		IDirectSoundBuffer_SetFormat(lpdsb, &wfm );
	    IDirectSoundBuffer_Play(lpdsb, 0, 0, DSBPLAY_LOOPING );
	}

	wfm.nSamplesPerSec = Frequency;
	wfm.wBitsPerSample = 16;
	wfm.nBlockAlign = wfm.wBitsPerSample / 8 * wfm.nChannels;
	wfm.nAvgBytesPerSec = wfm.nSamplesPerSec * wfm.nBlockAlign;

    memset( &dsbdesc, 0, sizeof( DSBUFFERDESC ) ); 
    dsbdesc.dwSize = sizeof( DSBUFFERDESC ); 
    dsbdesc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY;
    dsbdesc.dwBufferBytes = BufferSize * 3;  
    dsbdesc.lpwfxFormat = &wfm; 

	if ( FAILED( hr = IDirectSound_CreateSoundBuffer(lpds, &dsbdesc, &lpdsbuf, NULL ) ) ) {
		DisplayError("Failed in creation of Play buffer 1");	
	}
	FillBufferWithSilence( lpdsbuf );

    rgdscbpn[0].dwOffset = ( BufferSize ) - 1;
    rgdscbpn[0].hEventNotify = rghEvent[0];
    rgdscbpn[1].dwOffset = ( BufferSize * 2) - 1;
    rgdscbpn[1].hEventNotify = rghEvent[1];
    rgdscbpn[2].dwOffset = ( BufferSize * 3) - 1;
    rgdscbpn[2].hEventNotify = rghEvent[2];
    rgdscbpn[3].dwOffset = DSBPN_OFFSETSTOP;
    rgdscbpn[3].hEventNotify = rghEvent[3];

    if ( FAILED( hr = IDirectSound_QueryInterface(lpdsbuf, &IID_IDirectSoundNotify, ( VOID ** )&lpdsNotify ) ) ) {
		DisplayError("IDirectSound_QueryInterface: Failed\n");
		return;
	}

    // Set capture buffer notifications.
    if ( FAILED( hr = IDirectSoundNotify_SetNotificationPositions(lpdsNotify, NUMCAPTUREEVENTS, rgdscbpn ) ) ) {
		DisplayError("IDirectSoundNotify_SetNotificationPositions: Failed");
		return;
    }
}

void Soundmemcpy(void * dest, const void * src, size_t count) {
	if (AudioInfo.MemoryBswaped) {
		_asm {
			mov edi, dest
			mov ecx, src
			mov edx, 0		
		memcpyloop1:
			mov ax, word ptr [ecx + edx]
			mov bx, word ptr [ecx + edx + 2]
			mov  word ptr [edi + edx + 2],ax
			mov  word ptr [edi + edx],bx
			add edx, 4
			mov ax, word ptr [ecx + edx]
			mov bx, word ptr [ecx + edx + 2]
			mov  word ptr [edi + edx + 2],ax
			mov  word ptr [edi + edx],bx
			add edx, 4
			cmp edx, count
			jb memcpyloop1
		}
	} else {
		_asm {
			mov edi, dest
			mov ecx, src
			mov edx, 0		
		memcpyloop2:
			mov ax, word ptr [ecx + edx]
			xchg ah,al
			mov  word ptr [edi + edx],ax
			add edx, 2
			mov ax, word ptr [ecx + edx]
			xchg ah,al
			mov  word ptr [edi + edx],ax
			add edx, 2
			mov ax, word ptr [ecx + edx]
			xchg ah,al
			mov  word ptr [edi + edx],ax
			add edx, 2
			mov ax, word ptr [ecx + edx]
			xchg ah,al
			mov  word ptr [edi + edx],ax
			add edx, 2
			cmp edx, count
			jb memcpyloop2
		}
	}
}
