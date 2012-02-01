= mutex.cppにあるやつの翻訳

// share/vm/runtime/mutex.cpp
// o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o
//
// Native Monitor-Mutex locking - theory of operations
//
// * Native Monitors are completely unrelated to Java-level monitors,
//   although the "back-end" slow-path implementations share a common lineage.

であるが 「バックエンド」の遅い処理は同じ系統を共有する。

//   See objectMonitor:: in synchronizer.cpp.
//   Native Monitors do *not* support nesting or recursion but otherwise
//   they're basically Hoare-flavor monitors.

ネイティブモニターはネストと再帰をサポートしないが、その他の点では
基本的にホーアっぽいモニターである。

* A thread acquires ownership of a Monitor/Mutex by CASing the LockByte
  in the _LockWord from zero to non-zero. 

スレッドはMonitor/Mutexの所有権を_LockWordのLockByteの0から0以外へのCASによって獲得する。

Note that the _Owner field is advisory and is used only to verify that
the thread calling unlock() is indeed the last thread to have acquired
the lock.

注意: _Ownerフィールドはadvisoryで、スレッドのunlock()呼び出しは本当に
最後のスレッドがロックを獲得したことを検証するためにのみ使われる。

* Contending threads "push" themselves onto the front of the contention
 queue -- called the cxq -- with CAS and then spin/park.

競走中スレッドは彼ら自身をcontention queue -- cxq と呼ぶ -- に追加する。CASとその後のspin/parkによって。

The _LockWord contains the LockByte as well as the pointer to the head of the cxq. 

_LockWord は LockByte と cxq の先頭へのポインタを持つ。

Colocating the LockByte with the cxq precludes certain races.
LockByteとcxqを共同の場所はいくらかのレースを防ぐ。

* Using a separately addressable LockByte allows for CAS:MEMBAR or
CAS:0 idioms.

We currently use MEMBAR in the uncontended unlock() path, as MEMBAR
often has less latency than CAS.
MEMBARは現在競合者なしのunlock()パスの中で使っている。
MEMBARはしばしばCASよりも待ち時間よりが短い。

If warranted, we could switch to a CAS:0 mode, using timers to close
the resultant race, as is done with Java Monitors in synchronizer.cpp.
もし保証できれば、CAS:0モードにスイッチできるだろう、タイマーと競合結果を閉じることによって。

See the following for a discussion of the relative cost of atomics
(CAS) MEMBAR, and ways to eliminate such instructions from the
common-case paths:
//   -- http://blogs.sun.com/dave/entry/biased_locking_in_hotspot
//   -- http://blogs.sun.com/dave/resource/MustangSync.pdf
//   -- http://blogs.sun.com/dave/resource/synchronization-public2.pdf
//   -- synchronizer.cpp


* Overall goals 全体的なゴール - desiderata 欲しい物たち
  1. Minimize context switching 小さいコンテキストスイッチング
  2. Minimize lock migration 小さいロックの移行
  3. Minimize CPI(Cycles Per Instruction) -- affinity and locality
  4. Minimize the execution of high-latency instructions such as CAS or MEMBAR
  5. Minimize outer lock hold times
  6. Behave gracefully on a loaded system


* Thread flow and list residency:

  Contention queue --> EntryList --> OnDeck --> Owner --> !Owner
  [..resident on monitor list..]
  [...........contending..................]

  -- The contention queue (cxq) contains recently-arrived threads (RATs).
     Threads on the cxq eventually drain into the EntryList.
  -- Invariant: a thread appears on at most one list -- cxq, EntryList
     or WaitSet -- at any one time.
cxqとEntryListもしくはWaitSetの最大でも一つのリストに現れる。

  -- For a given monitor there can be at most one "OnDeck" thread at any
     given time but if need be this particular invariant could be relaxed.

* The WaitSet and EntryList linked lists are composed of ParkEvents.
  I use ParkEvent instead of threads as ParkEvents are immortal and
  type-stable, meaning we can safely unpark() a possibly stale
  list element in the unlock()-path.  (That's benign).

* Succession policy - providing for progress:
  継続ポリシー - 

As necessary, the unlock()ing thread identifies, unlinks, and unparks
an "heir presumptive" tentative successor thread from the EntryList.

This becomes the so-called "OnDeck" thread, of which there can be only
one at any given time for a given monitor. 

The wakee will recontend for ownership of monitor.

Succession is provided for by a policy of competitive handoff.

The exiting thread does _not_ grant or pass ownership to the
successor thread. 
(This is also referred to as "handoff" succession").

Instead the exiting thread releases ownership and possibly wakes a
successor, so the successor can (re)compete for ownership of the lock.


Competitive handoff provides excellent overall throughput at the
expense of short-term fairness. 

If fairness is a concern then one remedy might be to add an
AcquireCounter field to the monitor. 

After a thread acquires the lock it will decrement the AcquireCounter
field. 


When the count reaches 0 the thread would reset the AcquireCounter
variable, abdicate the lock directly to some thread on the EntryList,
and then move itself to the tail of the EntryList.


But in practice most threads engage or otherwise participate in
resource bounded producer-consumer relationships, so lock domination
is not usually a practical concern. 
Recall too, that in general it's easier to construct a fair lock from
a fast lock, but not vice-versa.

* The cxq can have multiple concurrent "pushers" but only one concurrent
  detaching thread.  This mechanism is immune from the ABA corruption.
  More precisely, the CAS-based "push" onto cxq is ABA-oblivious.
  We use OnDeck as a pseudo-lock to enforce the at-most-one detaching
  thread constraint.

* Taken together, the cxq and the EntryList constitute or form a
  single logical queue of threads stalled trying to acquire the lock.
  We use two distinct lists to reduce heat on the list ends.
  Threads in lock() enqueue onto cxq while threads in unlock() will
  dequeue from the EntryList.  (c.f. Michael Scott's "2Q" algorithm).
  A key desideratum is to minimize queue & monitor metadata manipulation
  that occurs while holding the "outer" monitor lock -- that is, we want to
  minimize monitor lock holds times.

  The EntryList is ordered by the prevailing queue discipline and
  can be organized in any convenient fashion, such as a doubly-linked list or
  a circular doubly-linked list.  If we need a priority queue then something akin
  to Solaris' sleepq would work nicely.  Viz.,
  -- http://agg.eng/ws/on10_nightly/source/usr/src/uts/common/os/sleepq.c.
  -- http://cvs.opensolaris.org/source/xref/onnv/onnv-gate/usr/src/uts/common/os/sleepq.c
  Queue discipline is enforced at ::unlock() time, when the unlocking thread
  drains the cxq into the EntryList, and orders or reorders the threads on the
  EntryList accordingly.

  Barring "lock barging", this mechanism provides fair cyclic ordering,
  somewhat similar to an elevator-scan.

* OnDeck
  --  For a given monitor there can be at most one OnDeck thread at any given
      instant.  The OnDeck thread is contending for the lock, but has been
      unlinked from the EntryList and cxq by some previous unlock() operations.
      Once a thread has been designated the OnDeck thread it will remain so
      until it manages to acquire the lock -- being OnDeck is a stable property.
  --  Threads on the EntryList or cxq are _not allowed to attempt lock acquisition.
  --  OnDeck also serves as an "inner lock" as follows.  Threads in unlock() will, after
      having cleared the LockByte and dropped the outer lock,  attempt to "trylock"
      OnDeck by CASing the field from null to non-null.  If successful, that thread
      is then responsible for progress and succession and can use CAS to detach and
      drain the cxq into the EntryList.  By convention, only this thread, the holder of
      the OnDeck inner lock, can manipulate the EntryList or detach and drain the
      RATs on the cxq into the EntryList.  This avoids ABA corruption on the cxq as
      we allow multiple concurrent "push" operations but restrict detach concurrency
      to at most one thread.  Having selected and detached a successor, the thread then
      changes the OnDeck to refer to that successor, and then unparks the successor.
      That successor will eventually acquire the lock and clear OnDeck.  Beware
      that the OnDeck usage as a lock is asymmetric.  A thread in unlock() transiently
      "acquires" OnDeck, performs queue manipulations, passes OnDeck to some successor,
      and then the successor eventually "drops" OnDeck.  Note that there's never
      any sense of contention on the inner lock, however.  Threads never contend
      or wait for the inner lock.
  --  OnDeck provides for futile wakeup throttling a described in section 3.3 of
      See http://www.usenix.org/events/jvm01/full_papers/dice/dice.pdf
      In a sense, OnDeck subsumes the ObjectMonitor _Succ and ObjectWaiter
      TState fields found in Java-level objectMonitors.  (See synchronizer.cpp).

* Waiting threads reside on the WaitSet list -- wait() puts
  the caller onto the WaitSet.  Notify() or notifyAll() simply
  transfers threads from the WaitSet to either the EntryList or cxq.
  Subsequent unlock() operations will eventually unpark the notifyee.
  Unparking a notifee in notify() proper is inefficient - if we were to do so
  it's likely the notifyee would simply impale itself on the lock held
  by the notifier.

* The mechanism is obstruction-free in that if the holder of the transient
  OnDeck lock in unlock() is preempted or otherwise stalls, other threads
  can still acquire and release the outer lock and continue to make progress.
  At worst, waking of already blocked contending threads may be delayed,
  but nothing worse.  (We only use "trylock" operations on the inner OnDeck
  lock).

* Note that thread-local storage must be initialized before a thread
  uses Native monitors or mutexes.  The native monitor-mutex subsystem
  depends on Thread::current().

* The monitor synchronization subsystem avoids the use of native
  synchronization primitives except for the narrow platform-specific
  park-unpark abstraction.  See the comments in os_solaris.cpp regarding
  the semantics of park-unpark.  Put another way, this monitor implementation
  depends only on atomic operations and park-unpark.  The monitor subsystem
  manages all RUNNING->BLOCKED and BLOCKED->READY transitions while the
  underlying OS manages the READY<->RUN transitions.

* The memory consistency model provide by lock()-unlock() is at least as
  strong or stronger than the Java Memory model defined by JSR-133.
  That is, we guarantee at least entry consistency, if not stronger.
  See http://g.oswego.edu/dl/jmm/cookbook.html.

* Thread:: currently contains a set of purpose-specific ParkEvents:
  _MutexEvent, _ParkEvent, etc.  A better approach might be to do away with
  the purpose-specific ParkEvents and instead implement a general per-thread
  stack of available ParkEvents which we could provision on-demand.  The
  stack acts as a local cache to avoid excessive calls to ParkEvent::Allocate()
  and ::Release().  A thread would simply pop an element from the local stack before it
  enqueued or park()ed.  When the contention was over the thread would
  push the no-longer-needed ParkEvent back onto its stack.

* A slightly reduced form of ILock() and IUnlock() have been partially
  model-checked (Murphi) for safety and progress at T=1,2,3 and 4.
  It'd be interesting to see if TLA/TLC could be useful as well.

* Mutex-Monitor is a low-level "leaf" subsystem.  That is, the monitor
  code should never call other code in the JVM that might itself need to
  acquire monitors or mutexes.  That's true *except* in the case of the
  ThreadBlockInVM state transition wrappers.  The ThreadBlockInVM DTOR handles
  mutator reentry (ingress) by checking for a pending safepoint in which case it will
  call SafepointSynchronize::block(), which in turn may call Safepoint_lock->lock(), etc.
  In that particular case a call to lock() for a given Monitor can end up recursively
  calling lock() on another monitor.   While distasteful, this is largely benign
  as the calls come from jacket that wraps lock(), and not from deep within lock() itself.

  It's unfortunate that native mutexes and thread state transitions were convolved.
  They're really separate concerns and should have remained that way.  Melding
  them together was facile -- a bit too facile.   The current implementation badly
  conflates the two concerns.

* TODO-FIXME:

  -- Add DTRACE probes for contended acquire, contended acquired, contended unlock
     We should also add DTRACE probes in the ParkEvent subsystem for
     Park-entry, Park-exit, and Unpark.

  -- We have an excess of mutex-like constructs in the JVM, namely:
     1. objectMonitors for Java-level synchronization (synchronizer.cpp)
     2. low-level muxAcquire and muxRelease
     3. low-level spinAcquire and spinRelease
     4. native Mutex:: and Monitor::
     5. jvm_raw_lock() and _unlock()
     6. JVMTI raw monitors -- distinct from (5) despite having a confusingly
        similar name.

o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o-o
