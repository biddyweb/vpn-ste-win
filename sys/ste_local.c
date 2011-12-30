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
/****************************************************************************
 *  ste_local.c
 *
 *    NDIS �̃G���g���|�C���g�ȊO�� Ste.sys �h���C�o�ŗL�̊֐����L�q����Ă���
 *
 *  �ύX����:
 *    2006/05/05
 *      o MAC �A�h���X���h���C�o�[�̃C���X�g�[�����ɓ��I�ɐ�������悤�ɂ����B
 *
 *****************************************************************************/
#include "ste.h"
#include <ntstrsafe.h>

/****************************************************************************
 * SteCreateAdapter()
 * 
 *   STE_ADAPTER �\���̂��m�ۂ��A���̑��A�K�v�ȃ��\�[�X���m�ہA����������B
 *
 * ����:
 *
 *    Adapter : �A�_�v�^�\���̂̃|�C���^�̃A�h���X
 *
 * �߂�l:
 *
 *   ������ : NDIS_STATUS_SUCCESS
 *
***************************************************************************/
NDIS_STATUS
SteCreateAdapter(
    IN OUT STE_ADAPTER    **pAdapter     
    )
{
    STE_ADAPTER      *Adapter = NULL;
    NDIS_STATUS       Status;
    INT                i;
    LARGE_INTEGER     SystemTime;    

    DEBUG_PRINT0(3, "SteCreateAdapter called\n");    

    do {
        //
        // STE_ADAPTER ���m��
        //
        Status = NdisAllocateMemoryWithTag(
            (PVOID)&Adapter,     //OUT PVOID  *
            sizeof(STE_ADAPTER), //IN UINT    
            (ULONG)'CrtA'        //IN ULONG   
            );
        if (Status != NDIS_STATUS_SUCCESS){
            DEBUG_PRINT1(1, "SteCreateAdapter: NdisAllocateMemoryWithTag Failed (0x%x)\n",
                            Status);
            break;
        }

        DEBUG_PRINT1(3, "SteCreateAdapter: Adapter = 0x%p\n", Adapter);
        
        NdisZeroMemory(Adapter, sizeof(STE_ADAPTER));

        //
        // ��M�p�o�b�t�@�[�v�[�����m��
        // 
        NdisAllocateBufferPool(
            &Status,                      //OUT PNDIS_STATUS  
            &Adapter->RxBufferPoolHandle, //OUT PNDIS_HANDLE  
            STE_MAX_RXDS                  //IN UINT           
            );
        if (Status != NDIS_STATUS_SUCCESS){
            DEBUG_PRINT1(1, "SteCreateAdapter: NdisAllocateBufferPool Failed (0x%x)\n",Status);
            break;
        }
        
        //
        // ��M�p�p�P�b�g�v�[�����m��
        //
        NdisAllocatePacketPool(
            &Status,                         //OUT PNDIS_STATUS  
            &Adapter->RxPacketPoolHandle,    //OUT PNDIS_HANDLE  
            STE_MAX_RXDS,                    //IN UINT           
            PROTOCOL_RESERVED_SIZE_IN_PACKET //IN UINT           
            );
        if (Status != NDIS_STATUS_SUCCESS){
            DEBUG_PRINT1(1, "SteCreateAdapter: NdisAllocatePacketPool Failed (0x%x)\n", Status);
            break;
        }            

        // ��M�p�� Queue ��������
        NdisInitializeListHead(&Adapter->RecvQueue.List);
        NdisAllocateSpinLock(&Adapter->RecvQueue.SpinLock);
        Adapter->RecvQueue.QueueMax = STE_QUEUE_MAX;
        Adapter->RecvQueue.QueueCount = 0;
        
        //���M�p�� Queue ��������
        NdisInitializeListHead(&Adapter->SendQueue.List);        
        NdisAllocateSpinLock(&Adapter->SendQueue.SpinLock);
        Adapter->SendQueue.QueueMax = STE_QUEUE_MAX;        
        Adapter->SendQueue.QueueCount = 0;

        // �A�_�v�^�[���̃��b�N��������
        NdisAllocateSpinLock(&Adapter->AdapterSpinLock);
        NdisAllocateSpinLock(&Adapter->SendSpinLock);
        NdisAllocateSpinLock(&Adapter->RecvSpinLock);                        

        // �p�P�b�g�t�B���^�[�̏����l���Z�b�g
        // �s�v�Ȃ悤�ȋC������B
        Adapter->PacketFilter |=
            (NDIS_PACKET_TYPE_ALL_MULTICAST| NDIS_PACKET_TYPE_BROADCAST |NDIS_PACKET_TYPE_DIRECTED);
        
        // �O���[�o���̃A�_�v�^���X�g�ɒǉ�
        NdisAcquireSpinLock(&SteGlobalSpinLock);
        Status = SteInsertAdapterToList(Adapter);
        NdisReleaseSpinLock(&SteGlobalSpinLock);
        if(Status != NDIS_STATUS_SUCCESS){
            DEBUG_PRINT1(1, "SteCreateAdapter: SteInsertAdapterToList Failed (0x%x)\n", Status);
            break;
        }

        //
        // MAC �A�h���X��o�^
        // ���I�N�e�b�g�� U/L �r�b�g�� 1�i=���[�J���A�h���X�j�ɃZ�b�g����B
        // �㔼�� 32 bit �̓h���C�o���C���X�g�[�����ꂽ���Ԃ��猈�߂�B
        //
        KeQuerySystemTime(&SystemTime);
        
        Adapter->EthernetAddress[0] = 0x0A; 
        Adapter->EthernetAddress[1] = 0x00;
        Adapter->EthernetAddress[2] = (UCHAR)((SystemTime.QuadPart >> 24) & 0xff);
        Adapter->EthernetAddress[3] = (UCHAR)((SystemTime.QuadPart >> 16) & 0xff);
        Adapter->EthernetAddress[4] = (UCHAR)((SystemTime.QuadPart >>  8) & 0xff);
        Adapter->EthernetAddress[5] = (UCHAR)((SystemTime.QuadPart + Adapter->Instance) & 0xff);

    } while(FALSE);


    if(Status != NDIS_STATUS_SUCCESS && Adapter != NULL){
        //
        // �G���[���ɂ͊m�ۂ������\�[�X���J������
        // 
        // �o�b�t�@�[�v�[�����J��
        if(Adapter->RxBufferPoolHandle != NULL)
            NdisFreeBufferPool(Adapter->RxBufferPoolHandle);
        // �p�P�b�g�v�[�����J��
        if(Adapter->RxPacketPoolHandle != NULL)
            NdisFreePacketPool(Adapter->RxPacketPoolHandle);
        // STE_ADAPTER �\���̂��J��
        NdisFreeMemory(
            (PVOID)Adapter,      //OUT PVOID  
            sizeof(STE_ADAPTER), //IN UINT    
            0                    //IN UINT
            );
    } else {
        //
        // �������͊m�ۂ����A�_�v�^�[�̃A�h���X���Z�b�g
        //
        *pAdapter = Adapter;
    }
    
    return(Status);
}

/****************************************************************************
 * SteDeleteAdapter()
 * 
 *   STE_ADAPTER �\���̂��폜���A���\�[�X����������B
 *
 * ����:
 *
 *    Adapter : �J������ STE_ADAPTER �\����
 *
 * �߂�l:
 *
 *  ���펞 :  NDIS_STATUS_SUCCESS
 *
***************************************************************************/
NDIS_STATUS
SteDeleteAdapter(
    IN STE_ADAPTER    *Adapter
    )
{
    NDIS_STATUS        Status = NDIS_STATUS_SUCCESS;
    NDIS_PACKET       *Packet = NULL;

    DEBUG_PRINT0(3, "SteDeleteAdapter called\n");        

    // ���M�L���[�ɓ����Ă���p�P�b�g����������
    while (SteGetQueue(&Adapter->SendQueue, &Packet) == NDIS_STATUS_SUCCESS) {
        NdisMSendComplete(
            Adapter->MiniportAdapterHandle,  //IN NDIS_HANDLE   
            Packet,                          //IN PNDIS_PACKET  
            NDIS_STATUS_REQUEST_ABORTED      //IN NDIS_STATUS   
            );
    }

    //  ��M�L���[�ɓ����Ă���p�P�b�g����������
    while (SteGetQueue(&Adapter->RecvQueue, &Packet) == NDIS_STATUS_SUCCESS) {
        SteFreeRecvPacket(Adapter, Packet);
    }
    //  ���ꂪ�I���΁A�S�Ă̎�M�p�P�b�g���t���[����Ă���͂��B
    
    // �o�b�t�@�[�v�[�����J��
    if(Adapter->RxBufferPoolHandle != NULL)
        NdisFreeBufferPool(Adapter->RxBufferPoolHandle);

    // �p�P�b�g�v�[�����J��
    if(Adapter->RxPacketPoolHandle != NULL)
        NdisFreePacketPool(Adapter->RxPacketPoolHandle);

    // �O���[�o���̃A�_�v�^�[�̃��X�g����폜
    NdisAcquireSpinLock(&SteGlobalSpinLock);
    SteRemoveAdapterFromList(Adapter);
    NdisReleaseSpinLock(&SteGlobalSpinLock);    

    //
    // STE_ADAPTER �\���̂��J��
    //
    NdisFreeMemory(
        (PVOID)Adapter,      //OUT PVOID  
        sizeof(STE_ADAPTER), //IN UINT    
        0                    //IN UINT
    );
    
    return(Status);
}

/****************************************************************************
 * SteResetTimerFunc()
 *
 *  ���Z�b�g�����̊������m�F���邽�߂̃^�C�}�[�֐��B
 *  SteMiniportInitialize() �ɂď���������@NdisSetTimer()�@�ɂĎ��Ԃ�����
 *  ��ꂽ���ɌĂяo�����B
 *
 * ����:
 *
 *    Adapter : STE_ADAPTER �\����
 *
 * �߂�l:
 *
 *   ����
 *
***************************************************************************/
VOID 
SteResetTimerFunc(
    IN PVOID  SystemSpecific1,
    IN PVOID  FunctionContext,
    IN PVOID  SystemSpecific2,
    IN PVOID  SystemSpecific3
    )
{
    STE_ADAPTER    *Adapter;
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;

    DEBUG_PRINT0(3, "SteResetTimverFunc called\n");            
    
    Adapter = (STE_ADAPTER *)FunctionContext;

    Adapter->ResetTrys++;

    // ��ʃv���g�R���ɒʒm�ς݂ł܂��߂��Ă��Ă��Ȃ��p�P�b�g�����邩
    // �ǂ������m�F����B

    if (Adapter->RecvIndicatedPackets > 0) {
        if(Adapter->ResetTrys > STE_MAX_WAIT_FOR_RESET){
            // �ő厎�s�񐔂𒴂���
            Status = NDIS_STATUS_FAILURE;
        } else {
            // ��قǍăg���C
            NdisSetTimer(&Adapter->ResetTimer, 300);
            return;
        }
    } else {
        Status = NDIS_STATUS_SUCCESS;
    }

    NdisMResetComplete(
        Adapter->MiniportAdapterHandle,  //IN NDIS_HANDLE  
        Status,                          //IN NDIS_STATUS  
        FALSE                            //IN BOOLEAN      
        );    

    return;
}

/****************************************************************************
 * SteRecvTimerFunc()
 *
 *  ��M�p�P�b�g�L���[�ɗ��܂����p�P�b�g����ʃv���g�R���ɒʒm���邽�߂�
 *  �^�C�}�[�֐��BSteMiniportInitialize() �ɂď��������� NdisSetTimer()
 *  �ɂĎ��Ԃ����߂�ꂽ���ɌĂяo�����B
 *
 * ����:
 *
 *    Adapter : �J������ STE_ADAPTER �\����
 *
 * �߂�l:
 *
 *   ����
 *
***************************************************************************/
VOID 
SteRecvTimerFunc(
    IN PVOID  SystemSpecific1,
    IN PVOID  FunctionContext,
    IN PVOID  SystemSpecific2,
    IN PVOID  SystemSpecific3
    )
{
    STE_ADAPTER *Adapter;
    NDIS_PACKET *Packet = NULL;
    
    Adapter = (STE_ADAPTER *)FunctionContext;

    DEBUG_PRINT0(3, "SteRecvtTimverFunc called\n");                

    while (SteGetQueue(&Adapter->RecvQueue, &Packet) == NDIS_STATUS_SUCCESS){
        /*
         * NdisMIndicateReceivePacket() ���Ă�ŏ�ʃv���g�R���ɑ΂���
         * �p�P�b�g��]������B
         */
        NdisMIndicateReceivePacket(
            Adapter->MiniportAdapterHandle,  // IN NDIS_HANDLE    
            &Packet,                         //IN PPNDIS_PACKET  
            1                                //IN UINT           
            );
    }
    
    return;
}


/****************************************************************************
 * SteRegisterDevice()
 * 
 *   NdisMRegisterDevice ���ĂԎ��ɂ��A�f�o�C�X�I�u�W�F�N�g���쐬���A�e�� 
 *   Dispatch�@���[�`����o�^����B                                               
 *   �쐬���ꂽ�f�o�C�X�I�u�W�F�N�g�͉��z NIC �f�[�����ɂ���ăI�[�v������        
 *   IOCTL �R�}���h�̑��M�AReadFile()�AWriteFile() �ɂ��f�[�^�̑���Ɏg����B 
 *                                                                                
 *  ����:
 *                                                                               
 *      Adapter : STE_ADAPTER �\����
 *                                                                               
 *  �Ԃ�l:                                                                
 *                                                                               
 *      ���펞 : NDIS_STATUS_SUCCESS 
 *
 ****************************************************************************/
NDIS_STATUS
SteRegisterDevice(
    IN STE_ADAPTER    *Adapter
    )
{
    NDIS_STATUS        Status = NDIS_STATUS_SUCCESS;
    NDIS_STRING        DeviceName;
    NDIS_STRING        SymbolicName;
    NDIS_STRING        Instance; // �g���ĂȂ�
    PDRIVER_DISPATCH   MajorFunctions[IRP_MJ_MAXIMUM_FUNCTION+1]; // IOCTL + READ + WRITE
    NDIS_HANDLE        NdisDeviceHandle;
    UCHAR              SteDeviceName[STE_MAX_DEVICE_NAME];
    UCHAR              SteSymbolicName[STE_MAX_DEVICE_NAME];

    DEBUG_PRINT0(3, "SteRegisterDevice called\n");

    //
    // �C���X�^���X���ɈقȂ�f�o�C�X���쐬����悤�ɂ���B
    // �i�����̉��z NIC ���T�|�[�g���邽�߁j
    // TODO: �C���X�^���X�ԍ���1���Ɖ��肵�Ă���B�Ȃ̂ŁA
    //       2���ȏゾ�Ɩ�肪��������B�Ȃɂ��΍���s��Ȃ���΁E�E�E
    //
    Status = RtlStringCbPrintfA(
        SteDeviceName,  //OUT LPSTR  
        15,             //IN size_t  
        "%s%d",STE_DEVICE_NAME, Adapter->Instance   //IN LPCSTR  
        );

    Status = RtlStringCbPrintfA(
        SteSymbolicName, //OUT LPSTR  
        19,              //IN size_t  
        "%s%d",STE_SYMBOLIC_NAME, Adapter->Instance  //IN LPCSTR  
        );        

    NdisInitializeString(
        &DeviceName, //IN OUT PNDIS_STRING
        SteDeviceName //IN PUCHAR
        );
    
    NdisInitializeString(
        &SymbolicName,    //IN OUT PNDIS_STRING  
        SteSymbolicName   //IN PCWSTR            
        );

    NdisZeroMemory(MajorFunctions, sizeof(MajorFunctions));    

    //
    // �f�B�X�p�b�`���[�`��
    //
    MajorFunctions[IRP_MJ_READ]           = SteDispatchRead;
    MajorFunctions[IRP_MJ_WRITE]          = SteDispatchWrite;
    MajorFunctions[IRP_MJ_DEVICE_CONTROL] = SteDispatchIoctl;
    MajorFunctions[IRP_MJ_CREATE]         = SteDispatchOther;
    MajorFunctions[IRP_MJ_CLEANUP]        = SteDispatchOther;
    MajorFunctions[IRP_MJ_CLOSE]          = SteDispatchOther;
        
    Status = NdisMRegisterDevice(
        NdisWrapperHandle,      //IN NDIS_HANDLE   
        &DeviceName,            //IN PNDIS_STRING     
        &SymbolicName,          //IN PNDIS_STRING     
        MajorFunctions,         //IN PDRIVER_DISPATCH 
        &Adapter->DeviceObject, //OUT PDEVICE_OBJECT *
        &Adapter->DeviceHandle  //OUT NDIS_HANDLE *
        );

    /*
     * �t���O���m�F�E�E�E 0x40(DO_DEVICE_HAS_NAME)�������B
     */
    DEBUG_PRINT0(3, "SteRegisterDevice ");         
    if(Adapter->DeviceObject->Flags & DO_BUFFERED_IO){
        DEBUG_PRINT0(3, "DeviceObject has DO_BUFFERED_IO flag\n");
    } else if (Adapter->DeviceObject->Flags & DO_DIRECT_IO){
        DEBUG_PRINT0(3, "DeviceObject has DO_DIRECT_IO flag\n");
    } else {
        DEBUG_PRINT1(3, "DeviceObject has flag 0x%x\n",Adapter->DeviceObject->Flags );
    }
    /*
     * �V�����쐬���ꂽ DeviceObject �̃t���O�� Buffered I/O �̃t���O���Z�b�g����B
     * ����ɂ�� Irp->AssociatedIrp.SystemBuffer �Ƀf�[�^�������Ă���悤�ɂȂ�悤���B
     * �������̃R�s�[���K�v�� Buffered I/O ��� Direct I/O �̂ق��������Ƃ����l��������
     * ���A���̂Ƃ��� Buffered I/O �ɂ��Ă������B�i������₷�����E�E)
     */    
    Adapter->DeviceObject->Flags |= DO_BUFFERED_IO;
    /*
     * �ēx�t���O���m�F
     */
    DEBUG_PRINT0(3, "SteRegisterDevice ");    
    if(Adapter->DeviceObject->Flags & DO_BUFFERED_IO){
        DEBUG_PRINT0(3, "After set flag: DeviceObject has DO_BUFFERED_IO flag\n");
    } else if (Adapter->DeviceObject->Flags & DO_DIRECT_IO){
        DEBUG_PRINT0(3, "After set flag: DeviceObject has DO_DIRECT_IO flag\n");
    } else {
        DEBUG_PRINT1(3, "After set flag: DeviceObject has flag 0x%x\n",Adapter->DeviceObject->Flags );
    }

    DEBUG_PRINT1(3, "SteRegisterDevice returned (0x%d)\n", Status);                    
    return(Status);
}


/****************************************************************************
 * SteDeregisterDevice()
 * 
 *   IOCTL/ReadFile/WiteFile �p�̃f�o�C�X�I�u�W�F�N�g���폜����B
 *
 *  ����:
 *                                                                               
 *      Adapter : STE_ADAPTER �\����
 *                                                                               
 *  �Ԃ�l:                                                                
 *                                                                               
 *      ���펞 : NDIS_STATUS_SUCCESS 
 *
 ****************************************************************************/
NDIS_STATUS
SteDeregisterDevice(
    IN STE_ADAPTER    *Adapter
    )
{
    NDIS_STATUS        Status  = NDIS_STATUS_SUCCESS;

    DEBUG_PRINT0(3, "SteDeregisterDevice called\n");                

    Status = NdisMDeregisterDevice(Adapter->DeviceHandle);
    Adapter->DeviceHandle = NULL;
    
    return(Status);
}

/*********************************************************************
 * SteDispatchRead() 
 *
 * ���̊֐��� Dispatch Read ���[�`���BIRP_MJ_READ �̏����̂��߂ɌĂ΂��B
 * 
 * ����:
 *
 *    DeviceObject - �f�o�C�X�I�u�W�F�N�g�ւ̃|�C���^
 *    Irp      - I/O ���N�G�X�g�p�P�b�g�ւ̃|�C���^(I/O Request Packt)
 *
 * �Ԃ�l:
 *
 *    ���펞   : STATUS_SUCCESS
 *    �G���[�� : STATUS_UNSUCCESSFUL
 *                 
 ******************************************************************************/
NTSTATUS
SteDispatchRead(
    IN PDEVICE_OBJECT           DeviceObject,
    IN PIRP                     Irp
    )
{
    NTSTATUS            NtStatus = STATUS_SUCCESS;
    NDIS_STATUS         Status = NDIS_STATUS_SUCCESS;
    STE_ADAPTER        *Adapter = NULL; // Packet �̑���M���s�� Adapter
    NDIS_PACKET        *Packet = NULL;

    DEBUG_PRINT0(3, "SteDispatchRead called\n");

    //
    // Debug �p�B Buffer �̃t���O���`�F�b�N
    //
    if( DebugLevel >= 3) {
        DEBUG_PRINT0(3, "SteDispatchRead: ");
        if ( Irp->Flags & DO_BUFFERED_IO){
            DEBUG_PRINT1(3, "Irp->Flags(0x%x)==DO_BUFFERED_IO\n",Irp->Flags);
        } else if ( Irp->Flags & DO_DIRECT_IO){
            DEBUG_PRINT1(3, "Irp->Flags(0x%x)==DO_DIRECT_IO\n",Irp->Flags);
        } else {
            DEBUG_PRINT1(3, "Irp->Flags(0x%x)\n",Irp->Flags);
        }
    }    


    //
    // �r���� break ���邽�߂����� do-while ��
    // 
    do {    
        //
        // �O���[�o�����b�N���擾���A�A�_�v�^�[�̃��X�g���� ������
        // �n���ꂽ DeviceObject ���܂ރA�_�v�^�[��������
        //
        NdisAcquireSpinLock(&SteGlobalSpinLock);
        Status =  SteFindAdapterByDeviceObject(DeviceObject, &Adapter);
        NdisReleaseSpinLock(&SteGlobalSpinLock);
        
        if ( Status != NDIS_STATUS_SUCCESS){
            DEBUG_PRINT0(1, "SteDispatchRead: Can't find Adapter\n");
            break;
        }

        // ���M�L���[����p�P�b�g���擾
        Status = SteGetQueue(&Adapter->SendQueue, &Packet);
        if ( Status != NDIS_STATUS_SUCCESS){        
            DEBUG_PRINT0(2, "SteDispatchRead: No packet found\n");
            break;
        }

        // �擾�����p�P�b�g�̒��g��\��
        if(DebugLevel >= 3)
            StePrintPacket(Packet);                
        
        // �f�[�^�� Irp �ɃR�s�[����
        Status = SteCopyPacketToIrp(Packet, Irp);
        if(Status != NDIS_STATUS_SUCCESS){
            DEBUG_PRINT0(1, "SteDispatchRead: SteCopyPacketToIrp failed\n");  
            break;
        }             

        // NDIS �� SendComplete ��Ԃ�
        Status = NDIS_STATUS_SUCCESS;
        NdisMSendComplete(
            Adapter->MiniportAdapterHandle,  //IN NDIS_HANDLE   
            Packet,                          //IN PNDIS_PACKET  
            Status                           //IN NDIS_STATUS   
            );
        
    } while (FALSE);

    if(Status != NDIS_STATUS_SUCCESS){
        // �G���[�̏ꍇ IoStatus.Information �� 0 ���Z�b�g
        NtStatus = STATUS_UNSUCCESSFUL;
        Irp->IoStatus.Information = 0;
    }

    Irp->IoStatus.Status = NtStatus;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return(NtStatus);
}  

/*********************************************************************
 * SteDispatchWrite()
 *  
 *  Dispatch Write ���[�`���BIRP_MJ_WRITE �̏����̂��߂ɌĂ΂��B
 *  ���̃��[�`���� sted ����󂯂Ƃ������C�[�T�l�b�g�t���[���̒��g��
 *  �R�s�[���A��M�p�P�b�g�𓾁A�o�b�t�@�Ƃ��̎�M�p�P�b�g�Ɋ֘A�t�����s���A
 *  �p�P�b�g���A�_�v�^�[�C���X�^���X�̎�M�L���[�ɓ����B
 *  �܂��A��قǃp�P�b�g����ʃv���g�R���ɒʒm�����悤�ɁA�^�C�}�[���Z�b�g�B
 *
 * ����:
 *
 *    DeviceObject :  �f�o�C�X�I�u�W�F�N�g�ւ̃|�C���^
 *    Irp          :  I/O ���N�G�X�g�p�P�b�g�ւ̃|�C���^(I/O Request Packt)
 *
 * �Ԃ�l:
 *
 *    ���펞   : STATUS_SUCCESS
 *    �G���[�� : STATUS_UNSUCCESSFUL
 *                 
 ******************************************************************************/
NTSTATUS
SteDispatchWrite(
    IN PDEVICE_OBJECT           DeviceObject,
    IN PIRP                     Irp
    )
{
    NDIS_STATUS            Status   = NDIS_STATUS_SUCCESS;
    NTSTATUS               NtStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION     *irpStack;
    STE_ADAPTER           *Adapter  = NULL;// Packet �̑���M���s�� Adapter
    NDIS_PACKET           *RecvPacket   = NULL;
    NDIS_BUFFER           *NdisBuffer;
    UINT                   NdisBufferLen;
    VOID                  *IrpRecvBuffer;    // IRP �� buffer 
    ULONG                  IrpRecvBufferLen; // IRP �� buffer �̒���
    VOID                  *VirtualAddress = NULL; // IRP �� Buffer �̃f�[�^�̃A�h���X
    
    DEBUG_PRINT0(3, "SteDispatchWrite called\n");        

    // IRP ���� IO_STACK_LOCATION �𓾂�
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    // IRP ����󂯎�����f�[�^�ƁA�f�[�^���𓾂�
    IrpRecvBuffer = Irp->AssociatedIrp.SystemBuffer;
    IrpRecvBufferLen = irpStack->Parameters.Write.Length;

    //
    // �r���� break �ł���悤�ɂ��邽�߂����� do-while ��
    //
    do {
        //
        // IRP �̃o�b�t�@�[���`�F�b�N
        //
        if(IrpRecvBufferLen == 0 ){
            DEBUG_PRINT0(2, "SteDispatchWrite: IrpRecvBufferLen == 0\n");
            Status = NDIS_STATUS_FAILURE;
            break;
        }
        if( IrpRecvBuffer == NULL){
            DEBUG_PRINT0(2, "SteDispatchWrite: IrpRecvBuffer is NULL\n");
            Status = NDIS_STATUS_FAILURE;
            break;
        }
        
        //
        // �O���[�o�����b�N���擾���AAdapter �̃��X�g���� ������
        // �n���ꂽ DeviceObject ���܂� Adapter ��������
        //
        NdisAcquireSpinLock(&SteGlobalSpinLock);
        Status =  SteFindAdapterByDeviceObject(DeviceObject, &Adapter);
        NdisReleaseSpinLock(&SteGlobalSpinLock);
        
        if ( Status != NDIS_STATUS_SUCCESS){
            DEBUG_PRINT0(1, "SteDispatchWrite: Can't find Adapter\n");
            break;
        }

        //
        // Debug �p�B Buffer �̃t���O���`�F�b�N
        //
        if( DebugLevel >= 3) {
            DEBUG_PRINT0(3, "SteDispatchWrite: ");
            if ( Irp->Flags & DO_BUFFERED_IO){
                DEBUG_PRINT1(3, "Irp->Flags(0x%x)== DO_BUFFERED_IO\n",Irp->Flags);
            } else if ( Irp->Flags & DO_DIRECT_IO){
                DEBUG_PRINT1(3, "Irp->Flags(0x%x)== DO_DIRECT_IO\n",Irp->Flags);
            } else {
                DEBUG_PRINT1(3, "Irp->Flags(0x%x)n",Irp->Flags);
            }
        }
        
        DEBUG_PRINT1(3, "SteDispatchWrite: IrpRecvBufferLen = %d\n",IrpRecvBufferLen);
        
        //
        //  �A�_�v�^�[���̃��b�N���擾���A
        //  �p�P�b�g�v�[�������M�p�̃p�P�b�g���m�ۂ���
        //
        NdisAcquireSpinLock(&Adapter->AdapterSpinLock);                        
        Status = SteAllocateRecvPacket(Adapter, &RecvPacket);
        NdisReleaseSpinLock(&Adapter->AdapterSpinLock);
        
        if(Status != NDIS_STATUS_SUCCESS){
            DEBUG_PRINT0(2, "SteDispatchWrite: SteAllocateRecvPacket failed\n");
            Adapter->NoResources++;
            break;
        }
        
        DEBUG_PRINT1(3, "SteDispatchWrite: RecvPacket = %p\n", RecvPacket);        


        // Packet �� Status �� NDIS_STATUS_SUCCESS �ɃZ�b�g
        NDIS_SET_PACKET_STATUS(RecvPacket, NDIS_STATUS_SUCCESS);

        //
        // �m�ۂ��� Packet ���� NDIS_BUFFER �\���̂̃|�C���^�ƃC�[�T�l�b�g
        // �t���[�����i�[���邽�߂̃������̃A�h���X�𓾂�
        //
        NdisQueryPacket(
            RecvPacket,           //IN  PNDIS_PACKET   
            NULL,                 //OUT PUINT OPTIONAL
            NULL,                 //OUT PUINT         
            &NdisBuffer,          //OUT PNDIS_BUFFER *
            NULL                  //OUT PUINT         
            );

        // Debug �p
        if(NdisBuffer == NULL){
            DEBUG_PRINT0(2, "SteDispatchWrite: NdisBuffer is NULL.\n");
            Status = NDIS_STATUS_FAILURE;
            break;
        } else {
            DEBUG_PRINT1(3, "SteDispatchWrite: NdisBuffer is %p\n", NdisBuffer);
        }
            
        NdisQueryBufferSafe(
            NdisBuffer,           //IN PNDIS_BUFFER    
            &VirtualAddress,      //OUT PVOID *
            &NdisBufferLen,       //OUT PUINT          
            NormalPagePriority    //IN MM_PAGE_PRIORITY
            );

        // NdisBuffer �ɂ� ETHERMAX(1514bytes)�ȏ�̓R�s�[�ł��Ȃ�
        if ( IrpRecvBufferLen > ETHERMAX){
            IrpRecvBufferLen = ETHERMAX;
        }

        //
        // IRP �̓��̃f�[�^�� Packet �ɃR�s�[����B
        //
        NdisMoveMemory(VirtualAddress, IrpRecvBuffer, IrpRecvBufferLen);

        // ���ۂɏ������񂾃T�C�Y�� Irp �ɃZ�b�g 
        Irp->IoStatus.Information = IrpRecvBufferLen;
            
        // Buffer �� Next �|�C���^�[�� NULL �ցB
        NdisBuffer->Next = NULL;
        // BufferLength ���Z�b�g
        NdisAdjustBufferLength(NdisBuffer, IrpRecvBufferLen);
        
        DEBUG_PRINT0(3, "SteDispatchWrite: Copying buffer data Completed\n");            

        //
        // TODO:
        // �p�P�b�g�t�B���^�[�̃`�F�b�N�ł���悤�ɂ���B
        // ���͂Ȃ�ł��i���Ƃ�����������Ȃ��Ă��j��ɏグ��B
        //

        // �R�s�[�����p�P�b�g�̒��g��\��
        if(DebugLevel >= 3)
            StePrintPacket(RecvPacket);        

        //   
        // �p�P�b�g����M�L���[�ɃL���[�C���O
        //
        Status = StePutQueue(&Adapter->RecvQueue, RecvPacket);
        if(Status != NDIS_STATUS_SUCCESS){            
            // ��M�L���[����t�̂悤���B
            DEBUG_PRINT0(2, "SteDispatchWrite: Receive Queue is Full.\n");
            break;
        }        
        
        NdisInterlockedIncrement(&Adapter->RecvIndicatedPackets);
        Adapter->Ipackets++;
        DEBUG_PRINT0(3, "SteDispatchWrite: StePutQueue Completed\n");
        //
        // SteRecvTimerFunc() ���Ăяo�����߂̃^�C�}�[���Z�b�g
        //
        NdisSetTimer(&Adapter->RecvTimer, 0);

        DEBUG_PRINT0(3, "SteDispatchWrite: NdisSetTimer Completed\n");  
    } while (FALSE);

    if(Status != NDIS_STATUS_SUCCESS){
        NtStatus = STATUS_UNSUCCESSFUL;
        Adapter->Ierrors++;
        // �������݃T�C�Y�� 0 �ɃZ�b�g
        Irp->IoStatus.Information = 0;
        // Packet ���擾�ς݂̏ꍇ�͊J������B
        if (RecvPacket != NULL){
            SteFreeRecvPacket(Adapter, RecvPacket);
        }
    }

    Irp->IoStatus.Status = NtStatus; 
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return(NtStatus);
 }  

/***********************************************************************
 * SteDispatchIoctl()
 *
 *    ���̃f�o�C�X�ɑ���ꂽ IOCTL �R�}���h����������
 *
 * ����:
 *
 *    DeviceObject :  �f�o�C�X�I�u�W�F�N�g�ւ̃|�C���^
 *    Irp          :  I/O ���N�G�X�g�p�P�b�g�ւ̃|�C���^(I/O Request Packt)
 *
 * �Ԃ�l:
 *
 *    ������ : STATUS_SUCCESS�@
 *    ���s�� : STATUS_UNSUCCESSFUL
 *
 *********************************************************************/
NTSTATUS
SteDispatchIoctl(
    IN PDEVICE_OBJECT           DeviceObject,
    IN PIRP                     Irp
    )
{

    NTSTATUS             NtStatus = STATUS_SUCCESS;
    NDIS_STATUS          Status   = NDIS_STATUS_SUCCESS;
    STE_ADAPTER         *Adapter  = NULL; // Packet �̑���M���s�� Adapter     
    IO_STACK_LOCATION   *irpStack = NULL;
    HANDLE               hEvent;          // sted.exe ����n�����C�x���g�n���h��

    DEBUG_PRINT0(3, "SteDispatchIoctl called\n");
        
    // IRP ���� IO_STACK_LOCATION �𓾂�
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    // break ���邽�߂����� do-while ��
    do {
    
        //
        // �O���[�o�����b�N���擾���AAdapter �̃��X�g���� ������
        // �n���ꂽ DeviceObject ���܂� Adapter ��������
        //
        NdisAcquireSpinLock(&SteGlobalSpinLock);
        Status = SteFindAdapterByDeviceObject(DeviceObject, &Adapter);
        NdisReleaseSpinLock(&SteGlobalSpinLock);    

        if ( Status != NDIS_STATUS_SUCCESS){
            DEBUG_PRINT0(1, "SteDispatchIoctl: Can't find Adapter\n");
            break;
        }    

        switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {
            //
            // ���z NIC �f�[��������̃C�x���g�I�u�W�F�N�g���󂯎��
            // �A�_�v�^�[�\���̂ɕۑ�����B
            //
            case REGSVC:
                DEBUG_PRINT0(3, "SteDispatchIoctl: Received REGSVC IOCTL\n");                  
                hEvent = (HANDLE) irpStack->Parameters.DeviceIoControl.Type3InputBuffer;

                if(hEvent == NULL){
                    DEBUG_PRINT0(1, "SteDispatchIoctl: hEvent is NULL\n");
                    Status = NDIS_STATUS_FAILURE;
                    break;
                }
                DEBUG_PRINT1(3,"SteDispatchIoctl: hEvent = 0x%p\n", hEvent);
                DEBUG_PRINT1(3,"SteDispatchIoctl: Adapter = 0x%p\n", Adapter);            
                DEBUG_PRINT1(3,"SteDispatchIoctl: &Adapter->EventObject = 0x%p\n",
                               &Adapter->EventObject);

                //
                // STE_ADAPTER �\���̂� EventObject ��ύX����̂ŁA
                // �A�_�v�^�[���̃��b�N���擾����
                //
                NdisAcquireSpinLock(&Adapter->AdapterSpinLock);                
                NtStatus = ObReferenceObjectByHandle(
                    hEvent,                 //IN HANDLE                       
                    GENERIC_ALL,            //IN ACCESS_MASK                  
                    NULL,                   //IN POBJECT_TYPE                 
                    KernelMode,             //IN KPROCESSOR_MODE              
                    &Adapter->EventObject,  //OUT PVOID  *                    
                    NULL                    //OUT POBJECT_HANDLE_INFORMATION  
                    );

                NdisReleaseSpinLock(&Adapter->AdapterSpinLock);
            
                if(NtStatus != STATUS_SUCCESS){
                    DEBUG_PRINT1(1, "SteDispatchIoctl: ObReferenceObjectByHandle failed(%d) ",
                                    NtStatus);
                    switch(NtStatus){
                        case STATUS_OBJECT_TYPE_MISMATCH:
                            DEBUG_PRINT0(1," Failed STATUS_OBJECT_TYPE_MISMATCH\n");
                            break;
                        case STATUS_ACCESS_DENIED:
                            DEBUG_PRINT0(1," Failed STATUS_ACCESS_DENIED\n");
                            break;
                        case STATUS_INVALID_HANDLE:
                            DEBUG_PRINT0(1," Failed STATUS_INVALID_HANDLE\n");
                            break;
                        default:
                            DEBUG_PRINT1(1," Failed (0x%x)\n", NtStatus);
                            break;                        
                    }
                    Status = NDIS_STATUS_FAILURE;
                    break;
                }
                break;
            //
            // �C�x���g�I�u�W�F�N�g�̓o�^��������
            //
            case UNREGSVC:
                DEBUG_PRINT0(3, "SteDispatchIoctl: Received UNREGSVC IOCTL\n");
                NdisAcquireSpinLock(&Adapter->AdapterSpinLock);                                
                if(Adapter->EventObject){
                    ObDereferenceObject(Adapter->EventObject);
                    Adapter->EventObject = NULL;
                }
                NdisReleaseSpinLock(&Adapter->AdapterSpinLock);                
                break;
            default:
                DEBUG_PRINT1(3, "SteDispatchIoctl: Received Unknown(0x%x)IOCTL\n",
                                irpStack->Parameters.DeviceIoControl.IoControlCode);             
                NtStatus = STATUS_UNSUCCESSFUL;
                break;
        }
    } while(FALSE);

    if(Status != NDIS_STATUS_SUCCESS){
        // �G���[�̏ꍇ
        NtStatus = STATUS_UNSUCCESSFUL;
    }    
    Irp->IoStatus.Information = 0;    
    Irp->IoStatus.Status = NtStatus; 
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return(NtStatus);
}

/***********************************************************************
 * SteDispatchOther()
 *
 *    ReadFile, WriteFile, IOCTL �ȊO�� I/O ���N�G�X�g����������B
 *
 * ����:
 *
 *    DeviceObject :  �f�o�C�X�I�u�W�F�N�g�ւ̃|�C���^
 *    Irp          :  I/O ���N�G�X�g�p�P�b�g�ւ̃|�C���^(I/O Request Packt)
 *
 * �Ԃ�l:
 *
 *    ��Ɂ@STATUS_SUCCESS
 *
 *********************************************************************/
NTSTATUS
SteDispatchOther(
    IN PDEVICE_OBJECT           DeviceObject,
    IN PIRP                     Irp
    )
{
    NTSTATUS             NtStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION   *irpStack = NULL;

    DEBUG_PRINT0(3, "SteDispatchOther called\n");

    /* IRP ���� IO_STACK_LOCATION �𓾂�*/
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    DEBUG_PRINT0(3, "SteDispatchOther: MajorFunction = ");                    
    switch (irpStack->MajorFunction){
        case IRP_MJ_CREATE:
            DEBUG_PRINT0(3, "IRP_MJ_CREATE\n");                
            break;
        case IRP_MJ_CLEANUP:
            DEBUG_PRINT0(3, "IRP_MJ_CLEANUP\n");            
            break;
        case IRP_MJ_CLOSE:
            DEBUG_PRINT0(3, "IRP_MJ_CLOSE\n");            
            break;
        default:
            DEBUG_PRINT1(3, "0x%x (See ndis.h)\n", irpStack->MajorFunction);
            break;
    }

    Irp->IoStatus.Status = NtStatus;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return(NtStatus);
}


/***********************************************************************
 * SteAllocateRecvPacket()
 *
 *    ��M�p�P�b�g�v�[������p�P�b�g���m�ۂ���B
 *
 * ����:
 *
 *        Adapter : STE_ADAPTER �\���̂̃|�C���^
 *        Packet  : NIDS_PACKET �\���̂̃|�C���^�̃A�h���X
 *
 * �Ԃ�l:
 *
 *    ���펞   : NDIS_STATUS_SUCCESS
 *    �G���[�� : NDIS_STATUS_RESOURCES
 *
 *********************************************************************/
NDIS_STATUS
SteAllocateRecvPacket(
    IN STE_ADAPTER      *Adapter,
    IN OUT NDIS_PACKET  **pPacket
    )
{
    NDIS_STATUS       Status;    
    NDIS_BUFFER      *Buffer   = NULL;
    PVOID             VirtualAddress = NULL;

    DEBUG_PRINT0(3, "SteAllocateRecvPacket called\n");                

    *pPacket = NULL;
    
    //
    // �e�p�P�b�g�Ƀo�b�t�@�[���֘A�t���A�܂��A�m�ۂ���
    // �e��M�f�B�X�N���v�^�Ƀp�P�b�g���֘A�t����B
    //
    do {
        NdisAllocatePacket(
            &Status,                     //OUT PNDIS_STATUS  
            pPacket,                     //OUT PNDIS_PACKET  
            Adapter->RxPacketPoolHandle  //IN NDIS_HANDLE    
            );
        if (Status != NDIS_STATUS_SUCCESS)
            break;

        NDIS_SET_PACKET_HEADER_SIZE(*pPacket, ETHERMAX-ETHERMTU);        
        
        //
        // Ethernet �t���[���̊i�[�p���������m��
        //
        Status = NdisAllocateMemoryWithTag(
            (PVOID)&VirtualAddress,   //OUT PVOID  *
            ETHERMAX,                 //IN UINT    
            (ULONG)'AlcE'             //IN ULONG   
            );
        if (Status != NDIS_STATUS_SUCCESS){
            NdisFreePacket(*pPacket);
            break;
        }
        NdisZeroMemory(VirtualAddress, ETHERMAX);

        // �o�b�t�@�[�v�[������A�o�b�t�@�[�����o��
        NdisAllocateBuffer(
            &Status,                      //OUT PNDIS_STATUS  
            &Buffer,                      //OUT PNDIS_BUFFER
            Adapter->RxBufferPoolHandle,  //IN NDIS_HANDLE
            VirtualAddress,               //IN PVOID
            ETHERMAX                      //IN UINT
            );
        if (Status != NDIS_STATUS_SUCCESS){
            NdisFreePacket(*pPacket);
            NdisFreeMemory(VirtualAddress, ETHERMAX, 0);
            break;
        }
        //
        // �o�b�t�@�[�ƁA�p�P�b�g���֘A�t����
        // 
        NdisChainBufferAtBack(*pPacket, Buffer);

    } while(FALSE);

    return(Status);
}

/***********************************************************************
 * SteFreeRecvPacket()
 *
 *    ��M�p�P�b�g�v�[���Ƀp�P�b�g�����ǂ�
 *
 * ����:
 *
 *        Adapter : STE_ADAPTER �\���̂̃|�C���^
 *        Packet  : �J������ NDIS_PACKET �\����
 *
 * �Ԃ�l:
 *
 *     NDIS_STATUS_SUCCESS
 *
 *********************************************************************/
NDIS_STATUS
SteFreeRecvPacket(
    IN STE_ADAPTER   *Adapter,
    IN NDIS_PACKET   *Packet
    )
{
    NDIS_STATUS       Status = NDIS_STATUS_SUCCESS;    
    NDIS_BUFFER      *Buffer = NULL;
    VOID             *VirtualAddress = NULL;
    UINT              Length;

    DEBUG_PRINT0(3, "SteFreeRecvPacket called\n");     

    NdisQueryPacket(
        Packet,               //IN PNDIS_PACKET   
        NULL,                 //OUT PUINT OPTIONAL
        NULL,                 //OUT PUINT         
        &Buffer,              //OUT PNDIS_BUFFER  
        NULL                  //OUT PUINT         
        );

    NdisQueryBufferSafe(
        Buffer,               //IN PNDIS_BUFFER    
        &VirtualAddress,      //OUT PVOID *
        &Length,              //OUT PUINT          
        NormalPagePriority    //IN MM_PAGE_PRIORITY
        );
        
    // Ethernet �t���[���̊i�[�p���������J��
    NdisFreeMemory(VirtualAddress, ETHERMAX, 0);
        
    // �o�b�t�@�[�v�[���Ƀo�b�t�@�[��߂�
    NdisFreeBuffer(Buffer);

    // �p�P�b�g�v�[���Ƀp�P�b�g��߂�
    NdisFreePacket(Packet);

    return(Status);
}

/***********************************************************************
 * StePutQueue()
 *
 *    �p�P�b�g���L���[�ɓ����B
 *
 * ����:
 *
 *        Queue      : Queue �\���̂̃|�C���^
 *        Packet     : �L���[�ɓ����p�P�b�g
 *
 * �Ԃ�l:
 *
 *     ������ : NDIS_STATUS_SUCCESS
 *     ���s�� : NDIS_STATUS_RESOURCES
 *
 *********************************************************************/
NDIS_STATUS
StePutQueue(
    IN STE_QUEUE     *Queue,    
    IN NDIS_PACKET   *Packet
    )
{
    NDIS_STATUS      Status = NDIS_STATUS_SUCCESS;

    DEBUG_PRINT0(3, "StePutQueue called\n");         

    //
    // �L���[�ɐݒ肵�Ă���ێ��ł���p�P�b�g�̍ő吔���r����B
    // Queue->QueueMax �Q�Ǝ��Ƀ��b�N�͂Ƃ�Ȃ��̂ł����܂Ŗڈ��B
    //
    if( Queue->QueueCount >= Queue->QueueMax){
        // �����ő吔���ȏ�L���[�C���O����Ă��܂��Ă���悤���B
        DEBUG_PRINT2(2, "StePutQueue: QFULL (%d >= %d)\n", Queue->QueueCount, Queue->QueueMax);
        Status = NDIS_STATUS_RESOURCES;
        DEBUG_PRINT0(3, "StePutQueue returned\n");
        return(Status);
    }

    // ���X�g�̍Ō�ɑ}��
    NdisInterlockedInsertTailList(
        &Queue->List,                           //IN PLIST_ENTRY      
        (PLIST_ENTRY)&Packet->MiniportReserved, //IN PLIST_ENTRY
        &Queue->SpinLock                        //IN PNDIS_SPIN_LOCK  
        );

   // �L���[�̃J�E���g�l�𑝂₷
    NdisInterlockedIncrement(&Queue->QueueCount);
    DEBUG_PRINT1(3, "StePutQueue: QueueCount = %d\n", Queue->QueueCount);    

    DEBUG_PRINT0(3, "StePutQueue returned\n");
    return(Status);
}

/***********************************************************************
 * SteGetQueue()
 *
 *    �L���[����p�P�b�g�����o��
 *
 * ����:
 *
 *        Queue      : Queue �\���̂̃|�C���^
 *        Packet     : �L���[������o���p�P�b�g�̃|�C���^�̃A�h���X
 *
 * �Ԃ�l:
 *
 *     ������ : NDIS_STATUS_SUCCESS
 *     ���s�� : NDIS_STATUS_RESOURCES
 *
 *********************************************************************/
NDIS_STATUS
SteGetQueue(
    IN STE_QUEUE         *Queue,    
    IN OUT NDIS_PACKET   **pPacket
    )
{
    NDIS_STATUS       Status = NDIS_STATUS_SUCCESS;
    LIST_ENTRY       *ListEntry;
    NDIS_SPIN_LOCK   *SpinLock;

    DEBUG_PRINT0(3, "SteGetQueue called\n");             

    ListEntry = NdisInterlockedRemoveHeadList(
        &Queue->List,          //IN PLIST_ENTRY      
        &Queue->SpinLock       //IN PNDIS_SPIN_LOCK  
        );

    if (ListEntry == NULL){
        // Packet �̓L���[�C���O����Ă��Ȃ�
        DEBUG_PRINT0(2, "SteGetQueue: No queued packet\n");
        Status = NDIS_STATUS_RESOURCES;
        *pPacket = NULL;
        return(Status);
    }
    
    *pPacket = CONTAINING_RECORD(ListEntry, NDIS_PACKET, MiniportReserved);

    // �L���[�̃J�E���g�l�����炷
    NdisInterlockedDecrement(&Queue->QueueCount);

    return(Status);
}


/*****************************************************************************
 * SteInsertAdapterToList()
 *
 * STE_ADAPTER �\���̂��A�_�v�^�[�̃����N���X�g�ɒǉ�����B
 *
 *  �����F
 *          Adapter : STE_ADAPTER �\����
 * 
 * �߂�l�F
 *          ����: NDIS_STATUS_SUCCESS
 *          ���s: 
 *****************************************************************************/
NDIS_STATUS
SteInsertAdapterToList(
    IN STE_ADAPTER  *Adapter
    )
{
    NDIS_STATUS    Status = NDIS_STATUS_SUCCESS;
    STE_ADAPTER   *pAdapter;
    ULONG          Instance = 0;

    DEBUG_PRINT0(3, "SteInsertAdapterToList called\n");

    // SteCreateAdapter �ɂ� NULL �ɏ���������Ă���͂������O�̂���
    Adapter->NextAdapter = NULL;

    // break ���邽�߂����� do-while ��
    do {
        if (SteAdapterHead == NULL){
            // �ŏ��̃A�_�v�^�[�Ȃ̂ŁA�C���X�^���X�ԍ��� 0�B
            Adapter->Instance = Instance; 
            SteAdapterHead = Adapter;
            break;
        }
        //
        // �o�^����Ă���Ō�̃A�_�v�^�[��T���A�V�����A�_�v�^�[��
        // �C���X�^���X�ԍ������肷��B
        //
        pAdapter = SteAdapterHead;
        while(pAdapter->NextAdapter != NULL){
            pAdapter = pAdapter->NextAdapter;
        }
        // �C���X�^���X�ԍ��͍Ō�̃A�_�v�^�[�̎��̐���
        Adapter->Instance = pAdapter->Instance + 1;
        // �ő�C���X�^���X���� 10�B�Ȃ̂ŁA�C���X�^���X�ԍ��� 9 �ȏ�͎��Ȃ��E�E
        if(Adapter->Instance > STE_MAX_ADAPTERS - 1){
            Status = NDIS_STATUS_RESOURCES;
            break;
        }
        // �A�_�v�^�[���X�g�̍Ō�ɒǉ� 
        pAdapter->NextAdapter = Adapter;
    } while(FALSE);
    
    DEBUG_PRINT1(3, "SteInsertAdapterToList: New Adapter's instance is %d\n", Adapter->Instance);

    return(Status);
}

/*****************************************************************************
 * SteRemoveAdapterFromList()
 *
 * STE_ADAPTER �\���̂��A�_�v�^�[�̃����N���X�g�����菜��
 *
 *  �����F
 *          Adapter : STE_ADAPTER �\����
 * 
 * �߂�l�F
 *          ����: NDIS_STATUS_SUCCESS
 *          ���s:  
 *****************************************************************************/
NDIS_STATUS
SteRemoveAdapterFromList(
    IN STE_ADAPTER    *Adapter
    )
{  
    NDIS_STATUS    Status = NDIS_STATUS_SUCCESS;
    STE_ADAPTER   *pAdapter, *PreviousAdapter;

    DEBUG_PRINT0(3, "SteRemoveAdapterFromList called\n");

    if (SteAdapterHead == NULL){
        // �܂��ЂƂ��A�_�v�^���o�^����Ă��Ȃ�
        DEBUG_PRINT0(1, "SteRemoveAdapterFromList: No Adapter exist\n");
        return(NDIS_STATUS_FAILURE);
    }

    pAdapter = SteAdapterHead;
    PreviousAdapter = (STE_ADAPTER *)NULL;
    do{
        if (pAdapter == Adapter){
            if (PreviousAdapter == NULL)
                SteAdapterHead = (STE_ADAPTER *)NULL;
            else
                PreviousAdapter->NextAdapter = pAdapter->NextAdapter;

            return(Status);
        }
        PreviousAdapter = pAdapter;
        pAdapter = pAdapter->NextAdapter;
    } while(pAdapter);

    // �Y������A�_�v�^�����݂��Ȃ�����
    DEBUG_PRINT0(1, "SteRemoveAdapterFromList: Adapter not found\n");            
    return(NDIS_STATUS_FAILURE);
}

/*****************************************************************************
 * SteFindAdapterByDeviceObject()
 *
 *  �^����ꂽ�f�o�C�X�I�u�W�F�N�g����A�_�v�^�[��������
 *
 *  �����F
 *          DeviceObject: �f�o�C�X�I�u�W�F�N�g�̃|�C���^
 *          Adapter     : �������� STE_ADAPTER �\���̂̃|�C���^
 * 
 * �߂�l�F
 *          ����: NDIS_STATUS_SUCCESS
 *          ���s: NDIS_STATUS_FAILURE
 * 
 *****************************************************************************/
NDIS_STATUS
SteFindAdapterByDeviceObject(
    IN      DEVICE_OBJECT    *DeviceObject,
    IN OUT  STE_ADAPTER      **pAdapter
    )
{ 
    NDIS_STATUS    Status = NDIS_STATUS_SUCCESS;
    STE_ADAPTER   *TempAdapter;

    DEBUG_PRINT0(3, "SteFindAdapterByDeviceObject called\n");
    
    if (SteAdapterHead == NULL){
        // �܂��ЂƂ��A�_�v�^���o�^����Ă��Ȃ�
        DEBUG_PRINT0(1, "SteFindAdapterByDeviceObject: No Adapter exist\n");
        return(NDIS_STATUS_FAILURE);
    }

    TempAdapter = SteAdapterHead;    

    while (TempAdapter){
        if (TempAdapter->DeviceObject  == DeviceObject){
            *pAdapter = TempAdapter;
            return(Status);
        }
        TempAdapter = TempAdapter->NextAdapter;
    }

    DEBUG_PRINT0(1, "SteFindAdapterByDeviceObject: No Adapter exist\n");            
    // �Y������A�_�v�^�����݂��Ȃ�����
    return(NDIS_STATUS_FAILURE);
}

/********************************************************************
 * SteCopyPacketToIrp()
 * 
 *    �����Ƃ��ēn���ꂽ Packet �̃f�[�^���A�����������Ƃ��Ă킽���ꂽ
 *    IRP �ɃR�s�[����B
 *        
 * ����:
 *
 *     Packet    : �p�P�b�g
 *     Irp       : ���z NIC �f�[��������󂯎���� IRP
 *
 * �Ԃ�l:
 *
 *    NDIS_STATUS
 *
 ********************************************************************/
NDIS_STATUS
SteCopyPacketToIrp(    
    IN     NDIS_PACKET  *Packet,
    IN OUT PIRP          Irp
    )
{
    NDIS_STATUS         Status = NDIS_STATUS_SUCCESS;
    PIO_STACK_LOCATION  irpStack;        
    UINT                IrpBufferSize; // ���X�� IRP �̃o�b�t�@�[�̃T�C�Y
    UINT                IrpBufferLeft; // �R�s�[�\�Ȏc��� IRP �o�b�t�@�[�̃T�C�Y
    UCHAR              *IrpBuffer;
    NDIS_BUFFER        *NdisBuffer;
    UINT                TotalPacketLength;
    UINT                CopiedLength = 0;
    VOID               *VirtualAddress;    
    UINT                Length;
    
    DEBUG_PRINT0(3, "SteCopyPacketToIrp called\n");                

    /* IRP ���� IO_STACK_LOCATION �𓾂�*/
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    /* IRP �����M�����o�b�t�@�[�ƁA�o�b�t�@�[�T�C�Y�𓾂� */
    IrpBuffer = Irp->AssociatedIrp.SystemBuffer;
    IrpBufferLeft = IrpBufferSize = irpStack->Parameters.Read.Length;

    DEBUG_PRINT1(3, "SteCopyPacketToIrp: IrpBufferSize = %d\n", IrpBufferSize);
    // IRP �̃o�b�t�@�[�T�C�Y���`�F�b�N
    if(IrpBufferSize < ETHERMIN){
        DEBUG_PRINT0(1, "SteCopyPacketToIrp: IRP buffer size too SMALL < ETHERMIN\n");
        Status = NDIS_STATUS_FAILURE;
        Irp->IoStatus.Information  = 0;        
        return(Status);
    }     
    
    if(IrpBuffer == NULL){
        DEBUG_PRINT0(1, "SteCopyPacketToIrp: Irp->AssociatedIrp.SystemBuffer is NULL\n");
        Status = NDIS_STATUS_FAILURE;
        Irp->IoStatus.Information  = 0;        
        return(Status);
    }

    NdisQueryPacket(
        Packet,              //IN PNDIS_PACKET           
        NULL,                //OUT PUINT OPTIONAL        
        NULL,                //OUT PUINT OPTIONAL        
        &NdisBuffer,         //OUT PNDIS_BUFFER OPTIONAL 
        &TotalPacketLength   //OUT PUINT OPTIONAL        
        );

    if( TotalPacketLength > ETHERMAX){
        // �傫������B�W�����{�t���[���H
        DEBUG_PRINT0(1, "SteCopyPacketToIrp: Packet size too BIG > ETHERMAX\n");
        Status = NDIS_STATUS_FAILURE;
        Irp->IoStatus.Information  = 0;        
        return(Status);
    }

    while(NdisBuffer != NULL) {
        NdisQueryBufferSafe(
            NdisBuffer,        //IN PNDIS_BUFFER      
            &VirtualAddress,   //OUT PVOID * OPTIONAL  
            &Length,           //OUT PUINT            
            NormalPagePriority //IN MM_PAGE_PRIORITY
            );

        //
        // �c��� IRP �o�b�t�@�[�ʂ��`�F�b�N�B
        // �������z NIC �f�[������ ReadFile() �ɂ� ETHERMAX �����̃o�b�t�@�[�T�C�Y
        // ���w�肵�Ă����ꍇ�́AIRP �̃o�b�t�@�[������Ȃ��Ȃ�\��������B
        //
        if (Length > IrpBufferLeft){
            //
            // ���r���[�� Ethernet �t���[�����R�s�[���Ă��d���������̂ŁA�G���[
            // �Ƃ��ă��^�[������B
            //
            Status = NDIS_STATUS_FAILURE;
            DEBUG_PRINT0(1, "SteCopyPacketToIrp: Insufficient IRP buffer size\n");            
            break;            
        }
  
        if(VirtualAddress == NULL){
            // NDIS �o�b�t�@�[�Ƀf�[�^���܂܂�Ă��Ȃ��H
            Status = NDIS_STATUS_FAILURE;
            DEBUG_PRINT0(1, "SteCopyPacketToIrp: NdisQueryBuffer: VirtualAddress is NULL\n");
            break;
        }

        // �f�[�^���R�s�[����
        NdisMoveMemory(IrpBuffer, VirtualAddress, Length);
        CopiedLength  += Length;
        IrpBufferLeft -= Length;
        IrpBuffer     += Length;

        // �c��̃o�b�t�@�[�ʂ��`�F�b�N�B������Ȃ炱���Ńu���C�N�B
        if(IrpBufferLeft == 0){
            break;
        }

        // ���� NDIS �o�b�t�@�[�𓾂�B
        NdisGetNextBuffer(
            NdisBuffer,  //IN PNDIS_BUFFER   
            &NdisBuffer  //OUT PNDIS_BUFFER  
            );
    }

    if(Status != NDIS_STATUS_SUCCESS) {
        // �G���[�����������悤�Ȃ̂� Irp->IoStatus.Information �� 0 ���Z�b�g        
        Irp->IoStatus.Information  = 0;
    } else {
        // �R�s�[�����T�C�Y���`�F�b�N
        if(CopiedLength < ETHERMIN){
            // �������B�p�f�B���O����B
            // IRP �o�b�t�@�[�� ETHERMIN �ȏ�ł��邱�Ƃ͊m�F�ς݂Ȃ̂ŁA������
            // �R�s�[�ς݃T�C�Y�� ETHERMIN �Ɋg�債�Ă����͖����B
            DEBUG_PRINT0(2, "SteCopyPacketToIrp: Packet size is < ETHERMIN. Padded.\n"); 
            CopiedLength = ETHERMIN;   
        } 
        Irp->IoStatus.Information  = CopiedLength;        
    }

    return(Status);
}


/********************************************************************
 * StePrintPacket()
 * 
 *    �����Ƃ��ēn���ꂽ Packet �̃f�[�^��debug �o�͂���
 *        
 * ����:
 *
 *     Packet    - �\������ Packet
 *
 * �Ԃ�l:
 *
 *    ������: TRUE
 *    ���s��: FALSE
 *
 ********************************************************************/
VOID
StePrintPacket(
    NDIS_PACKET *Packet)
{
    
    NDIS_BUFFER       *Ndis_Buffer;  
    PVOID              VirtualAddress; 
    UINT               NdisBufferLen;  
    UINT               TotalPacketLength;   
    UINT               PrintOffset = 0;
    UCHAR             *Data;           
    
    DEBUG_PRINT0(3, "StePrintPacket called\n");

    NdisQueryPacket(
        Packet,             //IN  PNDIS_PACKET  
        NULL,               //OUT PUINT OPTIONAL
        NULL,               //OUT PUINT         
        &Ndis_Buffer,       //OUT PNDIS_BUFFER *
        &TotalPacketLength  //OUT PUINT         
        );

    while(Ndis_Buffer){
        UINT i = 0;
            
        NdisQueryBufferSafe(
            Ndis_Buffer,
            &VirtualAddress,
            &NdisBufferLen,
            NormalPagePriority);
        
        if(VirtualAddress == NULL){
            DEBUG_PRINT0(1, "StePrintPacket: VirtuallAddress is NULL\n");            
            break;
        }

        Data = (unsigned char *)VirtualAddress;

        /*
         * debug ���x���� 4 �����̏ꍇ�� 14byte
         * �iEthernet Header ���j�����o�͂���B
         */
        if(DebugLevel < 4){
            NdisBufferLen = 14;
        }
        
        while(NdisBufferLen){
            if( PrintOffset%16 == 0){
                if(PrintOffset != 0)
                    DbgPrint ("\n");
                DbgPrint("0x%.4x: ", PrintOffset);
            }
            DbgPrint("%.2x ", Data[i]);
            PrintOffset++;
            
            NdisBufferLen--;
            i++;
        }
        NdisGetNextBuffer( Ndis_Buffer, &Ndis_Buffer);
    }
    DbgPrint("\n");
    DEBUG_PRINT0(3, "StePrintPacket end\n");

    return;
}

/*****************************************************************************
 * bebug_print()
 *
 * �f�o�b�O�o�͗p�֐�
 *
 *  �����F
 *           Level  :  �G���[�̐[���x�B
 *           Format :  ���b�Z�[�W�̏o�̓t�H�[�}�b�g
 *           ...
 * �߂�l�F
 *           �Ȃ��B
 *****************************************************************************/
VOID
SteDebugPrint(
    IN ULONG   Level,
    IN PCCHAR  Format,
    ...
    )
{
    va_list        list;
    UCHAR          Buffer[STE_MAX_DEBUG_MSG];
    NTSTATUS       NtStatus;
    
    va_start(list, Format);
    NtStatus = RtlStringCbVPrintfA(Buffer, sizeof(Buffer), Format, list);
    if(NtStatus != STATUS_SUCCESS) {
        DbgPrint("Ste: SteDebugPrint Failed.\n");
        return;
    }
    va_end(list);

    if (Level <= DebugLevel) {
         DbgPrint ("Ste: %s", Buffer);
    }        
    return;
}
