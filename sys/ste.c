/*
 * Copyright (C) 2004-2010 Kazuyoshi Aizawa. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/********************************************************************************
 * ste.c
 * 
 * �h���C�o�̐���
 *
 *    ste.sys
 * 
 *    ���z NIC�i�l�b�g���[�N�C���^�[�t�F�[�X�J�[�h�j�f�o�C�X�h���C�o ste �� Windows �ŁB
 *    NDIS �󂯎�����f�[�^�� Ethernet �t���[���Ƃ��ă��[�U�v���Z�X�ł��� sted �ɓn���A
 *    �܂��Asted ����󂯎���� Ethernet�t���[���̃f�[�^���� NDIS �ɓn���B
 *
 *  �{���W���[���̐���
 *
 *     ste.c
 *
 *    �{���W���[���� NDIS ����Ăяo�����̃G���g���|�C���g�iSteMiniportXXX�j���L�q��
 *    ��Ă���ADriverEntry() �ɂāA�����̃G���g���|�C���g�̓o�^���s���B
 *
 *  �ύX����:
 *    2005/07/18
 *      o Opackets ���J�E���g����悤�ɕύX�����B
 *     
 ************************************************************************************/
#include "ste.h"

NDIS_HANDLE      NdisWrapperHandle;     // DireverEntry() �̒��Ŏg����B�G���Ă͂����Ȃ��B
NDIS_SPIN_LOCK   SteGlobalSpinLock;     // �h���C�o�̃O���[�o�����b�N
STE_ADAPTER     *SteAdapterHead = NULL; // STE_ADAPTER �̃����N���X�g�̃w�b�h

// �T�|�[�g���K�{(M)�̍��ڂ̂ݒ��o(�S 34 OID)
NDIS_OID STESupportedList[] = {
    //
    // ��ʓI�ȓ��� (22 OID)
    //
    OID_GEN_SUPPORTED_LIST,        // �T�|�[�g����� OID �̃��X�g
    OID_GEN_HARDWARE_STATUS,       // �n�[�h�E�F�A�X�e�[�^�X
    OID_GEN_MEDIA_SUPPORTED,       // NIC ���T�|�[�g�ł���i���K�{�ł͂Ȃ��j���f�B�A�^�C�v
    OID_GEN_MEDIA_IN_USE,          // NIC �����ݎg���Ă��銮�S�ȃ��f�B�A�^�C�v�̃��X�g
    OID_GEN_MAXIMUM_LOOKAHEAD,     // NIC �� lookahead �f�[�^�Ƃ��Ē񋟂ł���ő�o�C�g��
    OID_GEN_MAXIMUM_FRAME_SIZE,    // NIC ���T�|�[�g����A�w�b�_�𔲂����l�b�g���[�N�p�P�b�g�T�C�Y
    OID_GEN_LINK_SPEED,            // NIC ���T�|�[�g����ő�X�s�[�h
    OID_GEN_TRANSMIT_BUFFER_SPACE, // NIC ��̑��M�p�̃������̑���
    OID_GEN_RECEIVE_BUFFER_SPACE,  // NIC ��̎�M�p�̃������̑���
    OID_GEN_TRANSMIT_BLOCK_SIZE,   // NIC ���T�|�[�g���鑗�M�p�̃l�b�g���[�N�p�P�b�g�T�C�Y
    OID_GEN_RECEIVE_BLOCK_SIZE,    // NIC ���T�|�[�g�����M�p�̃l�b�g���[�N�p�P�b�g�T�C�Y
    OID_GEN_VENDOR_ID,             // IEEE �ɓo�^���Ă���x���_�[�R�[�h�i�o�^����ĂȂ��ꍇ 0xFFFFFF�j
    OID_GEN_VENDOR_DESCRIPTION,    // NIC �̃x���_�[��
    OID_GEN_VENDOR_DRIVER_VERSION, // �h���C�o�[�̃o�[�W����
    OID_GEN_CURRENT_PACKET_FILTER, // �v���g�R���� NIC ����󂯎��p�P�b�g�̃^�C�v
    OID_GEN_CURRENT_LOOKAHEAD,     // ���݂� lookahead �̃o�C�g��
    OID_GEN_DRIVER_VERSION,        // NDIS �̃o�[�W����
    OID_GEN_MAXIMUM_TOTAL_SIZE,    // NIC ���T�|�[�g����l�b�g���[�N�p�P�b�g�T�C�Y
    OID_GEN_PROTOCOL_OPTIONS,      // �I�v�V�����̃v���g�R���t���O
    OID_GEN_MAC_OPTIONS,           // �ǉ��� NIC �̃v���p�e�B���`�����r�b�g�}�X�N
    OID_GEN_MEDIA_CONNECT_STATUS,  // NIC ��� connection ���
    OID_GEN_MAXIMUM_SEND_PACKETS,  // ���̃��N�G�X�g�Ŏ󂯂���p�P�b�g�̍ő吔
    //
    // ��ʓI�ȓ��v��� (5 OID)
    //
    OID_GEN_XMIT_OK,               // ����ɑ��M�ł����t���[����
    OID_GEN_RCV_OK,                // ����Ɏ�M�ł����t���[����
    OID_GEN_XMIT_ERROR,            // ���M�ł��Ȃ������i�������̓G���[�ɂȂ����j�t���[����
    OID_GEN_RCV_ERROR,             // ��M�ł��Ȃ������i�������̓G���[�ɂȂ����j�t���[����
    OID_GEN_RCV_NO_BUFFER,         // �o�b�t�@�s���̂��߂Ɏ�M�ł��Ȃ������t���[����
    //
    // Ethernet �p�̓��� (4 OID)
    //
    OID_802_3_PERMANENT_ADDRESS,   // �n�[�h�E�F�A�ɏ�����Ă��� MAC �A�h���X
    OID_802_3_CURRENT_ADDRESS,     // NIC �̌��݂� MAC �A�h���X
    OID_802_3_MULTICAST_LIST,      // ���݂̃}���`�L���X�g�p�P�b�g�̃A�h���X���X�g
    OID_802_3_MAXIMUM_LIST_SIZE,   // NIC �h���C�o���Ǘ��ł���ő�̃}���`�L���X�g�A�h���X�̐�
    //
    // Ethernet �p���v��� (3 OID)
    //
    OID_802_3_RCV_ERROR_ALIGNMENT,   // �A���C�����g�G���[�̎�M�t���[����
    OID_802_3_XMIT_ONE_COLLISION,    // �R���W������ 1 �񔭐��������M�t���[����
    OID_802_3_XMIT_MORE_COLLISIONS   // �R���W������ 1 ��ȏ㔭���������M�t���[����
};

/*****************************************************************************
 * DriverEntry()
 *
 *   ���̊֐��̂� System �����̃h���C�o�����[�h����Ƃ��ɌĂ΂�A�h���C�o��
 *   NDIS �Ɗ֘A�t���A�G���g���|�C���g��o�^����B
 * 
 * ����:
 *     DriverObject :  �h���C�o�[�I�u�W�F�N�g�̃|�C���^
 *     RegistryPath :  �h���C�o�[�̃��W�X�g���̃p�X �֘A�t��
 *  
 * �Ԃ�l:
 * 
 *     NDIS_STATUS 
 * 
 ********************************************************************************/
NDIS_STATUS
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID RegistryPath)
{
    NDIS_MINIPORT_CHARACTERISTICS  MiniportCharacteristics;
    NDIS_STATUS                    Status;

    DEBUG_PRINT0(3, "DriverEntry called\n");        

    NdisZeroMemory(&MiniportCharacteristics, sizeof(NDIS_MINIPORT_CHARACTERISTICS));

    /*
     * �~�j�|�[�g�h���C�o�� NDIS �Ɋ֘A�t�����ANdisWrapperHandle �𓾂�B
     */
    NdisMInitializeWrapper(
        &NdisWrapperHandle, // OUT PNDIS_HANDLE  
        DriverObject,       // IN �h���C�o�[�I�u�W�F�N�g
        RegistryPath,       // IN ���W�X�g���p�X
        NULL                // IN �K�� NULL
        );

    if(NdisWrapperHandle == NULL){
        DEBUG_PRINT0(1, "NdisInitializeWrapper failed\n");
        return(STATUS_INVALID_HANDLE);
    }

    MiniportCharacteristics.MajorNdisVersion         = STE_NDIS_MAJOR_VERSION; // Major Version 
    MiniportCharacteristics.MinorNdisVersion         = STE_NDIS_MINOR_VERSION; // Minor Version 
    MiniportCharacteristics.CheckForHangHandler      = SteMiniportCheckForHang;     
    MiniportCharacteristics.HaltHandler              = SteMiniportHalt;             
    MiniportCharacteristics.InitializeHandler        = SteMiniportInitialize;       
    MiniportCharacteristics.QueryInformationHandler  = SteMiniportQueryInformation; 
    MiniportCharacteristics.ResetHandler             = SteMiniportReset ;            
    MiniportCharacteristics.SetInformationHandler    = SteMiniportSetInformation;   
    MiniportCharacteristics.ReturnPacketHandler      = SteMiniportReturnPacket;    
    MiniportCharacteristics.SendPacketsHandler       = SteMiniportSendPackets; 

    Status = NdisMRegisterMiniport(     
        NdisWrapperHandle,                    // IN NDIS_HANDLE                     
        &MiniportCharacteristics,             // IN PNDIS_MINIPORT_CHARACTERISTICS  
        sizeof(NDIS_MINIPORT_CHARACTERISTICS) // IN UINT                            
        );

    if( Status != NDIS_STATUS_SUCCESS){
        DEBUG_PRINT1(1, "NdisMRegisterMiniport failed(Status = 0x%x)\n", Status);
        NdisTerminateWrapper(
            NdisWrapperHandle, // IN NDIS_HANDLE  
            NULL               
            );        
        return(Status);
    }

    // �O���[�o�����b�N��������
    NdisAllocateSpinLock(&SteGlobalSpinLock);

    NdisMRegisterUnloadHandler(NdisWrapperHandle, SteMiniportUnload);    
    
    return(NDIS_STATUS_SUCCESS);
}

/*************************************************************************
 * SteMiniportinitialize()
 *
 * �@NDIS �G���g���|�C���g
 *   �l�b�g���[�N I/O ����̂��߂� NIC �h���C�o���l�b�g���[�N I/O �����
 *   ���邽�߂ɕK�v�ȃ��\�[�X���m�ۂ���B
 *
 *  ����:
 *
 *         OpenErrorStatus              OUT PNDIS_STATUS 
 *         SelectedMediumIndex          OUT PUINT        
 *         MediumArray                  IN PNDIS_MEDIUM  
 *         MediumArraySize              IN UINT          
 *         MiniportAdapterHandle        IN NDIS_HANDLE   
 *         WrapperConfigurationContext  IN NDIS_HANDLE   
 *
 *  �Ԃ�l:
 *    
 *     ���펞: NDIS_STATUS_SUCCESS
 *
 *************************************************************************/
NDIS_STATUS 
SteMiniportInitialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT        SelectedMediumIndex,
    IN PNDIS_MEDIUM  MediumArray,
    IN UINT          MediumArraySize,
    IN NDIS_HANDLE   MiniportAdapterHandle,
    IN NDIS_HANDLE   WrapperConfigurationContext
    )
{
    UINT             i;
    NDIS_STATUS     Status  = NDIS_STATUS_SUCCESS;
    STE_ADAPTER    *Adapter = NULL;
    BOOLEAN          MediaFound = FALSE;

    DEBUG_PRINT0(3, "SteMiniportInitialize called\n");    

    *SelectedMediumIndex = 0;
    
    for ( i = 0 ; i < MediumArraySize ; i++){
        if (MediumArray[i] == NdisMedium802_3){
            *SelectedMediumIndex = i;
            MediaFound = TRUE;
            break;
        }
    }

    // �r���� break ���邽�߂����� Do-While ��
    do {
        if(!MediaFound){
            // ��L�� for ���Ō������Ȃ������悤��
            DEBUG_PRINT0(1, "SteMiniportInitialize: No Media much\n");        
            Status = NDIS_STATUS_UNSUPPORTED_MEDIA;
            break;
        }

        //
        // Adapter ���m�ۂ��A����������
        //
        if ((Status = SteCreateAdapter(&Adapter)) != NDIS_STATUS_SUCCESS){
            DEBUG_PRINT0(1, "SteMiniportInitialize: Can't allocate memory for STE_ADAPTER\n");
            Status = NDIS_STATUS_RESOURCES;
            break;
        }

        DEBUG_PRINT1(3, "SteMiniportInitialize: Adapter = 0x%p\n", Adapter);        

        Adapter->MiniportAdapterHandle = MiniportAdapterHandle;

        //
        // Registory ��ǂޏ����B...�ȗ��B
        //    NdisOpenConfiguration();
        //    NdisReadConfiguration();
        //
        // NIC �̂��߂̃n�[�h�E�F�A���\�[�X�̃��X�g�𓾂�B...�ȗ��B
        //    NdisMQueryAdapterResources()
        //

        //
        // NDIS �� NIC �̏���`����B
        // ���Ȃ炸 NdisXxx �֐����Ăяo�����O�ɁA�ȉ��� NdisMSetAttributesEx
        // ���Ăяo���Ȃ���΂Ȃ�Ȃ��B
        //
        NdisMSetAttributesEx(
            MiniportAdapterHandle,       //IN NDIS_HANDLE 
            (NDIS_HANDLE) Adapter,       //IN NDIS_HANDLE 
            0,                           //IN UINT  
            NDIS_ATTRIBUTE_DESERIALIZE,  //IN ULONG  Deserialized �~�j�|�[�g�h���C�o
            NdisInterfaceInternal        //IN NDIS_INTERFACE_TYPE 
            );

        //
        // NDIS 5.0 �̏ꍇ�͂��Ȃ炸 SHUTDOWN_HANDLER ��o�^���Ȃ���΂Ȃ�Ȃ��B
        //
        NdisMRegisterAdapterShutdownHandler(
            MiniportAdapterHandle,                         // IN NDIS_HANDLE
            (PVOID) Adapter,                               // IN PVOID 
            (ADAPTER_SHUTDOWN_HANDLER) SteMiniportShutdown // IN ADAPTER_SHUTDOWN_HANDLER  
            );

        //
        // ���z NIC �f�[��������� IOCT/ReadFile/WriteFile �p��
        // �f�o�C�X���쐬���ADispatch�@���[�`����o�^����B
        //
        SteRegisterDevice(Adapter);
    
        //
        // SteRecvTimerFunc() ���ĂԂ��߂̃^�C�}�[�I�u�W�F�N�g��������
        //

        NdisInitializeTimer(
            &Adapter->RecvTimer,     //IN OUT PNDIS_TIMER  
            SteRecvTimerFunc,        //IN PNDIS_TIMER_FUNCTION      
            (PVOID)Adapter           //IN PVOID
            );
        //
        // SteResetTimerFunc() ���ĂԂ��߂̃^�C�}�[�I�u�W�F�N�g��������
        //
        NdisInitializeTimer(
            &Adapter->ResetTimer,    //IN OUT PNDIS_TIMER  
            SteResetTimerFunc,       //IN PNDIS_TIMER_FUNCTION      
            (PVOID)Adapter           //IN PVOID
            );
        
    } while (FALSE);
    
    
    return(Status);
}

/****************************************************************************
 * SteMiniportShutdown()
 *
 *   NDIS �G���g���|�C���g
 *   �V�X�e���̃V���b�g�_�E����A�\�����ʃV�X�e���G���[���ɁANIC ���������
 *   �ɖ߂����߂ɌĂ΂��B�����ł̓��������\�[�X�����������A�p�P�b�g�̓]
 *   ��������҂�����͂��Ȃ�
 *
 * ����:
 *
 *    ShutdownContext : STE_ADAPTER �\���̂̃|�C���^
 *
 * �߂�l:
 *
 *    ����
 *
***************************************************************************/
VOID
SteMiniportShutdown(
    IN PVOID  ShutdownContext
    )
{
    STE_ADAPTER     *Adapter;

    DEBUG_PRINT0(3, "SteMiniportShutdown called\n");        

    Adapter = (STE_ADAPTER *)ShutdownContext;

    return;
}

/************************************************************************
 * SteMiniportQueryInformation()
 *
 *  NDIS �G���g���|�C���g
 *  OID �̒l��₢���킹�邽�߂� NDIS �ɂ���ČĂ΂��B
 * 
 * ����:
 * 
 *     MiniportAdapterContext  :   STE_ADAPTER �\���̂̃|�C���^
 *     Oid                     :   ���̖₢���킹�� OID
 *     InformationBuffer       :   ���̂��߂̃o�b�t�@�[
 *     InformationBufferLength :   �o�b�t�@�̃T�C�Y
 *     BytesWritten            :   �����̏�񂪋L�q���ꂽ��
 *     BytesNeeded             :   �o�b�t�@�����Ȃ��ꍇ�ɕK�v�ȃT�C�Y���w��
 * 
 * �Ԃ�l:
 *
 *     ���펞 :  NDIS_STATUS_SUCCESS
 *     
 ************************************************************************/
NDIS_STATUS
SteMiniportQueryInformation(
    IN NDIS_HANDLE  MiniportAdapterContext,
    IN NDIS_OID     Oid,
    IN PVOID        InformationBuffer,
    IN ULONG        InformationBufferLength,
    OUT PULONG      BytesWritten,
    OUT PULONG      BytesNeeded
    )
{
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
    STE_ADAPTER    *Adapter;
    PVOID           Information = NULL;     // �񋟂�����ւ̃|�C���^
    ULONG           InformationLength = 0;  // �񋟂�����̒���
    ULONG           ulTemp;                 // �����l�̏��̂��߂̗̈�i�}�N�����ŗ��p�j
    CHAR            VendorName[] = STE_VENDOR_NAME; // �x���_�[��

    DEBUG_PRINT0(3, "SteMiniportQueryInformation called\n");    

    Adapter = (STE_ADAPTER *)MiniportAdapterContext;

    DEBUG_PRINT1(3, "SteMiniportQueryInformation: Oid = 0x%x\n", Oid);        

    switch(Oid) {    
        // ��ʓI�ȓ��� �i22��)
        case OID_GEN_SUPPORTED_LIST:        //�T�|�[�g����� OID �̃��X�g
            SET_INFORMATION_BY_POINTER(sizeof(STESupportedList), &STESupportedList);
            
        case OID_GEN_HARDWARE_STATUS:       // �n�[�h�E�F�A�X�e�[�^�X
            SET_INFORMATION_BY_VALUE(sizeof(NDIS_HARDWARE_STATUS), NdisHardwareStatusReady);
            
        case OID_GEN_MEDIA_SUPPORTED:       // NIC ���T�|�[�g�ł���i���K�{�ł͂Ȃ��j���f�B�A�^�C�v
        case OID_GEN_MEDIA_IN_USE:          // NIC �����ݎg���Ă��銮�S�ȃ��f�B�A�^�C�v�̃��X�g
            SET_INFORMATION_BY_VALUE(sizeof(NDIS_MEDIUM), NdisMedium802_3);
            
        case OID_GEN_MAXIMUM_LOOKAHEAD:     // NIC �� lookahead �f�[�^�Ƃ��Ē񋟂ł���ő�o�C�g��
        case OID_GEN_MAXIMUM_FRAME_SIZE:    // NIC ���T�|�[�g����A�w�b�_�𔲂����l�b�g���[�N�p�P�b�g�T�C�Y
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), ETHERMTU);
            
        case OID_GEN_LINK_SPEED:            //NIC ���T�|�[�g����ő�X�s�[�h
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), ETHERLINKSPEED);
            
        case OID_GEN_TRANSMIT_BUFFER_SPACE: // NIC ��̑��M�p�̃������̑���
            // TODO: ����ł����̂��H
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), ETHERMTU);
            
        case OID_GEN_RECEIVE_BUFFER_SPACE:  // NIC ��̎�M�p�̃������̑���
            // TODO: ����ł����̂��H
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), ETHERMTU);            
            
        case OID_GEN_TRANSMIT_BLOCK_SIZE:   // NIC ���T�|�[�g���鑗�M�p�̃l�b�g���[�N�p�P�b�g�T�C�Y
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), ETHERMAX);                        

        case OID_GEN_RECEIVE_BLOCK_SIZE:    // NIC ���T�|�[�g�����M�p�̃l�b�g���[�N�p�P�b�g�T�C�Y
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), ETHERMAX);                                    

        case OID_GEN_VENDOR_ID:             // IEEE �ɓo�^���Ă���x���_�[�R�[�h
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), 0xFFFFFF);            
            
        case OID_GEN_VENDOR_DESCRIPTION:    // NIC �̃x���_�[��
            SET_INFORMATION_BY_POINTER(sizeof(VendorName), VendorName);
            
        case OID_GEN_VENDOR_DRIVER_VERSION: // �h���C�o�[�̃o�[�W����
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), STE_DRIVER_VERSION);                        

        case OID_GEN_CURRENT_PACKET_FILTER: // �v���g�R���� NIC ����󂯎��p�P�b�g�̃^�C�v
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), Adapter->PacketFilter);
            
        case OID_GEN_CURRENT_LOOKAHEAD:     // ���݂� lookahead �̃o�C�g��
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), ETHERMTU);
            
        case OID_GEN_DRIVER_VERSION:        // NDIS �̃o�[�W����
            SET_INFORMATION_BY_VALUE(sizeof(USHORT), STE_NDIS_VERSION);
            
        case OID_GEN_MAXIMUM_TOTAL_SIZE:    // NIC ���T�|�[�g����l�b�g���[�N�p�P�b�g�T�C�Y
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), ETHERMAX);
            
       // case OID_GEN_PROTOCOL_OPTIONS:      // �I�v�V�����̃v���g�R���t���O�BSet �̂ݕK�{
                    
        case OID_GEN_MAC_OPTIONS:           // �ǉ��� NIC �̃v���p�e�B���`�����r�b�g�}�X�N
            SET_INFORMATION_BY_VALUE(sizeof(ULONG),
                                     NDIS_MAC_OPTION_NO_LOOPBACK |
                                     NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
                                     NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA);

        case OID_GEN_MEDIA_CONNECT_STATUS:  // NIC ��� connection ���
            // TODO: ��Ԃ��m�F���ANdisMediaStateDisconnected ��Ԃ��悤�ɂ���
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), NdisMediaStateConnected);
            
        case OID_GEN_MAXIMUM_SEND_PACKETS:  // ���̃��N�G�X�g�Ŏ󂯂���p�P�b�g�̍ő吔
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), STE_MAX_SEND_PACKETS);

        // ��ʓI�ȓ��v��� (5��)
        case OID_GEN_XMIT_OK:               // ����ɑ��M�ł����t���[����
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), Adapter->Opackets);

        case OID_GEN_RCV_OK:                // ����Ɏ�M�ł����t���[����
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), Adapter->Ipackets);            

        case OID_GEN_XMIT_ERROR:            // ���M�ł��Ȃ������i�������̓G���[�ɂȂ����j�t���[����
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), Adapter->Oerrors);
            
        case OID_GEN_RCV_ERROR:             // ��M�ł��Ȃ������i�������̓G���[�ɂȂ����j�t���[����
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), Adapter->Ierrors);
            
        case OID_GEN_RCV_NO_BUFFER:         // �o�b�t�@�s���̂��߂Ɏ�M�ł��Ȃ������t���[����
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), Adapter->NoResources);            
            
       // Ethernet �p�̓��� (4��)
        case OID_802_3_PERMANENT_ADDRESS:   // �n�[�h�E�F�A�ɏ�����Ă��� MAC �A�h���X
        case OID_802_3_CURRENT_ADDRESS:     // NIC �̌��݂� MAC �A�h���X            
            SET_INFORMATION_BY_POINTER(ETHERADDRL, Adapter->EthernetAddress);
            
        case OID_802_3_MULTICAST_LIST:     // ���݂̃}���`�L���X�g�p�P�b�g�̃A�h���X���X�g
            // TODO: �}���`�L���X�g���X�g���Z�b�g����B
            // ���̂Ƃ��� 0 ��Ԃ�
            SET_INFORMATION_BY_VALUE(ETHERADDRL, 0);            
            
        case OID_802_3_MAXIMUM_LIST_SIZE:  // NIC �h���C�o���Ǘ��ł���ő�̃}���`�L���X�g�A�h���X�̐�
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), STE_MAX_MCAST_LIST);
            
        // Ethernet �p���v���  (3��)
        case OID_802_3_RCV_ERROR_ALIGNMENT:   // �A���C�����g�G���[�̎�M�t���[����
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), Adapter->AlignErrors);

        case OID_802_3_XMIT_ONE_COLLISION:    // �R���W������ 1 �񔭐��������M�t���[����
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), Adapter->OneCollisions);

        case OID_802_3_XMIT_MORE_COLLISIONS:  // �R���W������ 1 ��ȏ㔭���������M�t���[����
            SET_INFORMATION_BY_VALUE(sizeof(ULONG), Adapter->Collisions);            

        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;                        
    }

    if(Information != NULL) {
        NdisMoveMemory(InformationBuffer, Information, InformationLength);        
        *BytesWritten = InformationLength;
    } else if(InformationLength > 0) {
        // �o�b�t�@���������ꍇ�́A�K�v�ȃT�C�Y��ʒm����B
        *BytesNeeded = InformationLength;
        Status = NDIS_STATUS_BUFFER_TOO_SHORT;
    }
    return(Status);    
}

/***************************************************************************
 * SteMiniportSetInformation()
 *
 *  NDIS �G���g���|�C���g
 *  OID �̒l��₢���킹�邽�߂� NDIS �ɂ���ČĂ΂��B
 * 
 * ����:
 * 
 *     MiniportAdapterContext  :    Adapter �\���̂̃|�C���^
 *     Oid                     :    ���̖₢���킹�� OID
 *     InformationBuffer       :    ���̂��߂̃o�b�t�@�[
 *     InformationBufferLength :    �o�b�t�@�̃T�C�Y
 *     BytesRead               :    �����̏�񂪓ǂ܂ꂽ��
 *     BytesNeeded             :    �o�b�t�@�����Ȃ��ꍇ�ɕK�v�ȃT�C�Y���w��
 * 
 * �Ԃ�l:
 * 
 *     ���펞 : NDIS_STATUS_SUCCESS
 * 
 *******************************************************************************/
NDIS_STATUS
SteMiniportSetInformation(
    IN NDIS_HANDLE  MiniportAdapterContext,
    IN NDIS_OID     Oid,
    IN PVOID        InformationBuffer,
    IN ULONG        InformationBufferLength,
    OUT PULONG      BytesRead,
    OUT PULONG      BytesNeeded)
{
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
    STE_ADAPTER    *Adapter;
    ULONG           NewFilter;

    DEBUG_PRINT0(3, "SteMiniportSetInformation called\n");        

    Adapter   =    (STE_ADAPTER *)MiniportAdapterContext;

    DEBUG_PRINT1(3, "SteMiniportSetInformation: Oid = 0x%x\n", Oid);
    
    // �K�{ OID �̃Z�b�g�v������������
    // TODO:
    // ���̂Ƃ���A�p�P�b�g�t�B���^�[�ȊO�͎��ۂɂ̓Z�b�g���Ă��Ȃ��̂ŁA�Z�b�g�ł���悤�ɂ���B
    //
    switch(Oid) {
        // ��ʓI�ȓ���         
        case OID_GEN_CURRENT_PACKET_FILTER:
            if(InformationBufferLength != sizeof(ULONG)){
                *BytesNeeded = sizeof(ULONG);
                *BytesRead = 0;
                Status = NDIS_STATUS_INVALID_LENGTH;
                break;
            }
            NewFilter = *(ULONG *)InformationBuffer;
            Adapter->PacketFilter = NewFilter;
            *BytesRead = InformationBufferLength;
            DEBUG_PRINT1(3,"SteMiniportSetInformation: New filter = 0x%x (See ntddndis.h)\n", NewFilter);
            break;
        case OID_GEN_CURRENT_LOOKAHEAD:
            *BytesRead = InformationBufferLength;
            break;
        case OID_GEN_PROTOCOL_OPTIONS:
            *BytesRead = InformationBufferLength;
            break;        
        // Ethernet �̓���
        case OID_802_3_MULTICAST_LIST:
            *BytesRead = InformationBufferLength;
            break;            
        default:
            Status = NDIS_STATUS_INVALID_OID;            
            break;
    }
    
    return(Status);
}


/************************************************************************
 * SteMiniportHalt()
 *
 *     NDIS Miniport �G���g���|�C���g
 *     Halt �n���h���� NDIS �� PNP �}�l�[�W������ IRP_MN_STOP_DEVICE�A
 *     IRP_MN_SUPRISE_REMOVE�AIRP_MN_REMOVE_DEVICE �v�����󂯎������
 *     ���ɌĂ΂��BSteMiniportInitialize �Ŋm�ۂ��ꂽ�S�Ẵ��\�[�X
 *     ���������B�i����̃~�j�|�[�g�h���C�o�C���X�^���X�Ɍ��肳���)
 *     
 *     o �S�Ă� I/O ���\�[�X�� free ���Aunmap ����B
 *     o NdisMRegisterAdapterShutdownHandler �ɂ���ēo�^���ꂽ�V���b
 *       �g�_�E���n���h����o�^��������B
 *     o NdisMCancelTimer ���Ă�ŃL���[�C���O����Ă���R�[���o�b�N
 *       ���[�`�����L�����Z������B
 *     o �S�Ă̖������̎�M�p�P�b�g����������I���܂ő҂B
 * 
 * ����:
 *      MiniportAdapterContext	�A�_�v�^�ւ̃|�C���^
 *  
 * �Ԃ�l:
 * 
 *     ����
********************************************************************/
VOID 
SteMiniportHalt(
    IN  NDIS_HANDLE    MiniportAdapterContext
    )
{
    STE_ADAPTER       *Adapter;
    BOOLEAN            bTimerCancelled;
    INT                i;

    DEBUG_PRINT0(3, "SteMiniportHalt called\n");        

    Adapter = (STE_ADAPTER *) MiniportAdapterContext;
    
    SteMiniportShutdown(
        (PVOID) Adapter   //IN PVOID
        );

    //
    // NdisMCancelTimer ���Ă�ŃL���[�C���O����Ă���R�[���o�b�N
    // ���[�`�����L�����Z������B
    //

    // ReceiveIndication �^�C�}�[���L�����Z��
    NdisCancelTimer(
        &Adapter->RecvTimer,  // IN  PNDIS_TIMER
        &bTimerCancelled      // OUT PBOOLEAN  
        );
    // Reset �^�C�}�[���L�����Z��    
    NdisCancelTimer(
        &Adapter->ResetTimer, // IN  PNDIS_TIMER
        &bTimerCancelled      // OUT PBOOLEAN  
        );
    if (bTimerCancelled == TRUE){
        // �L�����Z�����ꂽ�R�[���o�b�N���[�`�����������悤���B
        // ��M�L���[�Ɏc���Ă��� Packet �͂��̌�� SteDeleteAdapter()
        // �ɂ���� Free �����̂ŁA�����ł͉������Ȃ��B
    }

    // NdisMRegisterAdapterShutdownHandler �ɂ���ēo�^���ꂽ
    // �V���b�g�_�E���n���h����o�^��������B    
    NdisMDeregisterAdapterShutdownHandler(
        Adapter->MiniportAdapterHandle // IN NDIS_HANDLE 
        );

    //
    // ���z NIC �f�[��������� IOCT/READ/WRITE �p�̃f�o�C�X�̓o�^����������B
    //
    SteDeregisterDevice(Adapter);

    //
    // �������̎�M�ʒm�ς݃p�P�b�g���Ȃ����ǂ����`�F�b�N����B
    // 1 �b�����ɁASTE_MAX_WAIT_FOR_RECVINDICATE(=5)��m�F���A
    // RecvIndicatedPackets �� 0 �ɂȂ�Ȃ��悤�ł���΁A
    // �Ȃɂ���肪�������ƍl���������ă��\�[�X�̊J���ɐi�ށB
    //
    for ( i = 0 ; i < STE_MAX_WAIT_FOR_RECVINDICATE ; i++){
        if (Adapter->RecvIndicatedPackets == 0) {
            break;
        }
        NdisMSleep(1000);
    }
    
    //
    // Adapter ���폜����B���̂Ȃ��ŁAAdapter �ׂ̈Ɋm�ۂ��ꂽ���\�[�X��
    // �J�����iPacket ��ABuffer�j�s����B
    //
    SteDeleteAdapter(Adapter);

    return;
}

/******************************************************************************
 * SteMiniportReset()
 *
 *    NDIS Miniport �G���g���|�C���g
 *    �ȉ��̏����̂Ƃ� NIC ���n���O���Ă���Ɣ��f���h���C�o�[�̃\�t�g�X�e�[�g
 *    �����Z�b�g���邽�߂ɌĂ΂��B
 *
 *     1) SteMiniportCheckForHang() �� TRUE ��Ԃ��Ă���
 *     2) �������̑��M�p�P�b�g�����o�����i�V���A���C�Y�h�h���C�o�̂݁j
 *     3) ��莞�ԓ��Ɋ������邱�Ƃ��ł��Ȃ������������̗v����������
 *
 *   ���̊֐��̒��ł͈ȉ����s��
 *   
 *     1) �L���[�C���O����Ă��鑗�M�p�P�b�g�̏�������߁ANDIS_STATUS_REQUEST_ABORTED ��Ԃ��B
 *     2) ��ʂɒʒm�����S�Ẵp�P�b�g�����^�[�����Ă������m�F����B
 *     3) ��L�Q�ɖ�肪���胊�Z�b�g�������������Ɋ����ł��Ȃ��ꍇ�AResetTimer ���Z�b�g���A
 *        NDIS_STATUS_PENDING ��Ԃ��B

 * 
 * ����:
 * 
 *    AddressingReset        : �}���`�L���X�g��AMAC �A�h���X�Alookahead �T�C�Y
 *                             �ɕύX���������ꍇ�ɂ͂����� TRUE �ɂ��Ȃ���΂Ȃ�Ȃ��B
 *                             
 *    MiniportAdapterContext : STE_ADAPTER �\���̂̃|�C���^                  
 * 
 * 
 *   �Ԃ�l:
 * 
 *     NDIS_STATUS
 * 
 **************************************************************************/
NDIS_STATUS
SteMiniportReset(
    OUT PBOOLEAN     AddressingReset,
    IN  NDIS_HANDLE  MiniportAdapterContext
    )
{
    NDIS_STATUS        Status;
    STE_ADAPTER       *Adapter;
    NDIS_PACKET       *Packet = NULL;

    DEBUG_PRINT0(3, "SteMiniportReset called\n");

    Adapter = (STE_ADAPTER *)MiniportAdapterContext;

    // MAC �A�h���X�A�}���`�L���X�g�A�h���X�̕ύX�͂Ȃ��I�H�Ƃ��߂��� FALSE ��Ԃ�
    *AddressingReset = FALSE;    

    Status = NDIS_STATUS_REQUEST_ABORTED;

    // ���M�L���[�ɓ����Ă���p�P�b�g����������
    while (SteGetQueue(&Adapter->SendQueue, &Packet) == NDIS_STATUS_SUCCESS) {
        NdisMSendComplete(
            Adapter->MiniportAdapterHandle,  //IN NDIS_HANDLE   
            Packet,                          //IN PNDIS_PACKET  
            Status                           //IN NDIS_STATUS   
            );
    }

    // ��M�L���[�ɓ����Ă���p�P�b�g����������
    while (SteGetQueue(&Adapter->RecvQueue, &Packet) == NDIS_STATUS_SUCCESS) {
        SteFreeRecvPacket(Adapter, Packet);
    }
    // ���ꂪ�I���΁A�S�Ă̎�M�p�P�b�g���t���[����Ă���͂��B
    // ��͏�ʃv���g�R���ɒʒm�ς݂ł܂��߂��Ă��Ă��Ȃ��p�P�b�g�����邩�ǂ�����
    // �m�F����B

    if (Adapter->RecvIndicatedPackets > 0) {
        // �܂��߂��Ă��ĂȂ��p�P�b�g������悤���B
        // ���Z�b�g�^�C�}�[���Z�b�g���āA��ق� SteResetTimerFunc() ����
        // NdisMResetComplete() ���Ă�Ń��Z�b�g���������邱�Ƃɂ���B
        NdisSetTimer(&Adapter->ResetTimer, 300);
        Status = NDIS_STATUS_PENDING;
    } else {
        // ���Z�b�g���슮���I
        Status = NDIS_STATUS_SUCCESS;
    }

    return(Status);
}
 
/***************************************************************************
 * SteMiniportUnload()
 *
 *     NDIS Miniport �G���g���|�C���g
 *     �A���h���[�h�n���h���� DriverEntry �̒��Ŋl�����ꂽ���\�[�X�����
 *     ���邽�߂ɁA�h���C�o�̃A�����[�h���ɌĂ΂��B
 *     ���̃n���h���� NdisMRegisterUnloadHandler ��ʂ��ēo�^�����B
 *
 *     ** ����**
 *     MiniportUnload() �� MiniportHalt() �Ƃ͈Ⴄ�I�I
 *     MiniportUload() �͂��L��̃X�R�[�v�����̂ɑ΂��AMiniportHalt ��
 *     ����� miniport �h���C�o�C���X�^���X�Ɍ��肳���B
 * 
 * ����:
 * 
 *     DriverObject  : �g���ĂȂ�
 * 
 * �Ԃ�l:
 * 
 *     None
 *
 *************************************************************************/
VOID 
SteMiniportUnload(
    IN  PDRIVER_OBJECT      DriverObject
    )
{
    // DriverEntry() �Ń��\�[�X���m�ۂ��Ă��Ȃ��̂�
    // �������Ȃ��Ă悢���̂��H
    DEBUG_PRINT0(3, "SteMiniportUnload called\n");            

    return;
}

/*******************************************************************************
 * SteMiniportCheckForHang()
 *
 *     NDIS Miniport �G���g���|�C���g
 *     NIC �̏�Ԃ�񍐂��邽�߂ɂ�΂�A�܂��f�o�C�X�h���C�o�̔����̗L����
 *     ���j�^�[���邽�߂ɌĂ΂��B
 *     
 * ����:
 * 
 *     MiniportAdapterContext :  �A�_�v�^�̃|�C���^
 * 
 * �Ԃ�l:
 * 
 *     TRUE    NDIS ���h���C�o�� MiniportReset ���Ăяo����
 *     FALSE   ����
 * 
 * Note: 
 *     CheckForHang �n���h���̓^�C�}�[ DPC �̃R���e�L�X�g�ŌĂяo����܂��B
 *     ���̗��_�́Aspinlock ���m��/�������Ƃ��ɓ����܂��B
 * 
 ******************************************************************************/
BOOLEAN
SteMiniportCheckForHang(
    IN NDIS_HANDLE MiniportAdapterContext
    )
{
    // ���܂�ɏ璷�Ȃ̂ŁA�K�v�ȃf�o�b�O���x�����グ��
    DEBUG_PRINT0(4, "SteMiniportCheckForHang called\n");
    
    return(FALSE);
}

/**************************************************************************
 * SteMiniportReturnPacket()   
 *
 *    NDIS Miniport �G���g���|�C���g
 *    ���̃h���C�o����ʃv���g�R���ɒʒm�����p�P�b�g���A�v���g�R���ɂ����
 *    �������I�������ꍇ�� NDIS �ɂ���ČĂ΂��B
 *
 * ����:
 *
 *    MiniportAdapterContext :  ADAPTER �\���̂̃|�C���^
 *    Packet                 :  ���^�[������Ă����p�P�b�g
 *
 * �Ԃ�l:
 *
 *    ����
 *
 **************************************************************************/
VOID 
SteMiniportReturnPacket(
    IN NDIS_HANDLE   MiniportAdapterContext,
    IN PNDIS_PACKET  Packet)
{
    STE_ADAPTER         *Adapter;

    Adapter = (STE_ADAPTER *)MiniportAdapterContext;

    DEBUG_PRINT0(3, "SteMiniportReturnPacket called\n");    

    SteFreeRecvPacket(
        Adapter,  //IN STE_ADAPTER
        Packet    //IN NDIS_PACKET
        );

    NdisInterlockedDecrement(
        (PLONG)&Adapter->RecvIndicatedPackets  //IN PLONG  
    );    
    
    return;
}

/***************************************************************************
 * SteMiniportSendPackets()
 *
 *    NDIS Miniport �G���g���|�C���g
 *    ���̃h���C�o�Ƀo�C���h���Ă���v���g�R�����A�p�P�b�g�𑗐M����ۂ�
 *    NDIS �ɂ���ČĂ΂��B
 *
 * ����:
 *
 *    MiniportAdapterContext :   �A�_�v�^�R���e�L�X�g�ւ̃|�C���^
 *    PacketArray            :   ���M����p�P�b�g�z��
 *    NumberOfPackets        :   ��L�̔z��̒���
 *
 * �Ԃ�l:
 *
 *    ����
 *    
 *************************************************************************/
VOID 
SteMiniportSendPackets(
    IN NDIS_HANDLE       MiniportAdapterContext,
    IN PPNDIS_PACKET     PacketArray,
    IN UINT              NumberOfPackets
    )
{
    UINT            i;
    NDIS_STATUS     Status;
    STE_ADAPTER    *Adapter;
    BOOLEAN         IsStedRegistered = TRUE;

    DEBUG_PRINT0(3, "SteMiniportSendPackets called\n");            

    Adapter = (STE_ADAPTER *) MiniportAdapterContext;

    // ���z NIC �f�[��������̃C�x���g�I�u�W�F�N�g���o�^����Ă��邩
    // TODO: Adapter ���� Lock ���쐬���AEventObject �̎Q�Ǝ���
    // ���b�N���擾���ׂ�
    if( Adapter->EventObject == NULL ) {
        DEBUG_PRINT0(2, "SteMiniportSendPackets: sted is not registerd yet\n");
        IsStedRegistered = FALSE;
    }        

    for( i = 0 ; i < NumberOfPackets ; i++){

        if(DebugLevel >= 3)
            StePrintPacket(PacketArray[i]);
        
        if(IsStedRegistered){
            Status = StePutQueue(&Adapter->SendQueue, PacketArray[i]);
        } else {
            Status = NDIS_STATUS_RESOURCES;
        }
        
        if(Status != NDIS_STATUS_SUCCESS){            
            // ���M�L���[����t�A�������͉��z NIC �f�[���������o�^�̂悤���B
            NdisMSendComplete(
                Adapter->MiniportAdapterHandle,  //IN NDIS_HANDLE   
                PacketArray[i],                  //IN PNDIS_PACKET  
                Status                           //IN NDIS_STATUS
                // NDIS_STATUS_RESOURCES ��Ԃ����ƂɂȂ�B
                );
            Adapter->Oerrors++;
            continue;
        }
        
        Adapter->Opackets++;        

        /*
         * ���z NIC �f�[�����ɃC�x���g�ʒm
         */
        KeSetEvent(Adapter->EventObject, 0, FALSE);
        DEBUG_PRINT0(3, "SteMiniportSendPackets: KeSetEvent Called\n");
    }
    return;
}
