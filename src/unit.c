/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/** @defgroup UNIT  Work Unit
 * This group is for work units.
 */

/**
 * @ingroup UNIT
 * @brief   Set the associated pool for a work unit.
 *
 * \c ABT_unit_set_associated_pool() changes the associated pool of the target
 * work unit \c unit to the pool \c pool.  This routine must be called
 * after \c unit is popped from its original associated pool (i.e., \c unit must
 * not be inside any pool).
 *
 * @contexts
 * \DOC_CONTEXT_INIT \DOC_CONTEXT_NOCTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_ERROR_INV_UNIT_HANDLE{\c unit}
 * \DOC_ERROR_INV_POOL_HANDLE{\c pool}
 *
 * @undefined
 * \DOC_UNDEFINED_UNINIT
 * \DOC_UNDEFINED_WORK_UNIT_IN_POOL{\c unit}
 * \DOC_UNDEFINED_THREAD_UNSAFE{\c unit}
 *
 * @param[in] unit  work unit handle
 * @param[in] pool  pool handle
 * @return Error code
 */
int ABT_unit_set_associated_pool(ABT_unit unit, ABT_pool pool)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);
    ABTI_CHECK_TRUE(unit != ABT_UNIT_NULL, ABT_ERR_INV_UNIT);

    ABTI_unit_set_associated_pool(unit, p_pool);
    return ABT_SUCCESS;
}

/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

void ABTI_unit_set_associated_pool(ABT_unit unit, ABTI_pool *p_pool)
{
    ABT_thread thread = p_pool->u_get_thread(unit);
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    p_thread->p_pool = p_pool;
}
