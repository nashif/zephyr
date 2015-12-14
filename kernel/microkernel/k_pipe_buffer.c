/* k_pipe_buffer.c */

/*
 * Copyright (c) 1997-2015 Wind River Systems, Inc.
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


/* Implementation remarks:
 * - when using a floating end pointer: do not use pipe_desc->iBuffsize for
 *  (pipe_desc->end_ptr - pipe_desc->begin_ptr)
 */

#include <microkernel/base_api.h>
#include <k_pipe_buffer.h>
#include <string.h>
#include <toolchain.h>
#include <sections.h>
#include <misc/__assert.h>

#define STORE_NBR_MARKERS
/* NOTE: the number of pending write and read Xfers is always stored,
 * as it is required for the pipes to function properly. It is stored in the
 * pipe descriptor fields num_pending_writes and num_pending_reads.
 *
 * In the Writer and Reader MarkersList, the number of markers (==nbr. of
 * unreleased Xfers) is monitored as well. They actually equal
 * num_pending_writes and num_pending_reads.
 * Their existence depends on STORE_NBR_MARKERS. A reason to have them
 * additionally is that some extra consistency checking is performed in the
 * markers manipulation functionality itself.
 * Drawback: double storage of nbr. of pending write Xfers (but for test
 * purposes this is acceptable I think)
 */

#define CHECK_BUFFER_POINTER(data_ptr) \
	__ASSERT_NO_MSG(desc->begin_ptr <= data_ptr && data_ptr < desc->end_ptr)

static void pipe_intrusion_check(struct _k_pipe_desc *desc,
				 unsigned char *begin_ptr,
				 int size);

/**
 * Markers
 */

static int MarkerFindFree(struct _k_pipe_marker markers[])
{
	struct _k_pipe_marker *pM = markers;
	int i;

	for (i = 0; i < MAXNBR_PIPE_MARKERS; i++, pM++) {
		if (pM->pointer == NULL) {
			break;
		}
	}
	if (i == MAXNBR_PIPE_MARKERS) {
		i = -1;
	}

	return i;
}

static void MarkerLinkToListAfter(struct _k_pipe_marker markers[],
				  int iMarker,
				  int iNewMarker)
{
	int iNextMarker; /* index of next marker in original list */

	/* let the original list be aware of the new marker */
	if (iMarker != -1) {
		iNextMarker = markers[iMarker].next;
		markers[iMarker].next = iNewMarker;
		if (iNextMarker != -1) {
			markers[iNextMarker].prev = iNewMarker;
		} else {
			/* there was no next marker */
		}
	} else {
		iNextMarker = -1; /* there wasn't even a marker */
	}

	/* link the new marker with the marker and next marker */
	markers[iNewMarker].prev = iMarker;
	markers[iNewMarker].next = iNextMarker;
}

static int MarkerAddLast(struct _k_pipe_marker_list *pMarkerList,
			 unsigned char *pointer,
			 int size,
			 bool buffer_xfer_busy)
{
	int i = MarkerFindFree(pMarkerList->markers);

	if (i == -1) {
		return i;
	}

	pMarkerList->markers[i].pointer = pointer;
	pMarkerList->markers[i].size = size;
	pMarkerList->markers[i].buffer_xfer_busy = buffer_xfer_busy;

	if (pMarkerList->first_marker == -1) {
		__ASSERT_NO_MSG(pMarkerList->last_marker == -1);
		pMarkerList->first_marker = i; /* we still need to set prev & next */
	} else {
		__ASSERT_NO_MSG(pMarkerList->last_marker != -1);
		__ASSERT_NO_MSG(
			pMarkerList->markers[pMarkerList->last_marker].next == -1);
	}

	MarkerLinkToListAfter(pMarkerList->markers, pMarkerList->last_marker, i);

	__ASSERT_NO_MSG(pMarkerList->markers[i].next == -1);
	pMarkerList->last_marker = i;

#ifdef STORE_NBR_MARKERS
	pMarkerList->num_markers++;
	__ASSERT_NO_MSG(pMarkerList->num_markers > 0);
#endif

	return i;
}

static void MarkerUnlinkFromList(struct _k_pipe_marker markers[],
				 int iMarker,
				 int *piPredecessor,
				 int *piSuccessor)
{
	int iNextMarker = markers[iMarker].next;
	int iPrevMarker = markers[iMarker].prev;

	/* remove the marker from the list */
	markers[iMarker].next = -1;
	markers[iMarker].prev = -1;

	/* repair the chain */
	if (iPrevMarker != -1) {
		markers[iPrevMarker].next = iNextMarker;
	}
	if (iNextMarker != -1) {
		markers[iNextMarker].prev = iPrevMarker;
	}
	*piPredecessor = iPrevMarker;
	*piSuccessor = iNextMarker;
}

static void MarkerDelete(struct _k_pipe_marker_list *pMarkerList, int index)
{
	int i;
	int iPredecessor;
	int iSuccessor;

	i = index;

	__ASSERT_NO_MSG(i != -1);

	pMarkerList->markers[i].pointer = NULL;
	MarkerUnlinkFromList(pMarkerList->markers, i,
			     &iPredecessor, &iSuccessor);

	/* update first/last info */
	if (i == pMarkerList->last_marker) {
		pMarkerList->last_marker = iPredecessor;
	}
	if (i == pMarkerList->first_marker) {
		pMarkerList->first_marker = iSuccessor;
	}

#ifdef STORE_NBR_MARKERS
	pMarkerList->num_markers--;
	__ASSERT_NO_MSG(pMarkerList->num_markers >= 0);

	if (pMarkerList->num_markers == 0) {
		__ASSERT_NO_MSG(pMarkerList->first_marker == -1);
		__ASSERT_NO_MSG(pMarkerList->last_marker == -1);
	}
#endif
}

static void MarkersClear(struct _k_pipe_marker_list *pMarkerList)
{
	struct _k_pipe_marker *pM = pMarkerList->markers;
	int i;

	for (i = 0; i < MAXNBR_PIPE_MARKERS; i++, pM++) {
		memset(pM, 0, sizeof(struct _k_pipe_marker));
		pM->next = -1;
		pM->prev = -1;
	}
#ifdef STORE_NBR_MARKERS
	pMarkerList->num_markers = 0;
#endif
	pMarkerList->first_marker = -1;
	pMarkerList->last_marker = -1;
	pMarkerList->post_wrap_around_marker = -1;
}

/**/

/* note on setting/clearing markers/guards:
 *
 * If there is at least one marker, there is a guard and equals one of the
 * markers; if there are no markers (*), there is no guard.
 * Consequently, if a marker is add when there were none, the guard will equal
 * it. If additional markers are add, the guard will not change.
 * However, if a marker is deleted:
 *  if it equals the guard a new guard must be selected (**)
 *  if not, guard doesn't change
 *
 * (*) we need to housekeep how much markers there are or we can inspect the
 * guard
 * (**) for this, the complete markers table needs to be investigated
 */

/*
 * This function will see if one or more 'areas' in the buffer can be made
 * available (either for writing xor reading).
 * Note: such a series of areas starts from the beginning.
 */
static int ScanMarkers(struct _k_pipe_marker_list *pMarkerList,
		       int *piSizeBWA, int *piSizeAWA, int *piNbrPendingXfers)
{
	struct _k_pipe_marker *pM;
	bool bMarkersAreNowAWA;
	int index;

	index = pMarkerList->first_marker;

	__ASSERT_NO_MSG(index != -1);

	bMarkersAreNowAWA = false;
	do {
		int index_next;

		__ASSERT_NO_MSG(index == pMarkerList->first_marker);

		if (index == pMarkerList->post_wrap_around_marker) {
			/* from now on, everything is AWA */
			bMarkersAreNowAWA = true;
		}

		pM = &(pMarkerList->markers[index]);

		if (pM->buffer_xfer_busy == true) {
			break;
		}

		if (!bMarkersAreNowAWA) {
			*piSizeBWA += pM->size;
		} else {
			*piSizeAWA += pM->size;
		}

		index_next = pM->next;
		/* pMarkerList->first_marker will be updated */
		MarkerDelete(pMarkerList, index);
		/* adjust *piNbrPendingXfers */
		if (piNbrPendingXfers) {
			__ASSERT_NO_MSG(*piNbrPendingXfers >= 0);
			(*piNbrPendingXfers)--;
		}
		index = index_next;
	} while (index != -1);

	__ASSERT_NO_MSG(index == pMarkerList->first_marker);

	if (bMarkersAreNowAWA) {
		pMarkerList->post_wrap_around_marker =
			pMarkerList->first_marker;
	}

#ifdef STORE_NBR_MARKERS
	if (pMarkerList->num_markers == 0) {
		__ASSERT_NO_MSG(pMarkerList->first_marker == -1);
		__ASSERT_NO_MSG(pMarkerList->last_marker == -1);
		__ASSERT_NO_MSG(pMarkerList->post_wrap_around_marker == -1);
	}
#endif

	return pMarkerList->first_marker;
}

/**
 * General
 */

void BuffInit(unsigned char *pBuffer,
	      int *piBuffSize,
	      struct _k_pipe_desc *desc)
{
	desc->begin_ptr = pBuffer;

	desc->buffer_size = *piBuffSize;

	/* reset all pointers */

	desc->end_ptr = desc->begin_ptr +
		OCTET_TO_SIZEOFUNIT(desc->buffer_size);
	desc->original_end_ptr = desc->end_ptr;

	/* assumed it is allowed */
	desc->buffer_state = BUFF_EMPTY;
	desc->end_ptr = desc->original_end_ptr;
	desc->write_ptr = desc->begin_ptr;
	desc->write_guard = NULL;
	desc->wrap_around_write = false;
	desc->read_ptr = desc->begin_ptr;
	desc->read_guard = NULL;
	desc->wrap_around_read = true; /* YES!! */
	desc->free_space_count = desc->buffer_size;
	desc->free_space_post_wrap_around = 0;
	desc->num_pending_reads = 0;
	desc->available_data_count = 0;
	desc->available_data_post_wrap_around = 0;
	desc->num_pending_writes = 0;
	MarkersClear(&desc->write_markers);
	MarkersClear(&desc->read_markers);

}

int CalcFreeSpace(struct _k_pipe_desc *desc, int *free_space_count_ptr,
				  int *free_space_post_wrap_around_ptr)
{
	unsigned char *pStart = desc->write_ptr;
	unsigned char *pStop = desc->read_ptr;

	if (desc->write_guard != NULL) {
		pStop = desc->write_guard;
	} else {
		/*
		 * if buffer_state==BUFF_EMPTY but we have a WriteGuard,
		 * we still need to calculate it as a normal [Start,Stop]
		 * interval
		 */

		if (desc->buffer_state == BUFF_EMPTY) {
			*free_space_count_ptr =
				SIZEOFUNIT_TO_OCTET(desc->end_ptr - pStart);
			*free_space_post_wrap_around_ptr =
				SIZEOFUNIT_TO_OCTET(pStop - desc->begin_ptr);
			return (*free_space_count_ptr + *free_space_post_wrap_around_ptr);
			/* this sum equals end_ptr-begin_ptr */
		}
	}

	/*
	 * on the other hand, if buffer_state is full, we do not need a special
	 * flow; it will be correct as (pStop - pStart) equals 0
	 */

	if (pStop >= pStart) {
		*free_space_count_ptr = SIZEOFUNIT_TO_OCTET(pStop - pStart);
		*free_space_post_wrap_around_ptr = 0;
	} else {
		*free_space_count_ptr =
			SIZEOFUNIT_TO_OCTET(desc->end_ptr - pStart);
		*free_space_post_wrap_around_ptr =
			SIZEOFUNIT_TO_OCTET(pStop - desc->begin_ptr);
	}
	return (*free_space_count_ptr + *free_space_post_wrap_around_ptr);
}

void BuffGetFreeSpace(struct _k_pipe_desc *desc,
		      int *piFreeSpaceTotal,
		      int *free_space_count_ptr,
		      int *free_space_post_wrap_around_ptr)
{
	int free_space_count;
	int free_space_post_wrap_around;
	int iFreeSpaceTotal;

	iFreeSpaceTotal =
		CalcFreeSpace(desc, &free_space_count,
			      &free_space_post_wrap_around);
	__ASSERT_NO_MSG(free_space_count == desc->free_space_count);
	__ASSERT_NO_MSG(free_space_post_wrap_around == desc->free_space_post_wrap_around);
	*piFreeSpaceTotal = iFreeSpaceTotal;
	*free_space_count_ptr = desc->free_space_count;
	*free_space_post_wrap_around_ptr = desc->free_space_post_wrap_around;
}

void BuffGetFreeSpaceTotal(struct _k_pipe_desc *desc, int *piFreeSpaceTotal)
{
	int dummy1, dummy2;
	*piFreeSpaceTotal = CalcFreeSpace(desc, &dummy1, &dummy2);
	__ASSERT_NO_MSG(dummy1 == desc->free_space_count);
	__ASSERT_NO_MSG(dummy2 == desc->free_space_post_wrap_around);
}

int BuffEmpty(struct _k_pipe_desc *desc)
{
	/* 0==iAvailDataTotal is an INcorrect condition b/c of async behavior */

	int iTotalFreeSpace;

	BuffGetFreeSpaceTotal(desc, &iTotalFreeSpace);
	return (desc->buffer_size == iTotalFreeSpace);
}

int CalcAvailData(struct _k_pipe_desc *desc, int *available_data_count_ptr,
				  int *available_data_post_wrap_around_ptr)
{
	unsigned char *pStart = desc->read_ptr;
	unsigned char *pStop = desc->write_ptr;

	if (NULL != desc->read_guard) {
		pStop = desc->read_guard;
	} else {
		/*
		 * if buffer_state==BUFF_FULL but we have a ReadGuard,
		 * we still need to calculate it as a normal [Start,Stop] interval
		 */

		if (BUFF_FULL == desc->buffer_state) {
			*available_data_count_ptr =
				SIZEOFUNIT_TO_OCTET(desc->end_ptr - pStart);
			*available_data_post_wrap_around_ptr =
				SIZEOFUNIT_TO_OCTET(pStop - desc->begin_ptr);
			return (*available_data_count_ptr + *available_data_post_wrap_around_ptr);
			/* this sum equals end_ptr-begin_ptr */
		}
	}

	/*
	 * on the other hand, if buffer_state is empty, we do not need a
	 * special flow; it will be correct as (pStop - pStart) equals 0
	 */

	if (pStop >= pStart) {
		*available_data_count_ptr = SIZEOFUNIT_TO_OCTET(pStop - pStart);
		*available_data_post_wrap_around_ptr = 0;
	} else {
		*available_data_count_ptr =
			SIZEOFUNIT_TO_OCTET(desc->end_ptr - pStart);
		*available_data_post_wrap_around_ptr =
			SIZEOFUNIT_TO_OCTET(pStop - desc->begin_ptr);
	}
	return (*available_data_count_ptr + *available_data_post_wrap_around_ptr);
}

void BuffGetAvailData(struct _k_pipe_desc *desc,
		      int *piAvailDataTotal,
		      int *available_data_count_ptr,
		      int *available_data_post_wrap_around_ptr)
{
	int available_data_count;
	int available_data_post_wrap_around;
	int iAvailDataTotal;

	iAvailDataTotal = CalcAvailData(desc, &available_data_count,
					&available_data_post_wrap_around);
	__ASSERT_NO_MSG(available_data_count == desc->available_data_count);
	__ASSERT_NO_MSG(available_data_post_wrap_around == desc->available_data_post_wrap_around);
	*piAvailDataTotal = iAvailDataTotal;
	*available_data_count_ptr = desc->available_data_count;
	*available_data_post_wrap_around_ptr =
		desc->available_data_post_wrap_around;
}

void BuffGetAvailDataTotal(struct _k_pipe_desc *desc, int *piAvailDataTotal)
{
	int dummy1, dummy2;

	*piAvailDataTotal = CalcAvailData(desc, &dummy1, &dummy2);
	__ASSERT_NO_MSG(dummy1 == desc->available_data_count);
	__ASSERT_NO_MSG(dummy2 == desc->available_data_post_wrap_around);
}

int BuffFull(struct _k_pipe_desc *desc)
{
	/* 0==iTotalFreeSpace is an INcorrect condition b/c of async behavior */

	int iAvailDataTotal;

	BuffGetAvailDataTotal(desc, &iAvailDataTotal);
	return (desc->buffer_size == iAvailDataTotal);
}

/**
 * Buffer en-queuing:
 */

static int AsyncEnQRegstr(struct _k_pipe_desc *desc, int size)
{
	int i;

	pipe_intrusion_check(desc, desc->write_ptr, size);

	i = MarkerAddLast(&desc->write_markers, desc->write_ptr, size, true);
	if (i != -1) {
		/* adjust num_pending_writes */
		__ASSERT_NO_MSG(desc->num_pending_writes >= 0);
		desc->num_pending_writes++;
		/* read_guard changes? */
		if (desc->read_guard == NULL) {
			desc->read_guard = desc->write_ptr;
		}
		__ASSERT_NO_MSG(desc->write_markers.markers
				[desc->write_markers.first_marker].pointer ==
						desc->read_guard);
		/* post_wrap_around_marker changes? */
		if (desc->write_markers.post_wrap_around_marker == -1 &&
		    desc->wrap_around_write) {
			desc->write_markers.post_wrap_around_marker = i;
		}
	}
	return i;
}

static void AsyncEnQFinished(struct _k_pipe_desc *desc, int iTransferID)
{
	desc->write_markers.markers[iTransferID].buffer_xfer_busy = false;

	if (desc->write_markers.first_marker == iTransferID) {
		int iNewFirstMarker = ScanMarkers(&desc->write_markers,
						  &desc->available_data_count,
						  &desc->available_data_post_wrap_around,
						  &desc->num_pending_writes);
		if (iNewFirstMarker != -1) {
			desc->read_guard =
				desc->write_markers.markers[iNewFirstMarker].pointer;
		} else {
			desc->read_guard = NULL;
		}
	}
}

int BuffEnQ(struct _k_pipe_desc *desc, int size, unsigned char **ppWrite)
{
	int iTransferID;

	if (BuffEnQA(desc, size, ppWrite, &iTransferID) == 0) {
		return 0;
	}

	/* check ret value */

	BuffEnQA_End(desc, iTransferID, size /* optional */);
	return size;
}

int BuffEnQA(struct _k_pipe_desc *desc, int size, unsigned char **ppWrite,
			 int *piTransferID)
{
	if (size > desc->free_space_count) {
		return 0;
	}
	*piTransferID = AsyncEnQRegstr(desc, size);
	if (*piTransferID == -1) {
		return 0;
	}

	*ppWrite = desc->write_ptr;

	/* adjust write pointer and free space*/

	desc->write_ptr += OCTET_TO_SIZEOFUNIT(size);
	if (desc->end_ptr == desc->write_ptr) {
		desc->write_ptr = desc->begin_ptr;
		desc->free_space_count = desc->free_space_post_wrap_around;
		desc->free_space_post_wrap_around = 0;
		desc->wrap_around_write = true;
		desc->wrap_around_read = false;
		desc->read_markers.post_wrap_around_marker = -1;
	} else {
		desc->free_space_count -= size;
	}

	if (desc->write_ptr == desc->read_ptr) {
		desc->buffer_state = BUFF_FULL;
	} else {
		desc->buffer_state = BUFF_OTHER;
	}

	CHECK_BUFFER_POINTER(desc->write_ptr);

	return size;
}

void BuffEnQA_End(struct _k_pipe_desc *desc, int iTransferID,
				  int size /* optional */)
{
	ARG_UNUSED(size);

	/* An asynchronous data transfer to the buffer has finished */

	AsyncEnQFinished(desc, iTransferID);
}

/**
 * Buffer de-queuing:
 */

static int AsyncDeQRegstr(struct _k_pipe_desc *desc, int size)
{
	int i;

	pipe_intrusion_check(desc, desc->read_ptr, size);

	i = MarkerAddLast(&desc->read_markers, desc->read_ptr, size, true);
	if (i != -1) {
		/* adjust num_pending_reads */
		__ASSERT_NO_MSG(desc->num_pending_reads >= 0);
		desc->num_pending_reads++;
		/* write_guard changes? */
		if (desc->write_guard == NULL) {
			desc->write_guard = desc->read_ptr;
		}
		__ASSERT_NO_MSG(desc->read_markers.markers
			[desc->read_markers.first_marker].pointer ==
						desc->write_guard);
		/* post_wrap_around_marker changes? */
		if (desc->read_markers.post_wrap_around_marker == -1 &&
		    desc->wrap_around_read) {
			desc->read_markers.post_wrap_around_marker = i;
		}
	}
	return i;
}

static void AsyncDeQFinished(struct _k_pipe_desc *desc, int iTransferID)
{
	desc->read_markers.markers[iTransferID].buffer_xfer_busy = false;

	if (desc->read_markers.first_marker == iTransferID) {
		int iNewFirstMarker = ScanMarkers(&desc->read_markers,
						  &desc->free_space_count,
						  &desc->free_space_post_wrap_around,
						  &desc->num_pending_reads);
		if (iNewFirstMarker != -1) {
			desc->write_guard =
			desc->read_markers.markers[iNewFirstMarker].pointer;
		} else {
			desc->write_guard = NULL;
		}
	}
}

int BuffDeQ(struct _k_pipe_desc *desc, int size, unsigned char **ppRead)
{
	int iTransferID;

	if (BuffDeQA(desc, size, ppRead, &iTransferID) == 0) {
		return 0;
	}
	BuffDeQA_End(desc, iTransferID, size /* optional */);
	return size;
}

int BuffDeQA(struct _k_pipe_desc *desc, int size, unsigned char **ppRead,
			 int *piTransferID)
{
	/* asynchronous data transfer; read guard pointers must be set */

	if (size > desc->available_data_count) {
		/* free space is from read to guard pointer/end pointer */
		return 0;
	}
	*piTransferID = AsyncDeQRegstr(desc, size);
	if (*piTransferID == -1) {
		return 0;
	}

	*ppRead = desc->read_ptr;

	/* adjust read pointer and avail data */

	desc->read_ptr += OCTET_TO_SIZEOFUNIT(size);
	if (desc->end_ptr == desc->read_ptr) {
		desc->read_ptr = desc->begin_ptr;
		desc->available_data_count =
			desc->available_data_post_wrap_around;
		desc->available_data_post_wrap_around = 0;
		desc->wrap_around_write = false;
		desc->wrap_around_read = true;
		desc->write_markers.post_wrap_around_marker = -1;
	} else {
		desc->available_data_count -= size;
	}

	if (desc->write_ptr == desc->read_ptr) {
		desc->buffer_state = BUFF_EMPTY;
	} else {
		desc->buffer_state = BUFF_OTHER;
	}

	CHECK_BUFFER_POINTER(desc->read_ptr);

	return size;
}

void BuffDeQA_End(struct _k_pipe_desc *desc, int iTransferID,
				  int size /* optional */)
{
	ARG_UNUSED(size);

	/* An asynchronous data transfer from the buffer has finished */

	AsyncDeQFinished(desc, iTransferID);
}

/**
 * Buffer instrusion
 */

static bool AreasCheck4Intrusion(unsigned char *pBegin1, int iSize1,
				 unsigned char *pBegin2, int iSize2)
{
	unsigned char *pEnd1;
	unsigned char *pEnd2;

	pEnd1 = pBegin1 + OCTET_TO_SIZEOFUNIT(iSize1);
	pEnd2 = pBegin2 + OCTET_TO_SIZEOFUNIT(iSize2);

	/*
	 * 2 tests are required to determine the status of the 2 areas,
	 * in terms of their position wrt each other
	 */

	if (pBegin2 >= pBegin1) {
		/* check intrusion of pBegin2 in [pBegin1, pEnd1( */
		if (pBegin2 < pEnd1) {
			/* intrusion!! */
			return true;
		}

		/*
		 * pBegin2 lies outside and to the right of the first
		 * area, intrusion is impossible
		 */
		return false;
	}

	/* pBegin2 lies to the left of (pBegin1, pEnd1) */
	/* check end pointer: is pEnd2 in (pBegin1, pEnd1( ?? */
	if (pEnd2 > pBegin1) {
		/* intrusion!! */
		return true;
	}

	/*
	 * pEnd2 lies outside and to the left of the first area,
	 * intrusion is impossible
	 */
	return false;
}

static void pipe_intrusion_check(struct _k_pipe_desc *desc,
				 unsigned char *begin_ptr,
				 int size)
{
	/*
	 * check possible collision with all existing data areas,
	 * both for read and write areas
	 */

	int index;
	struct _k_pipe_marker_list *pMarkerList;

	/* write markers */

#ifdef STORE_NBR_MARKERS
	/* first a small consistency check */

	if (desc->write_markers.num_markers == 0) {
		__ASSERT_NO_MSG(desc->write_markers.first_marker == -1);
		__ASSERT_NO_MSG(desc->write_markers.last_marker == -1);
		__ASSERT_NO_MSG(desc->write_markers.post_wrap_around_marker == -1);
	}
#endif

	pMarkerList = &desc->write_markers;
	index = pMarkerList->first_marker;

	while (index != -1) {
		struct _k_pipe_marker *pM;

		pM = &(pMarkerList->markers[index]);

		if (AreasCheck4Intrusion(begin_ptr, size,
					      pM->pointer, pM->size) != 0) {
			__ASSERT_NO_MSG(1 == 0);
		}
		index = pM->next;
	}

	/* read markers */

#ifdef STORE_NBR_MARKERS
	/* first a small consistency check */

	if (desc->read_markers.num_markers == 0) {
		__ASSERT_NO_MSG(desc->read_markers.first_marker == -1);
		__ASSERT_NO_MSG(desc->read_markers.last_marker == -1);
		__ASSERT_NO_MSG(desc->read_markers.post_wrap_around_marker == -1);
	}
#endif

	pMarkerList = &desc->read_markers;
	index = pMarkerList->first_marker;

	while (index != -1) {
		struct _k_pipe_marker *pM;

		pM = &(pMarkerList->markers[index]);

		if (AreasCheck4Intrusion(begin_ptr, size,
					      pM->pointer, pM->size) != 0) {
			__ASSERT_NO_MSG(1 == 0);
		}
		index = pM->next;
	}
}
