    QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength,
                                       const UBaseType_t uxItemSize,
                                       const uint8_t ucQueueType )
    {
        Queue_t * pxNewQueue;
        size_t xQueueSizeInBytes;
        uint8_t * pucQueueStorage;

        configASSERT( uxQueueLength > ( UBaseType_t ) 0 );

        /* Allocate enough space to hold the maximum number of items that
         * can be in the queue at any time.  It is valid for uxItemSize to be
         * zero in the case the queue is used as a semaphore. */
        xQueueSizeInBytes = ( size_t ) ( uxQueueLength * uxItemSize ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

        /* Check for multiplication overflow. */
        configASSERT( ( uxItemSize == 0 ) || ( uxQueueLength == ( xQueueSizeInBytes / uxItemSize ) ) );

        /* Allocate the queue and storage area.  Justification for MISRA
         * deviation as follows:  pvPortMalloc() always ensures returned memory
         * blocks are aligned per the requirements of the MCU stack.  In this case
         * pvPortMalloc() must return a pointer that is guaranteed to meet the
         * alignment requirements of the Queue_t structure - which in this case
         * is an int8_t *.  Therefore, whenever the stack alignment requirements
         * are greater than or equal to the pointer to char requirements the cast
         * is safe.  In other cases alignment requirements are not strict (one or
         * two bytes). */
        pxNewQueue = ( Queue_t * ) pvPortMalloc( sizeof( Queue_t ) + xQueueSizeInBytes ); /*lint !e9087 !e9079 see comment above. */

        if( pxNewQueue != NULL )
        {
            /* Jump past the queue structure to find the location of the queue
             * storage area. */
            pucQueueStorage = ( uint8_t * ) pxNewQueue;
            pucQueueStorage += sizeof( Queue_t ); /*lint !e9016 Pointer arithmetic allowed on char types, especially when it assists conveying intent. */

            #if ( configSUPPORT_STATIC_ALLOCATION == 1 )
                {
                    /* Queues can be created either statically or dynamically, so
                     * note this task was created dynamically in case it is later
                     * deleted. */
                    pxNewQueue->ucStaticallyAllocated = pdFALSE;
                }
            #endif /* configSUPPORT_STATIC_ALLOCATION */

            prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
        }
        else
        {
            traceQUEUE_CREATE_FAILED( ucQueueType );
            mtCOVERAGE_TEST_MARKER();
        }

        return pxNewQueue;
    }