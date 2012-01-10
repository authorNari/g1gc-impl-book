= GCスレッド

『アルゴリズム編』で述べたとおり、G1GCは並列・並行GCを組み合わせたGCです。
本章ではHotspotVMが実装しているスレッドの概要と、GCによるスレッドの利用方法を解説します。

== Threadクラス

HotspotVM内ではスレッドを扱うための基本的な機能を@<code>{Thread}クラスに実装し、@<code>{Thread}クラスを継承した子クラスによって実際にスレッドを生成・管理します。
@<img>{thread_hierarchy}に@<code>{Thread}クラスの継承関係を示します。

//image[thread_hierarchy][Threadクラスの継承関係]

@<code>{Thread}クラスは@<code>{CHeapObj}クラスを直接継承しているため、Cのヒープ領域から直接アロケーションされます。

また@<code>{Thread}クラスは仮想関数として@<code>{run()}が定義されています。

//source[share/vm/runtime/thread.hpp]{
94: class Thread: public ThreadShadow {
    ...
1428:  public:
1429:   virtual void run();
//}

@<code>{run()}は生成したスレッド上で実行される関数です。@<code>{Thread}クラスを継承した子クラスで@<code>{run()}を実装し、実際にスレッド上で動作させる処理を定義します。

親クラスの@<code>{ThreadShadow}クラスはスレッド実行中に発生した例外を統一的に扱うためのクラスです。

子クラスの@<code>{JavaThread}クラスはJavaの言語レベルで実行されるスレッドを表現しています。言語利用者がJavaのスレッドを一つ作ると、内部ではこの@<code>{JavaThread}クラスが一つ生成されています。@<code>{JavaThread}クラスはGCとそれほど関係のないクラスですので、本書では詳しく説明しません。

@<code>{NamedThread}クラスはスレッドの名前付けをサポートします。@<code>{NamedThread}クラスや子クラスのインスタンスに対し、一意の名前を設定できます。
GCスレッドとして利用するクラスは、この@<code>{NamedThread}クラスを継承して実装されます。

== スレッドのライフサイクル

スレッドのライフサイクルは以下の通りです。

 1. @<code>{Thread}クラスのインスタンス生成
 2. スレッド生成
 3. スレッド処理開始
 4. スレッド処理終了
 5. @<code>{Thread}クラスのインスタンス解放

まず、1.で@<code>{Thread}クラスのインスタンスを生成します。インスタンス生成時にスレッドを管理するためのリソースを初期化したり、スレッドを生成する前準備を行います。

以降の2.,3.,4.について@<img>{os_thread_start_end_fllow}に図示します。

//image[os_thread_start_end_fllow][スレッド生成・処理開始・処理終了の流れ図]

2.で実際にスレッドを生成します。この段階ではスレッドを一時停止した状態で作っておきます。

3.で停止していたスレッドを起動します。この段階で、@<code>{Thread}クラスの子クラスで実装された@<code>{run()}メンバ関数が、生成したスレッド上で呼び出されます。

4.のように、@<code>{run()}メンバ関数の処理が終わると、スレッドの処理は終了します。

5.で@<code>{Thread}クラスのインスタンスを解放し、デクストラクタにてスレッドで利用してきたリソースも合わせて解放します。

=== OSThreadクラス

@<code>{Thread}クラスには@<code>{OSThread}クラスのインスタンスを格納する@<code>{_osthread}メンバ変数が定義されています。@<code>{OSThread}クラスはスレッドを操作するのに必要な、各OSに依存したスレッドの情報を保持します。スレッド生成時に@<code>{OSThread}クラスのインスタンスが生成され、@<code>{_osthread}メンバ変数に格納されます。

//source[share/vm/runtime/osThread.hpp]{
61: class OSThread: public CHeapObj {
   ...
67:   volatile ThreadState _state; // スレッドの状態
   ...
102:   // Platform dependent stuff
103: #ifdef TARGET_OS_FAMILY_linux
104: # include "osThread_linux.hpp"
105: #endif
106: #ifdef TARGET_OS_FAMILY_solaris
107: # include "osThread_solaris.hpp"
108: #endif
109: #ifdef TARGET_OS_FAMILY_windows
110: # include "osThread_windows.hpp"
111: #endif

//}

@<code>{OSThread}クラスの定義では対象のOSごとに異なるヘッダファイルを読み込みます。Linuxのヘッダファイルを一部見てみましょう。

//source[os/linux/vm/osThread_linux.hpp]{
49:   pthread_t _pthread_id;
//}

@<code>{pthread_t}はPOSIXスレッド標準を実装したライブラリであるPthreadsで利用されるデータ型です。@<code>{_pthread_id}には、Pthreadsによるスレッド操作に必要なpthreadのIDが格納されます。

また、@<code>{OSThread}クラスにはスレッドの現在の状態を保持する@<code>{_state}メンバ変数が定義されています。@<code>{_state}メンバ変数の値は@<code>{ThreadState}で定義された識別子が格納されます。

//source[share/vm/runtime/osThread.hpp]{
44: enum ThreadState {
45:   ALLOCATED, // アロケーション済みだが初期化はまだ
46:   INITIALIZED, // 初期化済みだが処理開始はまだ
47:   RUNNABLE, // 処理開始済みで起動可能
48:   MONITOR_WAIT, // モニターロック競合待ち
49:   CONDVAR_WAIT, // 条件変数待ち
50:   OBJECT_WAIT, // Object.wait()呼び出しの待ち
51:   BREAKPOINTED, // ブレークポイントで中断
52:   SLEEPING, // Thread.sleep()中
53:   ZOMBIE // 終了済みだが回収されていない
54: };
//}

@<code>{_state}メンバ変数は各OS共通で定義され、@<code>{ThreadState}の識別子も各OSで同じものが利用されます。

=== Windowsのスレッド生成

最初にWindows環境でのスレッド生成を見てみましょう。スレッドを生成するメンバ関数は@<code>{os::create_thread()}に定義されています。@<code>{os::create_thread()}内でおこなう処理の概要を以下に示しました。

 1. @<code>{OSThread}のインスタンスを生成
 2. スレッドで使用するマシンスタックのサイズを決定
 3. スレッドを生成し、スレッドの情報を格納
 4. スレッドの状態を@<code>{INITIALIZED}に変更

では、実際に@<code>{os::create_thread()}の中身を見ていきましょう。

//source[os/windows/vm/os_windows.cpp:os::create_thread()]{
510: bool os::create_thread(Thread* thread,
                            ThreadType thr_type,
                            size_t stack_size) {
511:   unsigned thread_id;
512: 
513:   // 1.OSThreadのインスタンスを生成
514:   OSThread* osthread = new OSThread(NULL, NULL);
528:   thread->set_osthread(osthread);
//}

はじめに@<code>{OSThread}のインスタンスを生成して、引数にとった@<code>{Thread}のインスタンスに格納します。

//source[os/windows/vm/os_windows.cpp:os::create_thread()]{
       // 2.スレッドで使用するスタックサイズの決定
530:   if (stack_size == 0) {
531:     switch (thr_type) {
532:     case os::java_thread:
533:       // -Xssフラグで変更可能
534:       if (JavaThread::stack_size_at_create() > 0)
535:         stack_size = JavaThread::stack_size_at_create();
536:       break;
537:     case os::compiler_thread:
538:       if (CompilerThreadStackSize > 0) {
539:         stack_size = (size_t)(CompilerThreadStackSize * K);
540:         break;
541:       }
           // CompilerThreadStackSizeが0ならVMThreadStackSizeを設定する
543:     case os::vm_thread:
544:     case os::pgc_thread:
545:     case os::cgc_thread:
546:     case os::watcher_thread:
547:       if (VMThreadStackSize > 0)
             stack_size = (size_t)(VMThreadStackSize * K);
548:       break;
549:     }
550:   }
//}

その後、スレッドで使用するマシンスタックのサイズを決定します。
@<code>{os::create_thread()}の引数にとった、スタックサイズ（@<code>{stack_size}）が@<code>{0}であれば、スレッドの種類（@<code>{thr_type}）に対する適切なサイズを決定します。ここには利用する範囲のスタックサイズを指定することで無駄なメモリ消費を抑えようという狙いがあります。

@<code>{CompilerThreadStackSize}と@<code>{VMThreadStackSize}はOS環境によって決定されますが、@<code>{JavaThread::stack_size_at_create()}についてはJavaの起動オプションによって言語利用者が指定可能です。

//source[os/windows/vm/os_windows.cpp:os::create_thread()]{
     3. スレッドを生成し、スレッドの情報を格納
573: #ifndef STACK_SIZE_PARAM_IS_A_RESERVATION
574: #define STACK_SIZE_PARAM_IS_A_RESERVATION  (0x10000)
575: #endif
576: 
577:   HANDLE thread_handle =
578:     (HANDLE)_beginthreadex(NULL,
579:       (unsigned)stack_size,
580:       (unsigned (__stdcall *)(void*)) java_start,
581:       thread,
582:       CREATE_SUSPENDED | STACK_SIZE_PARAM_IS_A_RESERVATION,
583:       &thread_id);

606:   osthread->set_thread_handle(thread_handle);
607:   osthread->set_thread_id(thread_id);
//}

次にWindows APIである@<code>{_beginthreadex()}関数を使ってスレッドを生成します。@<code>{_beginthreadex()}関数の引数には以下の情報を渡します。

 1. スレッドのセキュリティ属性。@<code>{NULL}の場合は何も指定されません。
 2. スタックサイズ。@<code>{0}の場合はメインスレッドと同じ値を使用します。
 3. スレッド上で処理する関数のアドレス。
 4. スレッドの初期状態。@<code>{CREATE_SUSPENDED}は一時停止を表します。
 5. スレッドIDを受け取る変数へのポインタ。

加えて引数のスレッドの初期状態に@<code>{STACK_SIZE_PARAM_IS_A_RESERVATION}を指定しています。ソースコード中のコメントによれば、@<code>{_beginthreadex()}の@<code>{stack_size}の指定はかなりクセがあり、それを抑止するためにこのフラグが指定されているようです。ソースコード中の文を簡単に以下に翻訳しました。

//quote{
MSDNのドキュメントとは反対に、_beginthreadex()の"stack_size"はスタックサイズを定義されません。
その代わりに、最初の確保されたメモリを定義します。
スタックサイズは実行ファイルのPEヘッダ(*1)によって定義されます。
もし、"stack_size"がPEヘッダのデフォルト値より大きければ、スタックサイズは最も近い1MBの倍数に切り上げられます。
例えば、ランチャーのスタックサイズのデフォルト値が320KBだったとして、320KB以下のサイズはスタックサイズに何の影響も与えません。
この場合は、最初の確保されたメモリサイズにのみ影響があります。
一方、デフォルト値より大きな"stack_size"を指定した場合は、重大なメモリ使用量の増加を引き起こす可能性があります。
なぜなら、スタック領域が数MBに切り上げられるだけでなく、その全体の領域が前もって確保されるからです。

最終的にWindows XPはCreateThread()のために"STACK_SIZE_PARAM_IS_A_RESERVATION"を追加しました。
これは"stack_size"をスタックサイズとして扱うことができます。
ただ、JVMはCランタイムライブラリを利用するため、CreateThread()をMSDNに従って直接呼ぶことができません。

でも、いいニュースです。このフラグは_beginthredex()でもうまく動くようですよ！！

*1:訳注 PEヘッダとは実行ファイルに定義される実行に必要な設定を格納する場所。
//}

Windows APIの暗黒面を垣間見ましたが、@<code>{STACK_SIZE_PARAM_IS_A_RESERVATION}を指定している理由はわかりました。

606・607行目ではスレッド生成時に取得した@<code>{thread_handle}と@<code>{thread_id}を@<code>{OSThread}インスタンスに設定します。

//source[os/windows/vm/os_windows.cpp:os::create_thread()]{
       4. スレッドの状態を@<code>{INITIALIZED}に変更
610:   osthread->set_state(INITIALIZED);

613:   return true;
614: }
//}

最後にスレッドの状態を@<code>{INITIALIZED}に変更して、@<code>{os::create_thread()}の処理は終了です。

=== Windowsのスレッド処理開始

=== Linuxのスレッド生成

=== Linuxのスレッド処理開始

== TODO 排他制御
* Park
* Monitor
* Mutex

== TODO GC並列スレッド

== TODO GC並行スレッド
