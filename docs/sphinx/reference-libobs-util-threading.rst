Threading
=========

Libobs provides a number of helper functions/types specifically for
threading.  The threading header will additionally provide access to
pthread functions even on windows.

.. code:: cpp

   #include <util/threading.h>


Threading Types
---------------

.. type:: os_event_t
.. type:: os_sem_t


General Thread Functions
------------------------

.. function:: void os_set_thread_name(const char *name)

   Sets the name of the current thread.

----------------------


Event Functions
---------------

.. function:: int  os_event_init(os_event_t **event, enum os_event_type type)

   Creates an event object.

   :param event: Pointer that receives a pointer to a new event object
   :param type:  Can be one of the following values:

                 - OS_EVENT_TYPE_AUTO - Automatically resets when
                   signaled
                 - OS_EVENT_TYPE_MANUAL - Stays signaled until the
                   :c:func:`os_event_reset()` function is called

   :return:      0 if successful, negative otherwise

----------------------

.. function:: void os_event_destroy(os_event_t *event)

   Destroys an event.

   :param event: An event object

----------------------

.. function:: int  os_event_wait(os_event_t *event)

   Waits for an event to signal.

   :param event: An event object
   :return:      0 if successful, negative otherwise

----------------------

.. function:: int  os_event_timedwait(os_event_t *event, unsigned long milliseconds)

   Waits a specific duration for an event to signal.

   :param event:        An event object
   :param milliseconds: Milliseconds to wait
   :return:             Can be one of the following values:
   
                        - 0 - successful
                        - ETIMEDOUT - Timed out
                        - EINVAL - An unexpected error occurred

----------------------

.. function:: int  os_event_try(os_event_t *event)

   Checks for a signaled state without waiting.

   :param event: An event object
   :return:             Can be one of the following values:
   
                        - 0 - successful
                        - EAGAIN - The event is not signaled
                        - EINVAL - An unexpected error occurred

----------------------

.. function:: int  os_event_signal(os_event_t *event)

   Signals the event.

   :param event: An event object
   :return:      0 if successful, negative otherwise

----------------------

.. function:: void os_event_reset(os_event_t *event)

   Resets the signaled state of the event.

   :param event: An event object

----------------------


Semaphore Functions
-------------------

.. function:: int  os_sem_init(os_sem_t **sem, int value)

   Creates a semaphore object.

   :param sem:   Pointer that receives a pointer to the semaphore object
   :param value: Initial value of the semaphore
   :return:      0 if successful, negative otherwise

----------------------

.. function:: void os_sem_destroy(os_sem_t *sem)

   Destroys a semaphore object.

   :param sem:   Semaphore object

----------------------

.. function:: int  os_sem_post(os_sem_t *sem)

   Increments the semaphore.

   :param sem:   Semaphore object
   :return:      0 if successful, negative otherwise

----------------------

.. function:: int  os_sem_wait(os_sem_t *sem)

   Decrements the semaphore or waits until the semaphore has been
   incremented.

   :param sem:   Semaphore object
   :return:      0 if successful, negative otherwise

---------------------


Atomic Inline Functions
-----------------------

.. function:: long os_atomic_inc_long(volatile long *val)

   Increments a long variable atomically.

---------------------

.. function:: long os_atomic_dec_long(volatile long *val)

   Decrements a long variable atomically.

---------------------

.. function:: void os_atomic_store_long(volatile long *ptr, long val)

   Stores the value of a long variable atomically.

---------------------

.. function:: long os_atomic_set_long(volatile long *ptr, long val)

   Exchanges the value of a long variable atomically. Badly named.

---------------------

.. function:: long os_atomic_exchange_long(volatile long *ptr, long val)

   Exchanges the value of a long variable atomically. Properly named.

---------------------

.. function:: long os_atomic_load_long(const volatile long *ptr)

   Gets the value of a long variable atomically.

---------------------

.. function:: bool os_atomic_compare_swap_long(volatile long *val, long old_val, long new_val)

   Swaps the value of a long variable atomically if its value matches.

---------------------

.. function:: void os_atomic_store_bool(volatile bool *ptr, bool val)

   Stores the value of a boolean variable atomically.

---------------------

.. function:: bool os_atomic_set_bool(volatile bool *ptr, bool val)

   Exchanges the value of a boolean variable atomically. Badly named.

---------------------

.. function:: bool os_atomic_exchange_bool(volatile bool *ptr, bool val)

   Exchanges the value of a boolean variable atomically. Properly named.

---------------------

.. function:: bool os_atomic_load_bool(const volatile bool *ptr)

   Gets the value of a boolean variable atomically.
