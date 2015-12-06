/* -*- Mode: C; tab-width: 4; -*- */
/*
* Copyright (C) 2009, HustMoon Studio
*
* 文件名称：mystate.c
* 摘	要：改变认证状态
* 作	者：HustMoon@BYHH
* 邮	箱：www.ehust@gmail.com
*/

/*
 * modified by shilianggoo@gmail.com 2014
 * add NEED_LOGOUT, fix permission problems
 * in some secure setuid shebang implemention
 */

#include "mystate.h"
#include "i18n.h"
#include "myfunc.h"
#include "dlfunc.h"
#include "checkV4.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <netinet/in.h>

#define MAX_SEND_COUNT		3	/* 最大超时次数 */

volatile int state = ID_DISCONNECT;	/* 认证状态 */
const u_char *capBuf = NULL;	/* 抓到的包 */
static u_char sendPacket[0x3E8];	/* 用来发送的包 */
static int sendCount = 0;	/* 同一阶段发包计数 */

extern char nic[];
extern const u_char STANDARD_ADDR[];
extern char userName[];
extern unsigned startMode;
extern unsigned dhcpMode;
extern u_char localMAC[], destMAC[];
extern unsigned timeout;
extern unsigned echoInterval;
extern unsigned restartWait;
extern char dhcpScript[];
extern pcap_t *hPcap;
extern u_char *fillBuf;
extern unsigned fillSize;
extern u_int32_t pingHost;
#ifndef NO_ARP
extern u_int32_t rip, gateway;
extern u_char gateMAC[];
static void sendArpPacket();	/* ARP监视 */
#endif

static const u_char pad[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static pid_t cpid;
static const unsigned char pkt1[503] = {
/*0x00, 0x00, */0xff, 0xff, 0x37, 0x77, 0x7f, 0xff, /* ....7w.. */
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* ........ */
0xff, 0xff, 0xff, 0xac, 0xb1, 0xff, 0xb0, 0xb0, /* ........ */
0x2d, 0x00, 0x00, 0x13, 0x11, 0x38, 0x30, 0x32, /* -....802 */
0x31, 0x78, 0x2e, 0x65, 0x78, 0x65, 0x00, 0x00, /* 1x.exe.. */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, /* ........ */
0x02, 0x00, 0x00, 0x00, 0x13, 0x11, 0x01, 0xb1, /* ........ */
0x1a, 0x0c, 0x00, 0x00, 0x13, 0x11, 0x18, 0x06, /* ........ */
0x00, 0x00, 0x00, 0x01, 0x1a, 0x0e, 0x00, 0x00, /* ........ */
0x13, 0x11, 0x2d, 0x08, 0x08, 0x9e, 0x01, 0x64, /* ..-....d */
0xe3, 0x15, 0x1a, 0x08, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x2f, 0x02, 0x1a, 0x0f, 0x00, 0x00, 0x13, 0x11, /* /....... */
0x76, 0x09, 0x38, 0x2e, 0x38, 0x2e, 0x38, 0x2e, /* v.8.8.8. */
0x38, 0x1a, 0x09, 0x00, 0x00, 0x13, 0x11, 0x35, /* 8......5 */
0x03, 0x01, 0x1a, 0x18, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x36, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 6....... */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x1a, 0x18, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x38, 0x12, 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, /* 8....... */
0x00, 0x00, 0x0a, 0x9e, 0x01, 0xff, 0xfe, 0x64, /* .......d */
0xe3, 0x15, 0x1a, 0x18, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x4e, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* N....... */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x1a, 0x88, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x4d, 0x82, 0x62, 0x34, 0x36, 0x34, 0x38, 0x39, /* M.b46489 */
0x36, 0x64, 0x38, 0x31, 0x33, 0x35, 0x65, 0x65, /* 6d8135ee */
0x31, 0x64, 0x61, 0x37, 0x64, 0x64, 0x32, 0x39, /* 1da7dd29 */
0x32, 0x36, 0x62, 0x63, 0x62, 0x62, 0x36, 0x35, /* 26bcbb65 */
0x61, 0x65, 0x34, 0x65, 0x62, 0x37, 0x37, 0x64, /* ae4eb77d */
0x66, 0x36, 0x38, 0x31, 0x30, 0x31, 0x32, 0x61, /* f681012a */
0x38, 0x65, 0x35, 0x32, 0x63, 0x65, 0x38, 0x62, /* 8e52ce8b */
0x66, 0x35, 0x36, 0x36, 0x31, 0x32, 0x35, 0x31, /* f5661251 */
0x39, 0x34, 0x65, 0x64, 0x31, 0x39, 0x37, 0x62, /* 94ed197b */
0x63, 0x66, 0x64, 0x61, 0x38, 0x66, 0x37, 0x32, /* cfda8f72 */
0x39, 0x66, 0x35, 0x38, 0x32, 0x61, 0x31, 0x64, /* 9f582a1d */
0x33, 0x33, 0x30, 0x64, 0x35, 0x66, 0x30, 0x33, /* 330d5f03 */
0x62, 0x61, 0x34, 0x38, 0x32, 0x34, 0x35, 0x61, /* ba48245a */
0x38, 0x33, 0x32, 0x35, 0x66, 0x31, 0x32, 0x31, /* 8325f121 */
0x35, 0x65, 0x62, 0x62, 0x65, 0x34, 0x63, 0x66, /* 5ebbe4cf */
0x39, 0x63, 0x31, 0x63, 0x61, 0x32, 0x35, 0x62, /* 9c1ca25b */
0x30, 0x62, 0x1a, 0x28, 0x00, 0x00, 0x13, 0x11, /* 0b.(.... */
0x39, 0x22, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x6e, /* 9"intern */
0x65, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* et...... */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x1a, 0x48, 0x00, 0x00, 0x13, 0x11, /* ...H.... */
0x54, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* TB...... */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x1a, 0x08, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x55, 0x02, 0x1a, 0x09, 0x00, 0x00, 0x13, 0x11, /* U....... */
0x62, 0x03, 0x00, 0x1a, 0x09, 0x00, 0x00, 0x13, /* b....... */
0x11, 0x70, 0x03, 0x40, 0x1a, 0x1d, 0x00, 0x00, /* .p.@.... */
0x13, 0x11, 0x6f, 0x17, 0x52, 0x47, 0x2d, 0x53, /* ..o.RG-S */
0x55, 0x20, 0x46, 0x6f, 0x72, 0x20, 0x4c, 0x69, /* U For Li */
0x6e, 0x75, 0x78, 0x20, 0x56, 0x31, 0x2e, 0x30, /* nux V1.0 */
0x00                                            /* . */
};

static const unsigned char pkt2[503] = {
/*0x32, */0xff, 0xff, 0x37, 0x77, 0x7f, 0xff, 0xff, /* 2..7w... */
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* ........ */
0xff, 0xff, 0xac, 0xb1, 0xff, 0xb0, 0xb0, 0x2d, /* .......- */
0x00, 0x00, 0x13, 0x11, 0x38, 0x30, 0x32, 0x31, /* ....8021 */
0x78, 0x2e, 0x65, 0x78, 0x65, 0x00, 0x00, 0x00, /* x.exe... */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, /* ........ */
0x00, 0x00, 0x00, 0x13, 0x11, 0x01, 0xb1, 0x1a, /* ........ */
0x0c, 0x00, 0x00, 0x13, 0x11, 0x18, 0x06, 0x00, /* ........ */
0x00, 0x00, 0x01, 0x1a, 0x0e, 0x00, 0x00, 0x13, /* ........ */
0x11, 0x2d, 0x08, 0x08, 0x9e, 0x01, 0x64, 0xe3, /* .-....d. */
0x15, 0x1a, 0x08, 0x00, 0x00, 0x13, 0x11, 0x2f, /* ......./ */
0x02, 0x1a, 0x0f, 0x00, 0x00, 0x13, 0x11, 0x76, /* .......v */
0x09, 0x38, 0x2e, 0x38, 0x2e, 0x38, 0x2e, 0x38, /* .8.8.8.8 */
0x1a, 0x09, 0x00, 0x00, 0x13, 0x11, 0x35, 0x03, /* ......5. */
0x01, 0x1a, 0x18, 0x00, 0x00, 0x13, 0x11, 0x36, /* .......6 */
0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x1a, 0x18, 0x00, 0x00, 0x13, 0x11, 0x38, /* .......8 */
0x12, 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x0a, 0x9e, 0x01, 0xff, 0xfe, 0x64, 0xe3, /* ......d. */
0x15, 0x1a, 0x18, 0x00, 0x00, 0x13, 0x11, 0x4e, /* .......N */
0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x1a, 0x88, 0x00, 0x00, 0x13, 0x11, 0x4d, /* .......M */
0x82, 0x62, 0x34, 0x36, 0x34, 0x38, 0x39, 0x36, /* .b464896 */
0x64, 0x38, 0x31, 0x33, 0x35, 0x65, 0x65, 0x31, /* d8135ee1 */
0x64, 0x61, 0x37, 0x64, 0x64, 0x32, 0x39, 0x32, /* da7dd292 */
0x36, 0x62, 0x63, 0x62, 0x62, 0x36, 0x35, 0x61, /* 6bcbb65a */
0x65, 0x34, 0x65, 0x62, 0x37, 0x37, 0x64, 0x66, /* e4eb77df */
0x36, 0x38, 0x31, 0x30, 0x31, 0x32, 0x61, 0x38, /* 681012a8 */
0x65, 0x35, 0x32, 0x63, 0x65, 0x38, 0x62, 0x66, /* e52ce8bf */
0x35, 0x36, 0x36, 0x31, 0x32, 0x35, 0x31, 0x39, /* 56612519 */
0x34, 0x65, 0x64, 0x31, 0x39, 0x37, 0x62, 0x63, /* 4ed197bc */
0x66, 0x64, 0x61, 0x38, 0x66, 0x37, 0x32, 0x39, /* fda8f729 */
0x66, 0x35, 0x38, 0x32, 0x61, 0x31, 0x64, 0x33, /* f582a1d3 */
0x33, 0x30, 0x64, 0x35, 0x66, 0x30, 0x33, 0x62, /* 30d5f03b */
0x61, 0x34, 0x38, 0x32, 0x34, 0x35, 0x61, 0x38, /* a48245a8 */
0x33, 0x32, 0x35, 0x66, 0x31, 0x32, 0x31, 0x35, /* 325f1215 */
0x65, 0x62, 0x62, 0x65, 0x34, 0x63, 0x66, 0x39, /* ebbe4cf9 */
0x63, 0x31, 0x63, 0x61, 0x32, 0x35, 0x62, 0x30, /* c1ca25b0 */
0x62, 0x1a, 0x28, 0x00, 0x00, 0x13, 0x11, 0x39, /* b.(....9 */
0x22, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x6e, 0x65, /* "interne */
0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* t....... */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x1a, 0x48, 0x00, 0x00, 0x13, 0x11, 0x54, /* ..H....T */
0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* B....... */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x1a, 0x08, 0x00, 0x00, 0x13, 0x11, 0x55, /* .......U */
0x02, 0x1a, 0x09, 0x00, 0x00, 0x13, 0x11, 0x62, /* .......b */
0x03, 0x00, 0x1a, 0x09, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x70, 0x03, 0x40, 0x1a, 0x1d, 0x00, 0x00, 0x13, /* p.@..... */
0x11, 0x6f, 0x17, 0x52, 0x47, 0x2d, 0x53, 0x55, /* .o.RG-SU */
0x20, 0x46, 0x6f, 0x72, 0x20, 0x4c, 0x69, 0x6e, /*  For Lin */
0x75, 0x78, 0x20, 0x56, 0x31, 0x2e, 0x30, 0x00  /* ux V1.0. */
};

static const unsigned char pkt3[519] = {
/*0x32, 0x32, */0xff, 0xff, 0x37, 0x77, 0x7f, 0xff, /* 22..7w.. */
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* ........ */
0xff, 0xff, 0xff, 0xac, 0xb1, 0xff, 0xb0, 0xb0, /* ........ */
0x2d, 0x00, 0x00, 0x13, 0x11, 0x38, 0x30, 0x32, /* -....802 */
0x31, 0x78, 0x2e, 0x65, 0x78, 0x65, 0x00, 0x00, /* 1x.exe.. */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, /* ........ */
0x02, 0x00, 0x00, 0x00, 0x13, 0x11, 0x01, 0xc1, /* ........ */
0x1a, 0x0c, 0x00, 0x00, 0x13, 0x11, 0x18, 0x06, /* ........ */
0x00, 0x00, 0x00, 0x01, 0x1a, 0x0e, 0x00, 0x00, /* ........ */
0x13, 0x11, 0x2d, 0x08, 0x08, 0x9e, 0x01, 0x64, /* ..-....d */
0xe3, 0x15, 0x1a, 0x18, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x2f, 0x12, 0x08, 0xc8, 0x6f, 0xb1, 0xdb, 0xb7, /* /...o... */
0x83, 0x4b, 0x52, 0x1d, 0xb2, 0x71, 0x55, 0xec, /* .KR..qU. */
0xd6, 0xb2, 0x1a, 0x0f, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x76, 0x09, 0x38, 0x2e, 0x38, 0x2e, 0x38, 0x2e, /* v.8.8.8. */
0x38, 0x1a, 0x09, 0x00, 0x00, 0x13, 0x11, 0x35, /* 8......5 */
0x03, 0x01, 0x1a, 0x18, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x36, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 6....... */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x1a, 0x18, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x38, 0x12, 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, /* 8....... */
0x00, 0x00, 0x0a, 0x9e, 0x01, 0xff, 0xfe, 0x64, /* .......d */
0xe3, 0x15, 0x1a, 0x18, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x4e, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* N....... */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x1a, 0x88, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x4d, 0x82, 0x39, 0x36, 0x64, 0x30, 0x37, 0x62, /* M.96d07b */
0x35, 0x63, 0x64, 0x35, 0x38, 0x34, 0x62, 0x36, /* 5cd584b6 */
0x62, 0x62, 0x65, 0x64, 0x32, 0x35, 0x65, 0x64, /* bbed25ed */
0x39, 0x38, 0x38, 0x64, 0x34, 0x32, 0x66, 0x66, /* 988d42ff */
0x30, 0x65, 0x37, 0x39, 0x39, 0x63, 0x33, 0x39, /* 0e799c39 */
0x63, 0x38, 0x64, 0x37, 0x32, 0x31, 0x36, 0x61, /* c8d7216a */
0x38, 0x65, 0x35, 0x66, 0x61, 0x32, 0x66, 0x61, /* 8e5fa2fa */
0x64, 0x37, 0x66, 0x38, 0x64, 0x33, 0x35, 0x37, /* d7f8d357 */
0x61, 0x31, 0x38, 0x62, 0x33, 0x66, 0x66, 0x65, /* a18b3ffe */
0x65, 0x30, 0x64, 0x31, 0x38, 0x34, 0x65, 0x30, /* e0d184e0 */
0x32, 0x65, 0x32, 0x39, 0x31, 0x32, 0x38, 0x64, /* 2e29128d */
0x33, 0x30, 0x63, 0x61, 0x64, 0x33, 0x65, 0x36, /* 30cad3e6 */
0x36, 0x31, 0x38, 0x37, 0x36, 0x32, 0x33, 0x63, /* 6187623c */
0x31, 0x37, 0x31, 0x35, 0x31, 0x63, 0x61, 0x39, /* 17151ca9 */
0x36, 0x65, 0x35, 0x61, 0x32, 0x34, 0x32, 0x31, /* 6e5a2421 */
0x65, 0x39, 0x35, 0x35, 0x37, 0x61, 0x37, 0x38, /* e9557a78 */
0x35, 0x35, 0x1a, 0x28, 0x00, 0x00, 0x13, 0x11, /* 55.(.... */
0x39, 0x22, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x6e, /* 9"intern */
0x65, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* et...... */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x1a, 0x48, 0x00, 0x00, 0x13, 0x11, /* ...H.... */
0x54, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* TB...... */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........ */
0x00, 0x00, 0x1a, 0x08, 0x00, 0x00, 0x13, 0x11, /* ........ */
0x55, 0x02, 0x1a, 0x09, 0x00, 0x00, 0x13, 0x11, /* U....... */
0x62, 0x03, 0x00, 0x1a, 0x09, 0x00, 0x00, 0x13, /* b....... */
0x11, 0x70, 0x03, 0x40, 0x1a, 0x1d, 0x00, 0x00, /* .p.@.... */
0x13, 0x11, 0x6f, 0x17, 0x52, 0x47, 0x2d, 0x53, /* ..o.RG-S */
0x55, 0x20, 0x46, 0x6f, 0x72, 0x20, 0x4c, 0x69, /* U For Li */
0x6e, 0x75, 0x78, 0x20, 0x56, 0x31, 0x2e, 0x30, /* nux V1.0 */
0x00                                            /* . */
};

static void setTimer(unsigned interval);	/* 设置定时器 */
static int renewIP();	/* 更新IP */
static void fillEtherAddr(u_int32_t protocol);  /* 填充MAC地址和协议 */
static int sendStartPacket();	 /* 发送Start包 */
static int sendIdentityPacket();	/* 发送Identity包 */
static int sendChallengePacket();   /* 发送Md5 Challenge包 */
static int sendEchoPacket();	/* 发送心跳包 */
static int sendLogoffPacket();  /* 发送退出包 */
static int waitEchoPacket();	/* 等候响应包 */

static void setTimer(unsigned interval) /* 设置定时器 */
{
	struct itimerval timer;
	timer.it_value.tv_sec = interval;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = interval;
	timer.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &timer, NULL);
}

int switchState(int type)
{
	if (state == type) /* 跟上次是同一状态？ */
		sendCount++;
	else
	{
		state = type;
		sendCount = 0;
	}
	if (sendCount>=MAX_SEND_COUNT && type!=ID_ECHO)  /* 超时太多次？ */
	{
		switch (type)
		{
		case ID_START:
			printf(_(">> 找不到服务器，重启认证!\n"));
			break;
		case ID_IDENTITY:
			printf(_(">> 发送用户名超时，重启认证!\n"));
			break;
		case ID_CHALLENGE:
			printf(_(">> 发送密码超时，重启认证!\n"));
			break;
		case ID_WAITECHO:
			printf(_(">> 等候响应包超时，自行响应!\n"));
			return switchState(ID_ECHO);
		}
		return restart();
	}
	switch (type)
	{
	case ID_DHCP:
		return renewIP();
	case ID_START:
		return sendStartPacket();
	case ID_IDENTITY:
		return sendIdentityPacket();
	case ID_CHALLENGE:
		return sendChallengePacket();
	case ID_WAITECHO:	/* 塞尔的就不ping了，不好计时 */
		return waitEchoPacket();
	case ID_ECHO:
		if (pingHost && sendCount*echoInterval > 60) {	/* 1分钟左右 */
			if (isOnline() == -1) {
				printf(_(">> 认证掉线，开始重连!\n"));
				return switchState(ID_START);
			}
			sendCount = 1;
		}
#ifndef NO_ARP
		if (gateMAC[0] != 0xFE)
			sendArpPacket();
#endif
		return sendEchoPacket();
	case ID_DISCONNECT:
		return sendLogoffPacket();
	}
	return 0;
}

int restart()
{
	if (startMode >= 3)	/* 标记服务器地址为未获取 */
		startMode -= 3;
	state = ID_START;
	sendCount = -1;
	setTimer(restartWait);	/* restartWait秒后或者服务器请求后重启认证 */
	return 0;
}

static int renewIP()
{
    int cpidstate;
	setTimer(0);	/* 取消定时器 */
	printf(_(">> 正在获取IP...\n"));
	//system(dhcpScript);
    if(cpid != 0)
    {
        // dhcp client has been started
        printf("dhcpScript already run\n");
    }
    else
    {
        cpid = fork();
        if(cpid == 0)
            execlp(dhcpScript, dhcpScript, nic, NULL);
        else if(cpid < 0)
            printf("Fork dhcpScript failed, is your dhcpScript setted?\n");
        else
            wait(&cpidstate);
    }
    printf(_(">> 操作结束。\n"));
	dhcpMode += 3; /* 标记为已获取，123变为456，5不需再认证*/
	if (fillHeader() == -1)
		exit(EXIT_FAILURE);
	if (dhcpMode == 5)
		return switchState(ID_ECHO);
	return switchState(ID_START);
}

static void fillEtherAddr(u_int32_t protocol)
{
	memset(sendPacket, 0, 0x3E8);
	memcpy(sendPacket, destMAC, 6);
	memcpy(sendPacket+0x06, localMAC, 6);
	*(u_int32_t *)(sendPacket+0x0C) = htonl(protocol);
}

static int sendStartPacket()
{
	if (startMode%3 == 2)	/* 赛尔 */
	{
		if (sendCount == 0)
		{
			printf(_(">> 寻找服务器...\n"));
			memcpy(sendPacket, STANDARD_ADDR, 6);
			memcpy(sendPacket+0x06, localMAC, 6);
			*(u_int32_t *)(sendPacket+0x0C) = htonl(0x888E0101);
			*(u_int16_t *)(sendPacket+0x10) = 0;
			memset(sendPacket+0x12, 0xa5, 42);
			setTimer(timeout);
		}
		return pcap_sendpacket(hPcap, sendPacket, 60);
	}
	if (sendCount == 0)
	{
		printf(_(">> 寻找服务器...\n"));
		//fillStartPacket();
		fillEtherAddr(0x888E0101);
		memcpy(sendPacket + 0x12, pkt1, sizeof(pkt1));
                memcpy(sendPacket + 0xe2, computeV4(pad, 16), 0x80);
		setTimer(timeout);
	}
	return pcap_sendpacket(hPcap, sendPacket, 521);
}

static int sendIdentityPacket()
{
	int nameLen = strlen(userName);
	if (startMode%3 == 2)	/* 赛尔 */
	{
		if (sendCount == 0)
		{
			printf(_(">> 发送用户名...\n"));
			*(u_int16_t *)(sendPacket+0x0E) = htons(0x0100);
			*(u_int16_t *)(sendPacket+0x10) = *(u_int16_t *)(sendPacket+0x14) = htons(nameLen+30);
			sendPacket[0x12] = 0x02;
			sendPacket[0x16] = 0x01;
			sendPacket[0x17] = 0x01;
			fillCernetAddr(sendPacket);
			memcpy(sendPacket+0x28, "03.02.05", 8);
			memcpy(sendPacket+0x30, userName, nameLen);
			setTimer(timeout);
		}
		sendPacket[0x13] = capBuf[0x13];
		return pcap_sendpacket(hPcap, sendPacket, nameLen+48);
	}
	if (sendCount == 0)
	{
		printf(_(">> 发送用户名...\n"));
		fillEtherAddr(0x888E0100);
		nameLen = strlen(userName);
		*(u_int16_t *)(sendPacket+0x14) = *(u_int16_t *)(sendPacket+0x10) = htons(nameLen+5);
		sendPacket[0x12] = 0x02;
		sendPacket[0x13] = capBuf[0x13];
		sendPacket[0x16] = 0x01;
		memcpy(sendPacket+0x17, userName, nameLen);
		memcpy(sendPacket+0x17+nameLen, pkt2, sizeof(pkt2));
                memcpy(sendPacket + 0xe7 + nameLen, computeV4(pad, 16), 0x80);
		setTimer(timeout);
	}
	return pcap_sendpacket(hPcap, sendPacket, 536);
}

static int sendChallengePacket()
{
	int nameLen = strlen(userName);
	if (startMode%3 == 2)	/* 赛尔 */
	{
		if (sendCount == 0)
		{
			printf(_(">> 发送密码...\n"));
			*(u_int16_t *)(sendPacket+0x0E) = htons(0x0100);
			*(u_int16_t *)(sendPacket+0x10) = *(u_int16_t *)(sendPacket+0x14) = htons(nameLen+22);
			sendPacket[0x12] = 0x02;
			sendPacket[0x13] = capBuf[0x13];
			sendPacket[0x16] = 0x04;
			sendPacket[0x17] = 16;
			memcpy(sendPacket+0x18, checkPass(capBuf[0x13], capBuf+0x18, capBuf[0x17]), 16);
			memcpy(sendPacket+0x28, userName, nameLen);
			setTimer(timeout);
		}
		return pcap_sendpacket(hPcap, sendPacket, nameLen+40);
	}
	if (sendCount == 0)
	{
		printf(_(">> 发送密码...\n"));
		//fillMd5Packet(capBuf+0x18);
		fillEtherAddr(0x888E0100);
		*(u_int16_t *)(sendPacket+0x14) = *(u_int16_t *)(sendPacket+0x10) = htons(nameLen+22);
		sendPacket[0x12] = 0x02;
		sendPacket[0x13] = capBuf[0x13];
		sendPacket[0x16] = 0x04;
		sendPacket[0x17] = 16;
		memcpy(sendPacket+0x18, checkPass(capBuf[0x13], capBuf+0x18, capBuf[0x17]), 16);
		memcpy(sendPacket+0x28, userName, nameLen);

                memcpy(sendPacket+0x28+nameLen, pkt3, sizeof(pkt3));
                memcpy(sendPacket + 0x90 + nameLen, computePwd(capBuf+0x18), 0x10);
                //memcpy(sendPacket + 0xa0 +nameLen, fillBuf + 0x68, fillSize-0x68);
                memcpy(sendPacket + 0x108 + nameLen, computeV4(capBuf+0x18, capBuf[0x17]), 0x80);
                //sendPacket[0x77] = 0xc7;
		setTimer(timeout);
	}
	return pcap_sendpacket(hPcap, sendPacket, 569);
}

static int sendEchoPacket()
{
	if (startMode%3 == 2)	/* 赛尔 */
	{
		*(u_int16_t *)(sendPacket+0x0E) = htons(0x0106);
		*(u_int16_t *)(sendPacket+0x10) = 0;
		memset(sendPacket+0x12, 0xa5, 42);
		switchState(ID_WAITECHO);	/* 继续等待 */
		return pcap_sendpacket(hPcap, sendPacket, 60);
	}
	if (sendCount == 0)
	{
		u_char echo[] =
		{
			0x00,0x1E,0xFF,0xFF,0x37,0x77,0x7F,0x9F,0xFF,0xFF,0xD9,0x13,0xFF,0xFF,0x37,0x77,
			0x7F,0x9F,0xFF,0xFF,0xF7,0x2B,0xFF,0xFF,0x37,0x77,0x7F,0x3F,0xFF
		};
		printf(_(">> 发送心跳包以保持在线...\n"));
		fillEtherAddr(0x888E01BF);
		memcpy(sendPacket+0x10, echo, sizeof(echo));
		setTimer(echoInterval);
	}
	fillEchoPacket(sendPacket);
	return pcap_sendpacket(hPcap, sendPacket, 0x2D);
}

static int sendLogoffPacket()
{
	setTimer(0);	/* 取消定时器 */
	if (startMode%3 == 2)	/* 赛尔 */
	{
		*(u_int16_t *)(sendPacket+0x0E) = htons(0x0102);
		*(u_int16_t *)(sendPacket+0x10) = 0;
		memset(sendPacket+0x12, 0xa5, 42);
		return pcap_sendpacket(hPcap, sendPacket, 60);
	}
#ifdef NEED_LOGOUT
	fillStartPacket();	/* 锐捷的退出包与Start包类似，不过其实不这样也是没问题的 */
	fillEtherAddr(0x888E0102);
	memcpy(sendPacket+0x12, fillBuf, fillSize);
	return pcap_sendpacket(hPcap, sendPacket, 0x3E8);
#else
    return 0;
#endif
}

static int waitEchoPacket()
{
	if (sendCount == 0)
		setTimer(echoInterval);
	return 0;
}

#ifndef NO_ARP
static void sendArpPacket()
{
	u_char arpPacket[0x3C] = {
		0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x06,0x00,0x01,
		0x08,0x00,0x06,0x04,0x00};

	if (gateMAC[0] != 0xFF) {
		memcpy(arpPacket, gateMAC, 6);
		memcpy(arpPacket+0x06, localMAC, 6);
		arpPacket[0x15]=0x02;
		memcpy(arpPacket+0x16, localMAC, 6);
		memcpy(arpPacket+0x1c, &rip, 4);
		memcpy(arpPacket+0x20, gateMAC, 6);
		memcpy(arpPacket+0x26, &gateway, 4);
		pcap_sendpacket(hPcap, arpPacket, 0x3C);
	}
	memset(arpPacket, 0xFF, 6);
	memcpy(arpPacket+0x06, localMAC, 6);
	arpPacket[0x15]=0x01;
	memcpy(arpPacket+0x16, localMAC, 6);
	memcpy(arpPacket+0x1c, &rip, 4);
	memset(arpPacket+0x20, 0, 6);
	memcpy(arpPacket+0x26, &gateway, 4);
	pcap_sendpacket(hPcap, arpPacket, 0x2A);
}
#endif
