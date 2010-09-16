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
#ifndef __STED_H
#define __STED_H

#ifdef  STE_WINDOWS
#include "sted_win.h"
#define STEPATH "\\\\.\\STE"    /* ste �f�o�C�X�̃p�X */
#else
#define STEPATH "/dev/ste"      /* ste �f�o�C�X�̃p�X */
#endif

/*
 * Windows �̏ꍇ SAGetLastError() ���g���� errno �ɃG���[�ԍ����Z�b�g����
 * �܂��AWindows �̏ꍇ socket �� close �ɂ� closesocket() ���g���B
 */
#ifdef STE_WINDOWS
#define SET_ERRNO()   errno = WSAGetLastError()
#define CLOSE(fd)     closesocket(fd)
#else
#define SET_ERRNO()
#define CLOSE(fd)     close(fd)
#endif

/*******************************************************
 * o ���z NIC �f�[���������p����e��p�����[�^
 *
 *  CONNECT_REQ_SIZE     proxy �ɑ΂��� CONNECT �v���̕����� 
 *  CONNECT_REQ_TIMEOUT  Proxy ���� CONNECT �̃��X�|���X���󂯎��^�C���A�E�g
 *  STRBUFSIZE           getmsg(9F),putmsg(9F) �p�̃o�b�t�@�̃T�C�Y 
 *  PORT_NO              �f�t�H���g�̉��z�n�u�̃|�[�g�ԍ�
 *  SOCKBUFSIZE          recv(), send() �p�̃o�b�t�@�̃T�C�Y                
 *  ERR_MSG_MAX          syslog ��ASTDERR �ɏo�͂��郁�b�Z�[�W�̃T�C�Y   
 *  SENDBUF_THRESHOLD    ���M�ꎞ�o�b�t�@�̃f�[�^�𑗐M���邵�����l�B
 *  SELECT_TIMEOUT       select() �p�̃^�C���A�E�g�iSolaris �p)
 *  HTTP_STAT_OK         HTTP �̃X�e�[�^�X�R�[�h OK
 *  MAXHOSTNAME          �z�X�g���iHUB��Proxy�j�̍ő咷 
 *  GETMSG_MAXWAIT       getmsg(9F) �̃^�C���A�E�g�l�iSolaris �p)
 ********************************************************/
#define  CONNECT_REQ_SIZE         200    
#define  CONNECT_REQ_TIMEOUT      10  
#define  STRBUFSIZE               32768       
#define  PORT_NO                  80            
#define  SOCKBUFSIZE              32768     
#define  ERR_MSG_MAX              300         
#define  SENDBUF_THRESHOLD        3028    // ETHERMAX(1514) x 2
#define  SELECT_TIMEOUT           400000  // 400m sec = 0.4 sec
#define  HTTP_STAT_OK             200        
#define  MAXHOSTNAME              30          
#define  GETMSG_MAXWAIT           15
#define  STE_MAX_DEVICE_NAME      30

/*
 * ���z NIC �f�[���� sted �ƁA���z�n�u�f�[���� stehub ���ʐM��
 * �s���ہA����M���� Ethernet �t���[���̃f�[�^�ɕt�������w�b�_�B
 */
typedef struct stehead 
{
    int           len;    /* �p�f�B���O��̃f�[�^�T�C�Y */
    int           orglen; /* �p�f�B���O����O�̃T�C�Y�B*/
} stehead_t;

/*
 * sted �f�[�������g�� sted �̊Ǘ��p�\����
 * HUB �Ƃ̒ʐM�̏���A���z NIC �h���C�o�̏��������Ă���B
 */
typedef struct sted_stat
{
    /* Socket �ʐM�p�p��� */
    int           sock_fd;                 /* HUB �܂��� Proxy �Ƃ̒ʐM�ɂ��� FD  */
    char          hub_name[MAXHOSTNAME];   /* ���z�n�u�� */
    int           hub_port;                /* ���z�n�u�̃|�[�g�ԍ� */
    char          proxy_name[MAXHOSTNAME]; /* �v���L�V�[�T�[�o��   */ 
    int           proxy_port;              /* �v���L�V�[�T�[�o�̃|�[�g�ԍ�  */
    int           sendbuflen;              /* ���M�o�b�t�@�ւ̌��݂̏������݃T�C�Y  */
    int           datalen;                 /* �p�b�h���܂� Ethernet �t���[���̃T�C�Y*/
    int           orgdatalen;              /* ���� Ethernet �t���[���̃T�C�Y        */
    int           dataleft;                /* ����M�� Ethernet �t���[���̃T�C�Y    */
    stehead_t     dummyhead;               /* ��M�r���� stehead �̃R�s�[           */
    int           dummyheadlen;            /* ��M�ς݂� stehead �̃T�C�Y           */
    int           use_syslog;              /* ���b�Z�[�W�� STDERR �łȂ��Asyslog �ɏo�͂��� */
    unsigned char sendbuf[SOCKBUFSIZE];    /* Socket ���M�p�o�b�t�@ */
    unsigned char recvbuf[SOCKBUFSIZE];    /* Socket ��M�p�o�b�t�@ */
    /* ste �h���C�o�p��� */
#ifdef STE_WINDOWS
    HANDLE        ste_handle;              /* ���z NIC �f�o�C�X���I�[�v�������t�@�C���n���h�� */
#else    
    int           ste_fd;                  /* ���z NIC �f�o�C�X���I�[�v������ FD */
#endif    
    unsigned char wdatabuf[STRBUFSIZE]; /* �h���C�o�ւ̏������ݗp�o�b�t�@  */
    unsigned char rdatabuf[STRBUFSIZE]; /* �h���C�o����̓ǂݍ��ݗp�o�b�t�@*/    
} stedstat_t;

/*
 * sted �̓����֐��̃v���g�^�C�v
 */
extern void     print_err(int, char *, ...);
extern int      open_socket(stedstat_t *, char *, char *);
extern int      read_socket(stedstat_t *);
extern int      write_socket(stedstat_t *);
extern u_char  *read_socket_header(stedstat_t *, int *, unsigned char *);
extern int      send_connect_req(stedstat_t *);
extern char    *stat2string(int);
extern void     print_usage(char *);
extern int      open_ste(stedstat_t *, char *, int);
extern int      write_ste(stedstat_t *);
extern int      read_ste(stedstat_t *);

#endif /* #ifndef __STED_H */
