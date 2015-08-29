/*
 * Copyright (c) 2012, 2014 Wind River Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3) Neither the name of Wind River Systems nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @brief Microkernel command packet library
 */

#include <nanokernel.h>
#include <arch/cpu.h>
#include <microkernel/command_packet.h>
#include <micro_private.h>
#include <sections.h>

/**
 * Generate build error by defining a negative-size array if the hard-coded
 * command packet size differs from the actual size; otherwise, define
 * a zero-element array that gets thrown away by linker
 */
uint32_t _k_test_cmd_pkt_size
	[0 - ((CMD_PKT_SIZE_IN_WORDS * sizeof(uint32_t)) != sizeof(struct k_args))];

/**
 * @brief Get the next command packet
 *
 * This routine gets the next command packet from the specified set.
 * @param pSet Pointer to set of command packets
 *
 * @return pointer to the command packet
 */
cmdPkt_t *_cmd_pkt_get(struct cmd_pkt_set *pSet)
{
	uint32_t index; /* index into command packet array */
	int key;	/* interrupt lock level */

	key = irq_lock();
	index = pSet->index;
	pSet->index++;
	if (pSet->index >= pSet->num_packets)
		pSet->index = 0;
	irq_unlock(key);

	return &pSet->command_packet[index];
}

/**
 *
 * @brief Send command packet to be processed by _k_server
 * @param cmd_packet Arguments
 * @return N/A
 */
void _k_task_call(struct k_args *cmd_packet)
{
	cmd_packet->alloc = false;
	_k_current_task->args = cmd_packet;
	nano_task_stack_push(&_k_command_stack, (uint32_t)cmd_packet);
}
