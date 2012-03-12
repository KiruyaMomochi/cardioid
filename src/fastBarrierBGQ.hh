/*------------------------------------------------------------------*/
/*                                                                  */
/* (C) Copyright IBM Corp.  2009, 2009                              */
/* Eclipse Public License (EPL)                                     */
/*                                                                  */
/*------------------------------------------------------------------*/
#ifndef FAST_BARRIER_BGQ_HH // Prevent multiple inclusion.
#define FAST_BARRIER_BGQ_HH

/*!
 * \file fastBarrierBGQ.hh
 *
 * \brief C Header file containing SPI L2-atomics-based barrier implementation
 * Alexandre Eichenberger
 *
 */

#include <hwi/include/common/compiler_support.h>

#include <sys/types.h>
#include <stdint.h>
#include <spi/include/l2/atomic.h>
#include <builtins.h>

#ifndef FAST_BARRIER_HH
#error "Do not #include fastBarrierBGQ.hh.  #include fastBarrier.hh instead"
#endif


/*!
 * \brief L2 Barrier
 *
 * The L2_Barrier_t structure.
 *
 * The structure contains two fields, start and count, which are assumed to
 * be initially zero.  Every thread increments count atomically when it
 * enters the barrier.  The "last" thread to enter bumps start up to the
 * current value of count, which releases the other threads waiting in the
 * barrier and initializes the barrier for the next round.
 *
 * No initialization or re-initialization is required between rounds.  The
 * fields grow monotonically but will not wrap for hundreds of years.
 *
 * The start and count fields occupy separate cache lines.  The count field
 * on which waiters spin is updated just once per round, so waiters will not
 * see unnecessary invalidates.
 *
 * This barrier implementation does not require any memory synchronization
 * operations, but memory synchronization operations were added to 
 * provide release consistency. Namely, all of the values generated by
 * all threads executing an Arrive call will be seen by all the threads
 * that executed a WaitAndReset. No such guarantees are done for the
 * threads that executed a Reset only.

 * Typical sequence
 *
 * Master (and Producer)   Producer...          Consumers...               Comments
 *
 * Barrier b;                                                              decl of barrier
 * Init(b);                                                                Init before parallel
 * #pragma omp parallel                                                    parallel section
 * {  
 *   Handle h;            Handle h;             Handle h;                  decl of handle 
 *   InitInThread(b, h);  InitInThread(b, h);   InitInThread(b, h);        init of handle
 * 
 *   // first round
 *   Arrive(b, h, n);     Arrive(b, h, n);                                 n producers Arrive
 *   Reset(b, h, n);      Reset(b, h, n);                                  n proucers Reset
 *                                              WaitAndReset(b, h, n);     m consumers WaitAndReset
 * 
 *   // second round 
 *   Arrive(b, h, n);     Arrive(b, h, n);                                 arrive, reset, and
 *   Reset(b, h, n);      Reset(b, h, n);                                  waitAndReset can
 *                                              WaitAndReset(b, h, n);     be called in any
 *                                                                         orders
 *   // barrier 
 *   Barrier(b, h, t)     Barrier(b, h, t)     Barrier(b, h, t)            barrier executed
 *                                                                         by all t threads
 *   // next round ...
 *   Arrive(b, h, n);     Arrive(b, h, n);                                 but all threads
 *   Reset(b, h, n);      Reset(b, h, n);                                  must Reset or
 *                                              WaitAndReset(b, h, n);     WaitAndReset 
 * 
 * }                                                                       no cleanup needed
 * 
 * // the whole sequence can be repeated (starting at Init(b))
 *
 * Sequences
 *
 * Master:   Init(b) parallel 
 * Producer: InitInThread (Arrive Reset | Barrier)*
 * Consumer: InitInThread (WaitAndReset | Barrier)*
 *
 * All Producers and Consumers must execute the barrier at the same time
 */

typedef struct {
  // new cache line
  volatile __attribute__((aligned(L1D_CACHE_LINE_SIZE)))
  uint64_t start;  /*!< Thread count at start of current round. */
  // new cache line
  volatile __attribute__((aligned(L1D_CACHE_LINE_SIZE)))
  uint64_t count;  /*!< Current thread count. */
  char pad[L1D_CACHE_LINE_SIZE - 8];
} L2_Barrier_t;


#define L2_BARRIER_INITIALIZER {0, 0, 0}

/*
 * The L2_BarrierHandle_t structure.
 * 
 * This structure keep thread private information about a given barrier.
 * There is thread location specific information, so the thread cannot 
 * migrate to a different thread on the same node without re-initializing
 * the handle.
 */

typedef struct {
  uint64_t localStart; // local (private start)
  volatile uint64_t *localCountPtr; // encoded fetch and inc address
} L2_BarrierHandle_t;

__BEGIN_DECLS

/*
 * L2_BarrierWithSync_Init: 
 *
 * must call exactly once to initialize the barrier before any thread
 * has a chance to execute an Arrive call. Typically, this would be
 * called one before the parallel region.
 * Each time that it is called, the user must also call
 * L2_BarrierWithSync_InitInThread in each of the threads
 * partitipating in the barrier.
 * 
 * input param is the global shared barrier data structure.
 */

__INLINE__ void L2_BarrierWithSync_Init(L2_Barrier_t *b)
{
  b->start = b->count = 0;
} 

/*
 * L2_BarrierWithSync_InitInThread: 
 *
 * must be done once per thread using the barrier, after having 
 * init the barrier. The handle must be private to the thread, i.e. 
 * each thread must have its own copy. 
 * Each thread can execute this InitInThread independently.
 *
 * input params are the global shared barrier and private barrier 
 * handle. The handle is then uniquely tied to that particuliar 
 * barrier.
 */
__INLINE__ void L2_BarrierWithSync_InitInThread(
  L2_Barrier_t *b,       /* global barrier */
  L2_BarrierHandle_t *h) /* barrier handle private to this thread */
{
  h->localStart = 0;
  const uint32_t tid = Kernel_ProcessorThreadID();
  h->localCountPtr = __l2_op_tid_ptr(&b->count, 
    L2_ATOMIC_OPCODE_LOAD_INCREMENT, tid);
}

/* 
 * L2_BarrierWithSync_Arrive: 
 *
 * Announce arrival at the barrier. Only producers need to do this. 
 * A single thread may arrive once or multiple times, makes no difference.
 *
 * imput param are the global shared barrier, private barrier 
 * handle, and the number of arrival event expected at the barrier.
 */
__INLINE__ void L2_BarrierWithSync_Arrive(
  L2_Barrier_t *b,        /* global barrier */
  L2_BarrierHandle_t *h,  /* barrier handle private to this thread */
  int eventNum)           /* number of arrival events */
{
  // memory sync
  __lwsync();
  // fetch and increment
  uint64_t count =  *(h->localCountPtr);
  uint64_t current = count + 1;
  // if reached target, update barrier's start
  uint64_t target = h->localStart + eventNum;
  if (current == target) {
      b->start = current;  // advance to next round
  }
}

/*
 *  L2_BarrierWithSync_WaitAndReset: 
 *
 * Wait until all threads have arrived (i.e. eventNum calls to 
 * L2_BarrierWithSync_Arrive). Once all have arrived, it reset the 
 * barrier/handle for the next barrier. All of the values generated
 * by the threads doing the arriving calls are guaranteed to be 
 * observed by the thread doing the wait and reset.
 *
 * imput param are the global shared barrier, private barrier 
 * handle, and the number of arrival event expected at the barrier.
 */

__INLINE__ void L2_BarrierWithSync_WaitAndReset(
  L2_Barrier_t *b,       /* global barrier */
  L2_BarrierHandle_t *h, /* barrier handle private to this thread */
  int eventNum)          /* number of arrival events */
{
  // compute target from local start
  uint64_t target = h->localStart + eventNum;
  // advance local start
  h->localStart = target;
  // wait until barrier's start is advanced
  while (b->start < target) ;
  // prevent speculation
  __isync();
}


/*
 * L2_BarrierWithSyncReset: 
 *
 * Simply reset the barrier/handle for the next barrier without waiting.
 * Values generated by the threads executing arrive are not guaranteed
 * to be observed by this thread.
 *
 * imput param are the global shared barrier, private barrier 
 * handle, and the number of arrival event expected at the barrier.
 */

__INLINE__ void L2_BarrierWithSync_Reset(
  L2_Barrier_t *b,       /* global barrier */
  L2_BarrierHandle_t *h, /* barrier handle private to this thread */
  int eventNum)          /* number of arrival events */
{
  // compute target from local start
  uint64_t target = h->localStart + eventNum;
  // advance local start
  h->localStart = target;
}

/*
 * L2_BarrierWithSync_Barrier
 *
 * Perform a traditional barrier
 * imput param are the global shared barrier, private barrier 
 * handle, and the number of arrival event expected at the barrier.
 */
__INLINE__ void L2_BarrierWithSync_Barrier(
  L2_Barrier_t *b,       /* global barrier */
  L2_BarrierHandle_t *h, /* barrier handle private to this thread */
  int eventNum)          /* number of arrival events */
{
  L2_BarrierWithSync_Arrive(b, h, eventNum);
  L2_BarrierWithSync_WaitAndReset(b, h, eventNum);
}

/** Call this before the parallel region.  Replaces call to Init.
 * Caller is responsible to free the returned pointer. */
L2_Barrier_t* L2_BarrierWithSync_InitShared()
{
   L2_Barrier_t* bb = (L2_Barrier_t*) malloc(sizeof(L2_Barrier_t));
   Kernel_L2AtomicsAllocate(bb, sizeof(L2_Barrier_t));
   L2_BarrierWithSync_Init(bb);
   return bb;
}

__END_DECLS

#endif // Add nothing below this line.
