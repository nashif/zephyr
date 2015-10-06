/*
 * Copyright (c) 2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <ifaddrs.h>
#include <signal.h>

#include <tinydtls.h>
#include <global.h>
#include <debug.h>
#include <dtls.h>

#ifdef __GNUC__
#define UNUSED_PARAM __attribute__((unused))
#else
#define UNUSED_PARAM
#endif /* __GNUC__ */

#define SERVER_PORT  4242

#define CLIENT_PORT  8484
#define MAX_BUF_SIZE 1280	/* min IPv6 MTU, the actual data is smaller */
#define MAX_TIMEOUT  3		/* in seconds */

static bool debug;
static int renegotiate = -1;

#define ENTRY(e, expect_result) { sizeof(e), e, expect_result }
#define ENTRY_OK(e) ENTRY(e, true)
#define ENTRY_FAIL(e) ENTRY(e, false)

static const unsigned char A[] = { 'A' };
static const unsigned char null_byte[] = { 0x00 };
static const unsigned char foobar[] = { 'f','o','o','b','a','r' };
static const unsigned char small_binary[] = { 0x20, 0xff, 0x00, 0x56 };

/* Generated by http://www.lipsum.com/
 * 1202 bytes of Lorem Ipsum.
 *
 * This is the maximum we can send with encryption.
 */
static const char lorem_ipsum[] =
	"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin congue orci et lectus ultricies, sed elementum urna finibus. Nam bibendum, massa id sollicitudin finibus, massa ante pharetra lacus, nec semper felis metus eu massa. Curabitur gravida, neque a pulvinar suscipit, felis massa maximus neque, eu sagittis felis enim nec justo. Suspendisse sit amet sem a magna aliquam tincidunt. Mauris consequat ante in consequat auctor. Nam eu congue mauris, congue aliquet metus. Etiam elit ipsum, vehicula et lectus at, dignissim accumsan turpis. Sed magna nisl, tempor ut dolor sed, feugiat pharetra velit. Nulla sed purus at elit dapibus lobortis. In hac habitasse platea dictumst. Praesent quis libero id enim aliquet viverra eleifend non urna. Vivamus metus justo, dignissim eget libero molestie, tincidunt pellentesque purus. Quisque pulvinar, nisi sed egestas vestibulum, ante felis elementum justo, ut viverra nisl est sagittis leo. Curabitur pharetra eros at felis ultricies efficitur."
	"\n"
	"Ut rutrum urna vitae neque rhoncus, id dictum ex dictum. Suspendisse venenatis vel mauris sed maximus. Sed malesuada elit vel neque hendrerit, in accumsan odio sodales. Aliquam erat volutpat. Praesent non situ.\n";

/* 256 bytes of binary data */
static const unsigned char array_256[] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
	0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60,
	0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
	0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80,
	0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
	0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90,
	0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0,
	0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
	0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0,
	0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
	0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0,
	0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
	0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
	0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
	0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0,
	0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
	0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0,
	0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00
};

/* 1280 bytes of binary data */
static const unsigned char array_1280[] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
	0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60,
	0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
	0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80,
	0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
	0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90,
	0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0,
	0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
	0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0,
	0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
	0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0,
	0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
	0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
	0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
	0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0,
	0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
	0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0,
	0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
	0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60,
	0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
	0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80,
	0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
	0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90,
	0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0,
	0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
	0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0,
	0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
	0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0,
	0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
	0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
	0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
	0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0,
	0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
	0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0,
	0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
	0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60,
	0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
	0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80,
	0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
	0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90,
	0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0,
	0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
	0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0,
	0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
	0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0,
	0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
	0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
	0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
	0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0,
	0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
	0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0,
	0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
	0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60,
	0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
	0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80,
	0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
	0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90,
	0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0,
	0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
	0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0,
	0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
	0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0,
	0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
	0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
	0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
	0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0,
	0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
	0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0,
	0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
	0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60,
	0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
	0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80,
	0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
	0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90,
	0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0,
	0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
	0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0,
	0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
	0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0,
	0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
	0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
	0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
	0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0,
	0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
	0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0,
	0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00
};

static struct data {
	int len;
	const unsigned char *buf;
	bool expecting_reply;
} data[] = {
	ENTRY_OK(A),
	ENTRY_OK(foobar),
	ENTRY_OK(small_binary),
	ENTRY_OK("a bit longer data message"),
	ENTRY_OK(lorem_ipsum),
	ENTRY_OK(null_byte),
	ENTRY_OK(array_256),
	ENTRY_FAIL(array_1280), /* too long message will be discarded */

	{ 0, 0 }
};

struct client_data {
	int fd;
	int index; /* position in data[] */
	int len;
#define MAX_READ_BUF 2000
	uint8 buf[MAX_READ_BUF];
};

static inline void reverse(unsigned char *buf, int len)
{
	int i, last = len - 1;

	for(i = 0; i < len/2; i++) {
		unsigned char tmp = buf[i];
		buf[i] = buf[last - i];
		buf[last - i] = tmp;
	}
}

static int get_ifindex(const char *name)
{
	struct ifreq ifr;
	int sk, err;

	if (!name)
		return -1;

	sk = socket(PF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (sk < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name) - 1);

	err = ioctl(sk, SIOCGIFINDEX, &ifr);

	close(sk);

	if (err < 0)
		return -1;

	return ifr.ifr_ifindex;
}

static int get_address(int ifindex, int family, void *address)
{
	struct ifaddrs *ifaddr, *ifa;
	int err = -ENOENT;
	char name[IF_NAMESIZE];

	if (!if_indextoname(ifindex, name))
		return -EINVAL;

	if (getifaddrs(&ifaddr) < 0) {
		err = -errno;
		fprintf(stderr, "Cannot get addresses err %d/%s",
			err, strerror(-err));
		return err;
	}

	for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr)
			continue;

		if (strncmp(ifa->ifa_name, name, IF_NAMESIZE) == 0 &&
					ifa->ifa_addr->sa_family == family) {
			if (family == AF_INET) {
				struct sockaddr_in *in4 = (struct sockaddr_in *)
					ifa->ifa_addr;
				if (in4->sin_addr.s_addr == INADDR_ANY)
					continue;
				if ((in4->sin_addr.s_addr & IN_CLASSB_NET) ==
						((in_addr_t) 0xa9fe0000))
					continue;
				memcpy(address, &in4->sin_addr,
							sizeof(struct in_addr));
			} else if (family == AF_INET6) {
				struct sockaddr_in6 *in6 =
					(struct sockaddr_in6 *)ifa->ifa_addr;
				if (memcmp(&in6->sin6_addr, &in6addr_any,
						sizeof(struct in6_addr)) == 0)
					continue;
				if (IN6_IS_ADDR_LINKLOCAL(&in6->sin6_addr))
					continue;

				memcpy(address, &in6->sin6_addr,
						sizeof(struct in6_addr));
			} else {
				err = -EINVAL;
				goto out;
			}

			err = 0;
			break;
		}
	}

out:
	freeifaddrs(ifaddr);
	return err;
}

#define PSK_DEFAULT_IDENTITY "Client_identity"
#define PSK_DEFAULT_KEY      "secretPSK"
#define PSK_OPTIONS          "i:k:"

static bool quit = false;
static dtls_context_t *dtls_context;

static const unsigned char ecdsa_priv_key[] = {
			0x41, 0xC1, 0xCB, 0x6B, 0x51, 0x24, 0x7A, 0x14,
			0x43, 0x21, 0x43, 0x5B, 0x7A, 0x80, 0xE7, 0x14,
			0x89, 0x6A, 0x33, 0xBB, 0xAD, 0x72, 0x94, 0xCA,
			0x40, 0x14, 0x55, 0xA1, 0x94, 0xA9, 0x49, 0xFA};

static const unsigned char ecdsa_pub_key_x[] = {
			0x36, 0xDF, 0xE2, 0xC6, 0xF9, 0xF2, 0xED, 0x29,
			0xDA, 0x0A, 0x9A, 0x8F, 0x62, 0x68, 0x4E, 0x91,
			0x63, 0x75, 0xBA, 0x10, 0x30, 0x0C, 0x28, 0xC5,
			0xE4, 0x7C, 0xFB, 0xF2, 0x5F, 0xA5, 0x8F, 0x52};

static const unsigned char ecdsa_pub_key_y[] = {
			0x71, 0xA0, 0xD4, 0xFC, 0xDE, 0x1A, 0xB8, 0x78,
			0x5A, 0x3C, 0x78, 0x69, 0x35, 0xA7, 0xCF, 0xAB,
			0xE9, 0x3F, 0x98, 0x72, 0x09, 0xDA, 0xED, 0x0B,
			0x4F, 0xAB, 0xC3, 0x6F, 0xC7, 0x72, 0xF8, 0x29};

#ifdef DTLS_PSK
/* The PSK information for DTLS */
#define PSK_ID_MAXLEN 256
#define PSK_MAXLEN 256
static unsigned char psk_id[PSK_ID_MAXLEN];
static size_t psk_id_length = 0;
static unsigned char psk_key[PSK_MAXLEN];
static size_t psk_key_length = 0;

/* This function is the "key store" for tinyDTLS. It is called to
 * retrieve a key for the given identity within this particular
 * session. */
static int get_psk_info(struct dtls_context_t *ctx UNUSED_PARAM,
			const session_t *session UNUSED_PARAM,
			dtls_credentials_type_t type,
			const unsigned char *id, size_t id_len,
			unsigned char *result, size_t result_length)
{
	switch (type) {
	case DTLS_PSK_IDENTITY:
		if (id_len) {
			dtls_debug("got psk_identity_hint: '%.*s'\n", id_len,
				   id);
		}

		if (result_length < psk_id_length) {
			dtls_warn("cannot set psk_identity -- buffer too small\n");
			return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
		}

		memcpy(result, psk_id, psk_id_length);
		return psk_id_length;

	case DTLS_PSK_KEY:
		if (id_len != psk_id_length || memcmp(psk_id, id, id_len) != 0) {
			dtls_warn("PSK for unknown id requested, exiting\n");
			return dtls_alert_fatal_create(DTLS_ALERT_ILLEGAL_PARAMETER);
		} else if (result_length < psk_key_length) {
			dtls_warn("cannot set psk -- buffer too small\n");
			return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
		}

		memcpy(result, psk_key, psk_key_length);
		return psk_key_length;

	default:
		dtls_warn("unsupported request type: %d\n", type);
	}

	return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
}
#endif /* DTLS_PSK */

#ifdef DTLS_ECC
static int get_ecdsa_key(struct dtls_context_t *ctx,
			 const session_t *session,
			 const dtls_ecdsa_key_t **result)
{
	static const dtls_ecdsa_key_t ecdsa_key = {
		.curve = DTLS_ECDH_CURVE_SECP256R1,
		.priv_key = ecdsa_priv_key,
		.pub_key_x = ecdsa_pub_key_x,
		.pub_key_y = ecdsa_pub_key_y
	};

	*result = &ecdsa_key;
	return 0;
}

static int verify_ecdsa_key(struct dtls_context_t *ctx,
			    const session_t *session,
			    const unsigned char *other_pub_x,
			    const unsigned char *other_pub_y,
			    size_t key_size)
{
	return 0;
}
#endif /* DTLS_ECC */

static void print_data(const unsigned char *packet, int length)
{
	int n = 0;

	while (length--) {
		if (n % 16 == 0)
			printf("%X: ", n);

		printf("%X ", *packet++);

		n++;
		if (n % 8 == 0) {
			if (n % 16 == 0)
				printf("\n");
			else
				printf(" ");
		}
	}
	printf("\n");
}

static void try_send(struct dtls_context_t *ctx, session_t *dst)
{
	struct client_data *user_data =
			(struct client_data *)dtls_get_app_data(ctx);
	int ret;

	printf("Sending [%d] %d bytes\n",
	       user_data->index, data[user_data->index].len);

	if (debug)
		print_data(data[user_data->index].buf,
			   data[user_data->index].len);

	ret = dtls_write(ctx, dst,
			 (uint8 *)data[user_data->index].buf,
			 data[user_data->index].len);
	if (ret < 0) {
		/* Failure */
		quit = true;
	}
}

static int read_from_peer(struct dtls_context_t *ctx,
			  session_t *session,
			  uint8 *read_data, size_t read_len)
{
	struct client_data *user_data =
			(struct client_data *)dtls_get_app_data(ctx);

	printf("Read [%d] from peer %d bytes\n", user_data->index, read_len);

	reverse(read_data, read_len);

	if (debug)
		print_data(read_data, read_len);

	if (data[user_data->index].expecting_reply &&
	    (data[user_data->index].len != read_len ||
	     memcmp(data[user_data->index].buf, read_data, read_len) != 0)) {
		fprintf(stderr, "Check failed [%d] len %d\n",
			user_data->index, read_len);
		quit = true;
	}
	user_data->index++;
	if (!data[user_data->index].buf) {
		/* last entry, just bail out */
		quit = true;
		return 0;
	}
	if (user_data->index == renegotiate) {
		printf("Starting to renegotiate keys\n");
		dtls_renegotiate(ctx, session);
		return;
	}

	try_send(ctx, session);
	return 0;
}

static inline void sleep_ms(int ms)
{
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = ms * 1000;

	select(1, NULL, NULL, NULL, &tv);
}

static int send_to_peer(struct dtls_context_t *ctx,
			session_t *session,
			uint8 *data, size_t len)
{
	struct client_data *user_data =
			(struct client_data *)dtls_get_app_data(ctx);

	/* The Qemu uart driver can loose chars if sent too fast.
	 * So before sending more data, sleep a while.
	 */
	sleep_ms(200);

	printf("Sending to peer data %p len %d\n", data, len);
	return sendto(user_data->fd, data, len, 0,
		      &session->addr.sa, session->size);
}

static int dtls_handle_read(struct dtls_context_t *ctx)
{
	struct client_data *user_data;
	session_t session;

	user_data = (struct client_data *)dtls_get_app_data(ctx);

	if (!user_data || !user_data->fd)
		return -1;

	memset(&session, 0, sizeof(session_t));
	session.size = sizeof(session.addr);
	user_data->len = recvfrom(user_data->fd, user_data->buf, MAX_READ_BUF,
				  0, &session.addr.sa, &session.size);
	if (user_data->len < 0) {
		perror("recvfrom");
		return -1;
	} else {
		dtls_dsrv_log_addr(DTLS_LOG_DEBUG, "peer", &session);
		dtls_debug_dump("bytes from peer", user_data->buf,
				user_data->len);
	}

	return dtls_handle_message(ctx, &session, user_data->buf,
				   user_data->len);
}

static int handle_event(struct dtls_context_t *ctx, session_t *session,
			dtls_alert_level_t level, unsigned short code)
{
	if (debug)
		printf("event: level %d code %d\n", level, code);

	if (level > 0) {
		/* alert code, quit */
		quit = true;
	} else if (level == 0) {
		/* internal event */
		if (code == DTLS_EVENT_CONNECTED) {
			/* We can send data now */
			try_send(ctx, session);
		}
	}

	return 0;
}

static dtls_handler_t cb = {
	.write = send_to_peer,
	.read  = read_from_peer,
	.event = handle_event,
#ifdef DTLS_PSK
	.get_psk_info = get_psk_info,
#endif /* DTLS_PSK */
#ifdef DTLS_ECC
	.get_ecdsa_key = get_ecdsa_key,
	.verify_ecdsa_key = verify_ecdsa_key
#endif /* DTLS_ECC */
};

void signal_handler(int signum)
{
	switch (signum) {
	case SIGINT:
	case SIGTERM:
		quit = true;
		break;
	}
}

extern int optind, opterr, optopt;
extern char *optarg;

/* The application returns:
 *    < 0 : connection or similar error
 *      0 : no errors, all tests passed
 *    > 0 : could not send all the data to server
 */
int main(int argc, char**argv)
{
	int c, ret, fd, timeout = 0;
	struct sockaddr_in6 addr6_send = { 0 }, addr6_recv = { 0 };
	struct sockaddr_in addr4_send = { 0 }, addr4_recv = { 0 };
	struct sockaddr *addr_send, *addr_recv;
	int family, addr_len;
	const struct in6_addr any = IN6ADDR_ANY_INIT;
	const char *target = NULL, *interface = NULL;
	fd_set rfds;
	struct timeval tv = {};
	int ifindex = -1, on, port;
	void *address = NULL;
	session_t dst;
	struct client_data user_data;

#ifdef DTLS_PSK
	psk_id_length = strlen(PSK_DEFAULT_IDENTITY);
	psk_key_length = strlen(PSK_DEFAULT_KEY);
	memcpy(psk_id, PSK_DEFAULT_IDENTITY, psk_id_length);
	memcpy(psk_key, PSK_DEFAULT_KEY, psk_key_length);
#endif /* DTLS_PSK */

	opterr = 0;

	while ((c = getopt(argc, argv, "i:Dr")) != -1) {
		switch (c) {
		case 'i':
			interface = optarg;
			break;
		case 'r':
			/* Do a renegotiate once during the test run. */
			srandom(time(0));
			renegotiate = random() %
				(sizeof(data) / sizeof(struct data) - 1 - 1);
			printf("Renegotating after %d messages.\n", renegotiate);
			break;
		case 'D':
			debug = true;
			break;
		}
	}

	if (optind < argc)
		target = argv[optind];

	if (!target) {
		printf("usage: %s [-i tun0] [-D] [-r] <IPv{6|4} address of the dtls-server>\n",
		       argv[0]);
		printf("\n-i Use this network interface.\n");
		printf("\n-r Renegoating keys once during the test run.\n");
		printf("\n-D Activate debugging.\n");
		exit(-EINVAL);
	}

	if (inet_pton(AF_INET6, target, &addr6_send.sin6_addr) != 1) {
		if (inet_pton(AF_INET, target, &addr4_send.sin_addr) != 1) {
			printf("Invalid address family\n");
			exit(-EINVAL);
		} else {
			addr_send = (struct sockaddr *)&addr4_send;
			addr_recv = (struct sockaddr *)&addr4_recv;
			addr4_send.sin_port = port = htons(SERVER_PORT);
			addr4_recv.sin_family = AF_INET;
			addr4_recv.sin_addr.s_addr = INADDR_ANY;
			addr4_recv.sin_port = htons(CLIENT_PORT);
			family = AF_INET;
			addr_len = sizeof(addr4_send);
			address = &addr4_recv.sin_addr;
		}
	} else {
		addr_send = (struct sockaddr *)&addr6_send;
		addr_recv = (struct sockaddr *)&addr6_recv;
		addr6_send.sin6_port = port = htons(SERVER_PORT);
		addr6_recv.sin6_family = AF_INET6;
		addr6_recv.sin6_addr = any;
		addr6_recv.sin6_port = htons(CLIENT_PORT);
		family = AF_INET6;
		addr_len = sizeof(addr6_send);
		address = &addr6_recv.sin6_addr;
	}

	addr_send->sa_family = family;
	addr_recv->sa_family = family;

	fd = socket(family, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0) {
		perror("socket");
		exit(-errno);
	}

	if (interface) {
		struct ifreq ifr;
		char addr_buf[INET6_ADDRSTRLEN];

		memset(&ifr, 0, sizeof(ifr));
		snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), interface);

		if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE,
			       (void *)&ifr, sizeof(ifr)) < 0) {
			perror("SO_BINDTODEVICE");
			exit(-errno);
		}

		ifindex = get_ifindex(interface);
		if (ifindex < 0) {
			printf("Invalid interface %s\n", interface);
			exit(-EINVAL);
		}

		get_address(ifindex, family, address);

		printf("Binding to %s\n", inet_ntop(family, address,
					    addr_buf, sizeof(addr_buf)));
	}

	ret = bind(fd, addr_recv, addr_len);
	if (ret < 0) {
		perror("bind");
		exit(-errno);
	}

	on = 1;
#ifdef IPV6_RECVPKTINFO
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on,
		       sizeof(on) ) < 0) {
#else /* IPV6_RECVPKTINFO */
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_PKTINFO, &on,
		       sizeof(on) ) < 0) {
#endif /* IPV6_RECVPKTINFO */
		printf("setsockopt IPV6_PKTINFO: %s\n", strerror(errno));
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	dtls_init();

	user_data.fd = fd;
	user_data.index = 0;

	dtls_context = dtls_new_context(&user_data);
	if (!dtls_context) {
		dtls_emerg("cannot create context\n");
		exit(-EINVAL);
	}

	dtls_set_handler(dtls_context, &cb);

	if (debug)
		dtls_set_log_level(DTLS_LOG_DEBUG);

	memset(&dst, 0, sizeof(dst));
	dst.addr.sin.sin_port = port;
	dst.size = addr_len;
	memcpy(&dst.addr.sa, addr_send, addr_len);

	dtls_connect(dtls_context, &dst);

	do {
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		tv.tv_sec = MAX_TIMEOUT;
		tv.tv_usec = 0;

		ret = select(fd + 1, &rfds, NULL, NULL, &tv);
		if (ret < 0) {
			perror("select");
			break;
		} else if (ret == 0) {
			if (quit)
				break;

			if (user_data.index >
			    (sizeof(data) / sizeof(struct data)) - 1)
				break;

			if (!data[user_data.index].expecting_reply) {
				printf("Did not expect a reply, send next entry.\n");
				user_data.index++;
				if (!data[user_data.index].buf)
					break;

				continue;
			}

			fprintf(stderr,	"Timeout [%d] while waiting len %d\n",
				user_data.index, data[user_data.index].len);
			ret = user_data.index + 1;
			break;
		} else if (!FD_ISSET(fd, &rfds)) {
			fprintf(stderr, "Invalid fd in read, quitting.\n");
			ret = user_data.index + 1;
			break;
		}

		if (dtls_handle_read(dtls_context) < 0) {
			fprintf(stderr, "Peer connection failed.\n");
			ret = user_data.index + 1;
			break;
		}

	} while(!quit);

	printf("\n");

	dtls_close(dtls_context, &dst);

	dtls_free_context(dtls_context);

	close(fd);

	exit(ret);
}
