/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mem_protect.h"

/* Kernel objects */
K_THREAD_STACK_DEFINE(kobject_stack_1, KOBJECT_STACK_SIZE);
K_THREAD_STACK_DEFINE(kobject_stack_2, KOBJECT_STACK_SIZE);

K_SEM_DEFINE(kobject_sem, SEMAPHORE_INIT_COUNT, SEMAPHORE_MAX_COUNT);
K_SEM_DEFINE(kobject_public_sem, SEMAPHORE_INIT_COUNT, SEMAPHORE_MAX_COUNT);
K_MUTEX_DEFINE(kobject_mutex);
__kernel struct k_thread kobject_test_4_tid;
__kernel struct k_thread kobject_test_6_tid;
__kernel struct k_thread kobject_test_7_tid;

__kernel struct k_thread kobject_test_9_tid;
__kernel struct k_thread kobject_test_13_tid;
__kernel struct k_thread kobject_test_14_tid;

__kernel struct k_thread kobject_test_reuse_1_tid, kobject_test_reuse_2_tid;
__kernel struct k_thread kobject_test_reuse_3_tid, kobject_test_reuse_4_tid;
__kernel struct k_thread kobject_test_reuse_5_tid, kobject_test_reuse_6_tid;

struct k_thread kobject_test_10_tid_uninitialized;

struct k_sem *random_sem_type;
struct k_sem kobject_sem_not_hash_table;
__kernel struct k_sem kobject_sem_no_init_no_access;
__kernel struct k_sem kobject_sem_no_init_access;


/****************************************************************************/
void kobject_user_tc1(void *p1, void *p2, void *p3)
{
	valid_fault = true;
	k_sem_take(random_sem_type, K_FOREVER);
}

/* Test access to a invalid semaphore who's address is NULL. */
void kobject_test_1(void *p1, void *p2, void *p3)
{

	_k_object_init(random_sem_type);
	k_thread_access_grant(k_current_get(),
			      &kobject_sem,
			      &kobject_mutex,
			      random_sem_type, NULL);

	k_thread_user_mode_enter(kobject_user_tc1, NULL, NULL, NULL);

}
/****************************************************************************/
void kobject_user_tc2(void *p1, void *p2, void *p3)
{
	valid_fault = false;
	k_sem_give(&kobject_sem);

	/* should cause a fault */
	valid_fault = true;
	/* typecasting to override compiler warning */
	k_sem_take((struct k_sem *)&kobject_mutex, K_FOREVER);


}

/* Test if a syscall can take a different type of kobject. */
void kobject_test_2(void *p1, void *p2, void *p3)
{
	k_thread_access_grant(k_current_get(),
			      &kobject_sem,
			      &kobject_mutex, NULL);

	k_thread_user_mode_enter(kobject_user_tc2, NULL, NULL, NULL);

}

/****************************************************************************/
void kobject_user_tc3(void *p1, void *p2, void *p3)
{
	/* should cause a fault */
	valid_fault = true;
	k_sem_give(&kobject_sem);
}

/* test if a user thread can access a k object that has not be granted to it.*/
void kobject_test_3(void *p1, void *p2, void *p3)
{
	k_thread_access_grant(k_current_get(),
			      &kobject_mutex, NULL);

	k_thread_user_mode_enter(kobject_user_tc3, NULL, NULL, NULL);

}

/****************************************************************************/
void kobject_user_test4(void *p1, void *p2, void *p3)
{
	/* should cause a fault */
	if ((u32_t)p1 == 1) {
		valid_fault = false;
	} else {
		valid_fault = true;
	}
	k_sem_give(&kobject_sem);
}

/* Test access revoke. */
void kobject_test_4(void *p1, void *p2, void *p3)
{
	k_thread_access_grant(k_current_get(),
			      &kobject_sem, NULL);

	k_thread_create(&kobject_test_4_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_user_test4,
			(void *)1, NULL, NULL,
			0, K_INHERIT_PERMS|K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);
	k_object_access_revoke(&kobject_sem, k_current_get());

	k_thread_create(&kobject_test_4_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_user_test4,
			(void *)2, NULL, NULL,
			0, K_INHERIT_PERMS|K_USER, K_NO_WAIT);

	k_thread_abort(k_current_get());

}

/****************************************************************************/
/* grant access to all user threads that follow */
void kobject_user_1_test5(void *p1, void *p2, void *p3)
{
	valid_fault = false;
	k_sem_give(&kobject_sem);
	k_object_access_grant(&kobject_sem, &kobject_test_reuse_2_tid);
}

void kobject_user_2_test5(void *p1, void *p2, void *p3)
{
	valid_fault = false;
	k_sem_take(&kobject_sem, K_FOREVER);
	ztest_test_pass();
}

/* Test grant access. Will grant access to another thread for the
 * semaphore it holds.
 */
void kobject_test_5(void *p1, void *p2, void *p3)
{
	k_thread_access_grant(k_current_get(),
			      &kobject_sem, &kobject_test_reuse_2_tid, NULL);

	k_thread_create(&kobject_test_reuse_1_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_user_1_test5,
			NULL, NULL, NULL,
			0, K_INHERIT_PERMS | K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

	k_thread_create(&kobject_test_reuse_2_tid,
			kobject_stack_2,
			KOBJECT_STACK_SIZE,
			kobject_user_2_test5,
			NULL, NULL, NULL,
			0, K_INHERIT_PERMS|K_USER, K_NO_WAIT);

	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}

/****************************************************************************/
void kobject_user_test6(void *p1, void *p2, void *p3)
{
	valid_fault = false;
	k_sem_give(&kobject_sem);
	valid_fault = true;
	k_object_access_grant(&kobject_sem, &kobject_test_reuse_2_tid);
}

/* Test access grant to thread B from thread A which doesn't have
 * required permissions.
 */
void kobject_test_6(void *p1, void *p2, void *p3)
{
	k_thread_access_grant(k_current_get(),
			      &kobject_sem, NULL);

	k_thread_create(&kobject_test_6_tid,
			kobject_stack_2,
			KOBJECT_STACK_SIZE,
			kobject_user_test6,
			NULL, NULL, NULL,
			0, K_INHERIT_PERMS|K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}

/****************************************************************************/
void kobject_user_test7(void *p1, void *p2, void *p3)
{
	valid_fault = false;
	k_sem_give(&kobject_sem);
	k_object_access_revoke(&kobject_sem, k_current_get());
	valid_fault = true;
	k_sem_give(&kobject_sem);
}

/* Test revoke permission of a k object from userspace. */
void kobject_test_7(void *p1, void *p2, void *p3)
{
	k_thread_access_grant(k_current_get(),
			      &kobject_sem, NULL);

	k_thread_create(&kobject_test_7_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_user_test7,
			NULL, NULL, NULL,
			0, K_INHERIT_PERMS|K_USER, K_NO_WAIT);

	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}
/****************************************************************************/
void kobject_user_1_test8(void *p1, void *p2, void *p3)
{
	valid_fault = false;
	k_sem_give(&kobject_public_sem);

}

void kobject_user_2_test8(void *p1, void *p2, void *p3)
{
	valid_fault = false;
	k_sem_take(&kobject_public_sem, K_FOREVER);
	ztest_test_pass();
}

/* Test all access grant. test the access by creating 2 new user threads.
 */
void kobject_test_8(void *p1, void *p2, void *p3)
{

	k_object_access_all_grant(&kobject_public_sem);
	k_thread_create(&kobject_test_reuse_1_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_user_1_test8,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

	k_thread_create(&kobject_test_reuse_2_tid,
			kobject_stack_2,
			KOBJECT_STACK_SIZE,
			kobject_user_2_test8,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);

	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}
/****************************************************************************/

void kobject_user_1_test9(void *p1, void *p2, void *p3)
{
	valid_fault = false;
	k_sem_give(&kobject_sem);
	k_thread_abort(k_current_get());

}

void kobject_user_2_test9(void *p1, void *p2, void *p3)
{

	valid_fault = true;
	k_sem_take(&kobject_sem, K_FOREVER);
	zassert_unreachable("Failed to clear premission on a deleted thread");
}

/* if a deleted thread with some permissions is recreated with the same tid
 * Check if it still has the permissions.
 */
void kobject_test_9(void *p1, void *p2, void *p3)
{

	k_thread_access_grant(k_current_get(),
			      &kobject_sem, NULL);

	k_thread_create(&kobject_test_9_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_user_1_test9,
			NULL, NULL, NULL,
			0, K_INHERIT_PERMS | K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

	k_thread_create(&kobject_test_9_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_user_2_test9,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);
}

/****************************************************************************/
/*grant access to a valid kobject but invalid thread id */
#define ERROR_STR_TEST_10 "Access granted/revoked to invalid thread k_object"
void kobject_test_10(void *p1, void *p2, void *p3)
{

	k_object_access_grant(&kobject_sem, &kobject_test_10_tid_uninitialized);
	k_object_access_revoke(&kobject_sem,
			       &kobject_test_10_tid_uninitialized);

	/* Test if this has actually taken the required branch */
	extern void *_k_object_find(void *object);
	void *ret_value = _k_object_find(&kobject_test_10_tid_uninitialized);

	if (ret_value == NULL) {
		ztest_test_pass();
	} else {
		zassert_unreachable(ERROR_STR_TEST_10);
	}
}
/****************************************************************************/
/* object validation checks */
/* Test syscall on a kobject which is not present in the hash table. */
void kobject_test_11(void *p1, void *p2, void *p3)
{
	valid_fault = true;
	k_sem_take(&kobject_sem_not_hash_table, K_SECONDS(1));
	zassert_unreachable("k_object validation  failure.");

}

/****************************************************************************/
/* object validation checks */
/* Test syscall on a kobject which is not initialized and has no access */
void kobject_test_12(void *p1, void *p2, void *p3)
{
	valid_fault = true;
	k_sem_take(&kobject_sem_no_init_no_access, K_SECONDS(1));
	zassert_unreachable("k_object validation  failure");

}
/****************************************************************************/
/* object validation checks */
void kobject_test_user_13(void *p1, void *p2, void *p3)
{
	valid_fault = true;

	k_sem_take(&kobject_sem_no_init_access, K_SECONDS(1));
	zassert_unreachable("_SYSCALL_OBJ implementation failure.");
}

/* Test syscall on a kobject which is not initialized and has access */
void kobject_test_13(void *p1, void *p2, void *p3)
{
	k_thread_access_grant(k_current_get(),
			      &kobject_sem_no_init_access, NULL);

	k_thread_create(&kobject_test_13_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_13,
			NULL, NULL, NULL,
			0, K_INHERIT_PERMS | K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}

/****************************************************************************/
/* object validation checks */
void kobject_test_user_2_14(void *p1, void *p2, void *p3)
{
	zassert_unreachable("_SYSCALL_OBJ implementation failure.");
}

void kobject_test_user_1_14(void *p1, void *p2, void *p3)
{
	valid_fault = true;

	k_thread_create(&kobject_test_14_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_2_14,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);

	zassert_unreachable("_SYSCALL_OBJ implementation failure.");

}
/* Try to reintialize the k_thread object. Object validation check */
void kobject_test_14(void *p1, void *p2, void *p3)
{
	k_thread_create(&kobject_test_14_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_1_14,
			NULL, NULL, NULL,
			0, K_INHERIT_PERMS | K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);
}

/****************************************************************************/
/* object validation checks */
void kobject_test_user_2_15(void *p1, void *p2, void *p3)
{
	ztest_test_pass();
}

void kobject_test_user_1_15(void *p1, void *p2, void *p3)
{
	valid_fault = false;

	k_thread_create(&kobject_test_reuse_4_tid,
			kobject_stack_2,
			KOBJECT_STACK_SIZE,
			kobject_test_user_2_15,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);

	k_thread_abort(k_current_get());

}
/* Try to reintialize the k_thread object. Object validation check */
void kobject_test_15(void *p1, void *p2, void *p3)
{

	k_thread_access_grant(&kobject_test_reuse_3_tid,
			      &kobject_test_reuse_4_tid,
			      &kobject_stack_2, NULL);

	k_thread_create(&kobject_test_reuse_3_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_1_15,
			NULL, NULL, NULL,
			0, K_INHERIT_PERMS | K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}

/****************************************************************************/
/* object validation checks */
void kobject_test_user_2_16(void *p1, void *p2, void *p3)
{
	zassert_unreachable("k_object validation failure in k thread create");
}

void kobject_test_user_1_16(void *p1, void *p2, void *p3)
{
	valid_fault = true;

	k_thread_create(&kobject_test_reuse_6_tid,
			kobject_stack_2,
			KOBJECT_STACK_SIZE,
			kobject_test_user_2_16,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);

	k_thread_abort(k_current_get());

}
/* Create a new thread from user and the user doesn't have access
 * to the stack region of new thread.
 * _handler_k_thread_create validation.
 */
void kobject_test_16(void *p1, void *p2, void *p3)
{

	k_thread_access_grant(&kobject_test_reuse_5_tid,
			      &kobject_test_reuse_6_tid, NULL);

	k_thread_create(&kobject_test_reuse_5_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_1_16,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);
}

/****************************************************************************/
/* object validation checks */
void kobject_test_user_2_17(void *p1, void *p2, void *p3)
{
	zassert_unreachable("k_object validation failure in k thread create");
}

void kobject_test_user_1_17(void *p1, void *p2, void *p3)
{

	valid_fault = true;

	k_thread_create(&kobject_test_reuse_2_tid,
			kobject_stack_2,
			-1,
			kobject_test_user_2_17,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);

	zassert_unreachable("k_object validation failure in k thread create");
}
/* Create a new thread from user and use a huge stack size which overflows
 * _handler_k_thread_create validation.
 */
void kobject_test_17(void *p1, void *p2, void *p3)
{

	k_thread_access_grant(&kobject_test_reuse_1_tid,
			      &kobject_test_reuse_2_tid,
			      &kobject_stack_2, NULL);

	k_thread_create(&kobject_test_reuse_1_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_1_17,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}

/****************************************************************************/
/* object validation checks */
void kobject_test_user_2_18(void *p1, void *p2, void *p3)
{
	zassert_unreachable("k_object validation failure in k thread create");
}

void kobject_test_user_1_18(void *p1, void *p2, void *p3)
{

	valid_fault = true;

	k_thread_create(&kobject_test_reuse_4_tid,
			kobject_stack_2,
			(KOBJECT_STACK_SIZE * 5),
			kobject_test_user_2_18,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);

	zassert_unreachable("k_object validation failure in k thread create");
}
/* Create a new thread from user and use a stack bigger than allowed size.
 * _handler_k_thread_create validation.
 */
void kobject_test_18(void *p1, void *p2, void *p3)
{

	k_thread_access_grant(&kobject_test_reuse_3_tid,
			      &kobject_test_reuse_4_tid,
			      &kobject_stack_2, NULL);

	k_thread_create(&kobject_test_reuse_3_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_1_18,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}

/****************************************************************************/
/* object validation checks */

void kobject_test_user_2_19(void *p1, void *p2, void *p3)
{
	zassert_unreachable("k_object validation failure in k thread create");
}

void kobject_test_user_1_19(void *p1, void *p2, void *p3)
{

	valid_fault = true;

	k_thread_create(&kobject_test_reuse_6_tid,
			kobject_stack_2,
			KOBJECT_STACK_SIZE,
			kobject_test_user_2_19,
			NULL, NULL, NULL,
			0, 0, K_NO_WAIT);

	zassert_unreachable("k_object validation failure in k thread create");
}
/* Create a new supervisor thread from user.
 * _handler_k_thread_create validation.
 */

void kobject_test_19(void *p1, void *p2, void *p3)
{
	k_thread_access_grant(&kobject_test_reuse_5_tid,
			      &kobject_test_reuse_6_tid,
			      &kobject_stack_2, NULL);

	k_thread_create(&kobject_test_reuse_5_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_1_19,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}

/****************************************************************************/
/* object validation checks */

void kobject_test_user_2_20(void *p1, void *p2, void *p3)
{
	zassert_unreachable("k_object validation failure in k thread create");
}

void kobject_test_user_1_20(void *p1, void *p2, void *p3)
{

	valid_fault = true;

	k_thread_create(&kobject_test_reuse_2_tid,
			kobject_stack_2,
			KOBJECT_STACK_SIZE,
			kobject_test_user_2_20,
			NULL, NULL, NULL,
			0, K_USER | K_ESSENTIAL, K_NO_WAIT);

	zassert_unreachable("k_object validation failure in k thread create");
}
/* Create a new essential thread from user.
 * _handler_k_thread_create validation.
 */

void kobject_test_20(void *p1, void *p2, void *p3)
{
	k_thread_access_grant(&kobject_test_reuse_1_tid,
			      &kobject_test_reuse_2_tid,
			      &kobject_stack_2, NULL);

	k_thread_create(&kobject_test_reuse_1_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_1_20,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}

/****************************************************************************/
/* object validation checks */

void kobject_test_user_2_21(void *p1, void *p2, void *p3)
{
	zassert_unreachable("k_object validation failure in k thread create");
}

void kobject_test_user_1_21(void *p1, void *p2, void *p3)
{

	valid_fault = true;

	k_thread_create(&kobject_test_reuse_4_tid,
			kobject_stack_2,
			KOBJECT_STACK_SIZE,
			kobject_test_user_2_21,
			NULL, NULL, NULL,
			-1, K_USER, K_NO_WAIT);

	zassert_unreachable("k_object validation failure in k thread create");
}
/* Create a new thread whose prority is higher than the current thread.
 * _handler_k_thread_create validation.
 */

void kobject_test_21(void *p1, void *p2, void *p3)
{
	k_thread_access_grant(&kobject_test_reuse_3_tid,
			      &kobject_test_reuse_4_tid,
			      &kobject_stack_2, NULL);

	k_thread_create(&kobject_test_reuse_3_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_1_21,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);


	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}

/****************************************************************************/
/* object validation checks */

void kobject_test_user_2_22(void *p1, void *p2, void *p3)
{
	zassert_unreachable("k_object validation failure in k thread create");
}

void kobject_test_user_1_22(void *p1, void *p2, void *p3)
{

	valid_fault = true;

	k_thread_create(&kobject_test_reuse_6_tid,
			kobject_stack_2,
			KOBJECT_STACK_SIZE,
			kobject_test_user_2_22,
			NULL, NULL, NULL,
			6000, K_USER, K_NO_WAIT);

	zassert_unreachable("k_object validation failure in k thread create");
}
/* Create a new thread whose prority is invalid.
 * _handler_k_thread_create validation.
 */

void kobject_test_22(void *p1, void *p2, void *p3)
{
	k_thread_access_grant(&kobject_test_reuse_5_tid,
			      &kobject_test_reuse_6_tid,
			      &kobject_stack_2, NULL);


	k_thread_create(&kobject_test_reuse_5_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_1_22,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);

	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}

/****************************************************************************/
/*
 * Check if the permissions are given to a particular thread for a k object.
 */

void kobject_test_23(void *p1, void *p2, void *p3)
{
	bool return_value = true;
	k_thread_access_grant(&kobject_test_reuse_1_tid,
			      &kobject_test_reuse_2_tid,
			      &kobject_stack_2, NULL);

	return_value |= \
		k_object_access_check(&kobject_test_reuse_2_tid,
				      &kobject_test_reuse_1_tid) == 0;

	return_value |= \
		k_object_access_check(&kobject_test_reuse_1_tid,
				      &kobject_test_reuse_2_tid) == -EPERM;

	return_value |= \
		k_object_access_check(&kobject_public_sem,
				      &kobject_test_reuse_1_tid) == 0;

	return_value |=	\
		k_object_access_check(&kobject_sem_not_hash_table,
				      &kobject_test_reuse_1_tid) == -EPERM;


	if (return_value == true) {
		ztest_test_pass();
	}
	else {
		ztest_test_fail();
	}

}

/****************************************************************************/
void kobject_test_user_1_24(void *p1, void *p2, void *p3)
{
	bool return_value = true;

	valid_fault = false;

	return_value |=	\
		k_object_access_check(&kobject_test_reuse_2_tid,
				      &kobject_test_reuse_1_tid) == 0;

	if (return_value == false) {
		zassert_unreachable("permission check failed");
	}

	return_value |= \
		k_object_access_check(&kobject_test_reuse_1_tid,
				      &kobject_test_reuse_2_tid) == -EPERM;

	if (return_value == false) {
		zassert_unreachable("Thread has more permissions.");
	}

	return_value |= \
		k_object_access_check(&kobject_public_sem,
				      &kobject_test_reuse_1_tid) == 0;

	if (return_value == false) {
		zassert_unreachable("permissions on public k object failed.");
	}

	ztest_test_pass();

}
/* Check permissions from user thread.
 */

void kobject_test_24(void *p1, void *p2, void *p3)
{
	k_thread_access_grant(&kobject_test_reuse_1_tid,
			      &kobject_test_reuse_2_tid,
			      &kobject_stack_2, NULL);


	k_thread_create(&kobject_test_reuse_1_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_1_24,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);

	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}

/****************************************************************************/
void kobject_test_user_1_25(void *p1, void *p2, void *p3)
{
	bool return_value = true;

	valid_fault = true;

	return_value |=	\
		k_object_access_check(&kobject_test_reuse_3_tid,
				      &kobject_test_reuse_4_tid) == 0;

	zassert_unreachable("permission check failed");


}
/* Check permissions on unused k_thread
 */

void kobject_test_25(void *p1, void *p2, void *p3)
{

	k_thread_create(&kobject_test_reuse_3_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_1_25,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);

	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}

/****************************************************************************/
void kobject_test_user_1_26(void *p1, void *p2, void *p3)
{
	bool return_value = true;

	valid_fault = false;

	return_value |=	\
		k_object_access_check(&kobject_public_sem,
				      &kobject_test_reuse_5_tid) == 0;

	if (return_value == false) {
		zassert_unreachable("permission check failed");
	}

	valid_fault = true;

	return_value |=	\
		k_object_access_check(&kobject_public_sem,
				      &kobject_test_reuse_6_tid) == -EPERM;

	zassert_unreachable("accessed another thread's struct. ");

}
/* Check permissions on k objects for which access is not granted.
 */

void kobject_test_26(void *p1, void *p2, void *p3)
{

	k_thread_create(&kobject_test_reuse_5_tid,
			kobject_stack_1,
			KOBJECT_STACK_SIZE,
			kobject_test_user_1_26,
			NULL, NULL, NULL,
			0, K_USER, K_NO_WAIT);

	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);

}
