= HotspotVMのスレッド管理

この章から数章をかけてHotspotVMのスレッド管理方法について見ていきます。
『アルゴリズム編』で述べたとおり、G1GCは並列・並行GCを組み合わせたGCです。
並列・並行GCはそれぞれスレッドを利用して実装されており、複数のスレッドをどのように管理するかが実装の肝になります。

本章ではHotspotVMのスレッド管理の中でもOSに近い土台的な部分を見ていきます。

== スレッド操作の抽象化

WindowsやLinuxにはそれぞれOSのスレッドを利用するライブラリが存在します。
WindowsではWindows APIを使ってスレッドを操作し、LinuxではPOSIXスレッド標準を実装したライブラリであるPthreadsを利用します。

HotspotVM内には、OSよって異なるスレッド操作を共通化する層を設けており、HotspotVM内でスレッドが簡単に利用できるように工夫されています。

== Threadクラス

HotspotVM内ではスレッドを操作する基本的な機能を@<code>{Thread}クラスによって実装し、@<code>{Thread}クラスを継承した子クラスにの実装によってスレッドの振る舞いを決定します。
@<img>{thread_hierarchy}に@<code>{Thread}クラスの継承関係を示します。

//image[thread_hierarchy][Threadクラスの継承関係]

@<code>{Thread}クラスは@<code>{CHeapObj}クラスを継承しているため、インスタンスはCのヒープ領域から直接アロケーションされます。

@<code>{Thread}クラスは仮想関数として@<code>{run()}が定義されています。

//source[share/vm/runtime/thread.hpp]{
94: class Thread: public ThreadShadow {
    ...
1428:  public:
1429:   virtual void run();
//}

@<code>{run()}は生成したスレッド上で実行される関数です。@<code>{Thread}クラスを継承した子クラスで@<code>{run()}を実装し、実際にスレッド上で動作させる処理を定義します。

親クラスの@<code>{ThreadShadow}クラスはスレッド実行中に発生した例外を統一的に扱うためのクラスです。

子クラスの@<code>{JavaThread}クラスはJavaの言語レベルで実行されるスレッドを表現しています。言語利用者がJavaのスレッドを一つ作ると、内部ではこの@<code>{JavaThread}クラスが一つ生成されています。@<code>{JavaThread}クラスはGCとそれほど関係のないクラスですので、本書では詳しく説明しません。

@<code>{NamedThread}クラスはスレッドに対する名前付けをサポートします。@<code>{NamedThread}クラスやその子クラスでは、インスタンスに対して一意の名前を設定できます。
GCスレッドとして利用するクラスは、この@<code>{NamedThread}クラスを継承して実装されます。

== スレッドのライフサイクル

では、実際にスレッドが生成され、処理の開始・終了までを順を追ってみていきましょう。
以下がひとつのスレッドのライフサイクルです。

 1. @<code>{Thread}クラスのインスタンス生成
 2. スレッド生成（@<code>{os::create_thread()}）
 3. スレッド処理開始（@<code>{os::start_thread()}）
 4. スレッド処理終了
 5. @<code>{Thread}クラスのインスタンス解放

まず、1.で@<code>{Thread}クラスのインスタンスを生成します。インスタンス生成時にスレッドを管理するためのリソースを初期化したり、スレッドを生成する前準備を行います。

2.,3.,4.については@<img>{os_thread_start_end_fllow}に図示しました。

//image[os_thread_start_end_fllow][スレッド生成・処理開始・処理終了の流れ図]

2.で実際にスレッドを生成します。この段階ではスレッドを一時停止した状態で作っておきます。

3.で停止していたスレッドを起動します。この段階で、@<code>{Thread}クラスの子クラスで実装された@<code>{run()}メンバ関数が、生成したスレッド上で呼び出されます。

4.のように、@<code>{run()}メンバ関数の処理が終わると、スレッドの処理は終了します。

5.で@<code>{Thread}クラスのインスタンスを解放ます。
その際にデクストラクタにてスレッドで利用してきたリソースも合わせて解放されます。

=== OSThreadクラス

@<code>{Thread}クラスには@<code>{OSThread}クラスのインスタンスを格納する@<code>{_osthread}メンバ変数が定義されています。@<code>{OSThread}クラスはスレッドを操作するのに必要な、各OSに依存したスレッドの情報を保持します。スレッド生成時に@<code>{OSThread}クラスのインスタンスが生成され、@<code>{_osthread}メンバ変数に格納されます。

@<code>{OSThread}クラスの定義では対象のOSごとに異なる下記のようにヘッダファイルを読み込みます。

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

では、Linuxのヘッダファイルを一部見てみましょう。

//source[os/linux/vm/osThread_linux.hpp]{
49:   pthread_t _pthread_id;
//}

@<code>{pthread_t}はPthreadsで利用されるデータ型です。@<code>{_pthread_id}には、Pthreadsによるスレッド操作に必要なpthreadのIDが格納されます。

また、@<code>{OSThread}クラスにはスレッドの現在の状態を保持する@<code>{_state}メンバ変数が定義されています。@<code>{_state}メンバ変数の値は@<code>{ThreadState}で定義された識別子が格納されます。

//source[share/vm/runtime/osThread.hpp]{
44: enum ThreadState {
45:   ALLOCATED,    // アロケーション済みだが初期化はまだ
46:   INITIALIZED,  // 初期化済みだが処理開始はまだ
47:   RUNNABLE,     // 処理開始済みで起動可能
48:   MONITOR_WAIT, // モニターロック競合待ち
49:   CONDVAR_WAIT, // 条件変数待ち
50:   OBJECT_WAIT,  // Object.wait()呼び出しの待ち
51:   BREAKPOINTED, // ブレークポイントで中断
52:   SLEEPING,     // Thread.sleep()中
53:   ZOMBIE        // 終了済みだが回収されていない
54: };
//}

@<code>{_state}メンバ変数は各OS共通で定義され、@<code>{ThreadState}の識別子も各OSで同じものが利用されます。

== Windowsのスレッド生成

この節からは各OSでどのようにスレッドを扱っているのかを見ていきます。
最初はWindows環境でのスレッド生成です。

スレッドを生成するメンバ関数は@<code>{os::create_thread()}に定義されています。@<code>{os::create_thread()}内でおこなう処理の概要を次に示しました。

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
@<code>{os::create_thread()}の引数にとった、スタックサイズ（@<code>{stack_size}）が@<code>{0}であれば、スレッドの種類（@<code>{thr_type}）に対する適切なサイズを決定します。利用する範囲のスタックサイズを指定し、無駄なメモリ消費を抑えるのがこの処理の狙いです。

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

次にWindows APIである@<code>{_beginthreadex()}関数を使ってスレッドを生成します。@<code>{_beginthreadex()}関数の引数には次の情報を渡します。

 1. スレッドのセキュリティ属性。@<code>{NULL}の場合は何も指定されません。
 2. スタックサイズ。@<code>{0}の場合はメインスレッドと同じ値を使用します。
 3. スレッド上で処理する関数のアドレス。
 4. 3.に指定した関数に渡す引数。
 5. スレッドの初期状態。@<code>{CREATE_SUSPENDED}は一時停止を表します。
 6. スレッドIDを受け取る変数へのポインタ。

上記の他に5.（スレッドの初期状態）に対して@<code>{STACK_SIZE_PARAM_IS_A_RESERVATION}フラグを指定していますが、このフラグの詳細はすぐ後の項で説明します。

606、607行目ではスレッド生成時に取得した@<code>{thread_handle}と@<code>{thread_id}を@<code>{OSThread}インスタンスに設定します。

//source[os/windows/vm/os_windows.cpp:os::create_thread()]{
       4. スレッドの状態を@<code>{INITIALIZED}に変更
610:   osthread->set_state(INITIALIZED);

613:   return true;
614: }
//}

最後にスレッドの状態を@<code>{INITIALIZED}に変更して、@<code>{os::create_thread()}の処理は終了です。

=== STACK_SIZE_PARAM_IS_A_RESERVATIONフラグ

ソースコード中のコメントによれば、@<code>{_beginthreadex()}の@<code>{stack_size}の指定にはかなりクセがあり、それを抑止するために@<code>{STACK_SIZE_PARAM_IS_A_RESERVATION}フラグが指定されているようです。ソースコード（@<code>{os/windows/vm/os_windows.cpp}）内のコメントを簡単に以下に翻訳しました。

//quote{
MSDNのドキュメントに書いてあることと実際の動作は違い、_beginthreadex()の"stack_size"はスレッドのスタックサイズを定義しません。
その代わりに、最初のコミット済みメモリサイズを定義します。
スタックサイズは実行ファイルのPEヘッダ(*1)によって定義されます。
ランチャーのスタックサイズのデフォルト値が320kBだったとしましょう。
その場合、320kB以下の"stack_size"はスレッドのスタックサイズにたいして何の影響も与えません。
"stack_size"がPEヘッダのデフォルト値（この場合320kB）より大きい場合、スタックサイズは最も近い1MBの倍数に切り上げられます。
そしてこれは最初のコミット済みメモリサイズに影響があります。
つまり、"stack_size"がPFヘッダのデフォルト値より大きい場合、重大なメモリ使用量の増加を引き起こす可能性があるのです。
なぜなら、意図せずにスタック領域が数MBに切り上げられるだけでなく、その全体の領域が前もって確保されてしまうからです。

最終的にWindows XPはCreateThread()のために"STACK_SIZE_PARAM_IS_A_RESERVATION"を追加しました。
これは"stack_size"を「スタックサイズ」として扱うことができます。
ただ、JVMはCランタイムライブラリを利用するため、MSDNに従うとCreateThread()を直接呼ぶことができません(*2)。


でも、いいニュースです。このフラグは_beginthredex()でもうまく動くようですよ！！

*1:訳注 実行ファイルに定義される、実行に必要な設定を格納する場所のことをPEヘッダと呼ぶ。

*2:訳注 そのため、_beginthreadx()を利用している。
//}

Windows APIの暗黒面を垣間見ましたが、@<code>{STACK_SIZE_PARAM_IS_A_RESERVATION}を@<code>{_beginthredex()}の引数に指定している理由はわかりました。

== Windowsのスレッド処理開始

親スレッドは、@<code>{os::create_thread()}処理後に@<code>{os::start_thread()}を呼び出して生成したスレッドの処理を開始します。
このメンバ関数は各OSで共通のものです。

//source[share/vm/runtime/os.cpp]{
695: void os::start_thread(Thread* thread) {

698:   OSThread* osthread = thread->osthread();
699:   osthread->set_state(RUNNABLE);
700:   pd_start_thread(thread);
701: }
//}

699行目でスレッドの状態を@<code>{RUNNABLE}に設定し、700行目で@<code>{os::pd_start_thread()}を呼び出します。@<code>{os::pd_start_thread()}は各OSで異なるものが定義されています。

//source[os/windows/vm/os_windows.cpp]{
2975: void os::pd_start_thread(Thread* thread) {
2976:   DWORD ret = ResumeThread(thread->osthread()->thread_handle());

2982: }
//}

2976行目でWindows APIの@<code>{ResumeThread()}関数を呼び出し、一時中断していたスレッドを実行します。スレッド上ではじめに起動する関数は、@<code>{_beginthreadex()}の引数に渡していた@<code>{java_start()}です。

//source[os/windows/vm/os_windows.cpp]{
391: static unsigned __stdcall java_start(Thread* thread) {

421:        thread->run();

435:   return 0;
436: }
//}

421行目で、@<code>{Thread}クラスを継承した子クラスで定義した、@<code>{run()}が実行されます。

=== キャッシュラインの有効利用

前節では省略しましたが、@<code>{java_start()}の最初の方に一見意味不明なコードが登場します。

//source[os/windows/vm/os_windows.cpp]{
391: static unsigned __stdcall java_start(Thread* thread) {
397:   static int counter = 0;
398:   int pid = os::current_process_id();
399:   _alloca(((pid ^ counter++) & 7) * 128);

421:        thread->run();

435:   return 0;
436: }
//}

397〜399行目の処理を簡単に説明します。@<code>{_alloca()}はマシンスタック領域からメモリをアロケーションする関数です。@<code>{_alloca()}に渡される数値は、@<code>{java_start()}の呼び出し毎に、@<code>{[0..7]}の範囲で@<code>{1}づつずらした値に、@<code>{128}を掛けたものです。プロセスIDは@<code>{[0..7]}の中でずらしはじめる起点を決定します。もっと簡単に言えば、@<code>{128}の間隔でプロセス・スレッドごとに（重複を含む）ずれた値が@<code>{_alloca()}に渡されます。

この処理はマシンスタックが利用するCPUキャッシュラインの利用箇所を分散する役割があります。

CPUのキャッシュラインとはキャッシュメモリに格納するデータ1単位のことを指します。@<img>{cpu_cache_line}のようにほとんどのキャッシュメモリは数バイトのキャッシュラインで分割されています。CPUは頻繁に使うデータをこのキャッシュライン単位で格納しています。

//image[cpu_cache_line][現在、多くのCPUにはコアに近い側からL1（レベル1）、L2のキャッシュメモリがある。キャッシュメモリはキャッシュラインという単位でデータが格納される。]

問題の処理で懸念しているのは「同じスタックトレースを作るようなスレッドが複数作られた場合、キャッシュラインの利用箇所が偏るかもしれない」ということです。スタックトレースが同じであれば、マシンスタックの各フレームのアドレスの間隔が一致してしまい、スタックフレームが格納されるキャッシュラインが偏る可能性があります。

また、デュアルコアやクアッドコアのCPUではほとんどがL2のキャッシュメモリを共有します。そのため上記の状態に陥ると、スタックにアクセスする際にキャッシュの奪いあいがスレッド間で発生してしまい、スレッドの速度低下につながります。CPUの1プロセッサを2プロセッサに見せかけるハイパースレッディング技術ではL1とL2のキャッシュメモリを共有するため、速度低下はより深刻になります。

そのため、397〜399行目の処理によって、ある程度ずらしたアドレスからマシンスタックをスレッドに作らせることで、キャッシュラインの利用箇所が偏らないようにしています。

== Linuxのスレッド生成

次にLinux環境でのスレッド生成を見てみましょう。
@<hd>{Windowsのスレッド生成}にて紹介した内容と重複するものは省略します。

//source[os/linux/vm/os_linux.cpp:os::create_thread()]{
866: bool os::create_thread(Thread* thread,
                            ThreadType thr_type,
                            size_t stack_size) {

870:   OSThread* osthread = new OSThread(NULL, NULL);

876:   osthread->set_thread_type(thr_type);
877: 
       // 最初の状態はALLOCATED
879:   osthread->set_state(ALLOCATED);
880: 
881:   thread->set_osthread(osthread);
882: 
       // スレッドの属性初期化
884:   pthread_attr_t attr;
885:   pthread_attr_init(&attr);
886:   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

       // 省略: スレッドで使用するスタックサイズの決定

923:   pthread_attr_setguardsize(&attr, os::Linux::default_guard_size(thr_type));
924: 
//}

870〜881行目で@<code>{osthread}を初期化します。Windows版と異なるのは、@<code>{set_state()}で@<code>{ALLOCATED}を設定している点です。

884、885行目でスレッドの属性を初期化します。@<code>{pthread_attr_t}はpthreadの属性を格納する構造体です。885行目の@<code>{pthread_attr_init()}関数で@<code>{attr}変数をPthreadsが定めるデフォルト値に初期化します。

886行目の@<code>{pthread_attr_setdetachstate()}でスレッド属性のデタッチ状態を設定します。@<code>{PTHREAD_CREATE_DETACHED}フラグを設定すると、スレッドはデタッチ状態で生成されます。デタッチ状態のスレッドは、スレッドの処理が終了したときに自動でスレッド自身のリソースが解放されます。ただし、デタッチ状態のスレッドはメインスレッドから切り離された状態で処理を実行しますので、デタッチ状態のスレッドとは合流（join）できません。

923行目では@<code>{pthread_attr_setguardsize()}でスタックのガード領域サイズを指定しています。この点については後の@<hd>{Linuxのスレッド生成|スタックのガード領域}で詳しく取り上げます。

//source[os/linux/vm/os_linux.cpp:os::create_thread()]{
925:   ThreadState state;
926: 
927:   {

934:     pthread_t tid;
935:     int ret = pthread_create(&tid, &attr,
                                  (void* (*)(void*)) java_start,
                                  thread);
936: 
937:     pthread_attr_destroy(&attr);

         // pthread 情報を OSThread に格納
951:     osthread->set_pthread_id(tid);
952: 
         // 子スレッドが初期化 or 異常終了するまで待つ
954:     {
955:       Monitor* sync_with_child = osthread->startThread_lock();
956:       MutexLockerEx ml(sync_with_child, Mutex::_no_safepoint_check_flag);
957:       while ((state = osthread->get_state()) == ALLOCATED) {
958:         sync_with_child->wait(Mutex::_no_safepoint_check_flag);
959:       }
960:     }

965:   }

977:   return true;
978: }
//}

935行目の@<code>{pthread_create()}を使ってスレッドを生成します。
引数には次の情報を渡します。

 1. スレッドIDを受け取る変数へのポインタ。
 2. スタックの属性。今まで育ててきた@<code>{attr}へのポインタを指定する。
 3. スレッド上で処理する関数のアドレス。
 4. 3.に指定した関数に渡す引数。

954〜960行目で、作成したスレッドが初期化されるのを待ちます。
957行目の@<code>{while}ループで、スレッドの状態が@<code>{ALLOCATED}以外に書き換えられるのを待っています。
スレッドの状態は@<code>{java_start()}の中で書き換えられます。
つまり、作成したスレッドの準備が整い、スレッドの処理が実際に実行された時に@<code>{while}ループを抜けます。
958行目の@<code>{wait()}はスレッドを待たせる処理です。@<code>{wait()}の詳細についてはまた後述します。

=== スタックのガード領域

@<code>{os::create_thread()}では次のように、スタックのガード領域のサイズが指定されます。

//source[os/linux/vm/os_linux.cpp:os::create_thread():再掲]{
923:   pthread_attr_setguardsize(&attr, os::Linux::default_guard_size(thr_type));
//}

スタックのガード領域とは、スタックのオーバーフローを防ぐ（ガードする）ための緩衝域として用意されるものです。
@<img>{guard_page}のように、Linux環境ではマシンスタックの上限のすぐ上に余分にガード領域を設けおり、もしスタックが溢れてガード領域にアクセスした場合には@<code>{SEGV}のシグナルが発生します。

//image[guard_page][スタックには、マシンスタックとして使える領域の底の直後にガード領域があり、OSのガード領域にメモリアクセスがあるとスタックオーバーフローとなる。]

ガード領域のサイズとして@<code>{os::Linux::default_guard_size()}が指定されています。この関数の定義は次のとおりです。

//source[os_cpu/linux_x86/vm/os_linux_x86.cpp]{
662: size_t os::Linux::default_guard_size(os::ThreadType thr_type) {
665:   return (thr_type == java_thread ? 0 : page_size());
666: }
//}

Javaスレッド以外は1ページ分のサイズ、Javaスレッドの場合は@<code>{0}のサイズが返されます。
つまり、Javaスレッドの場合はガード領域が作成されません。
これはなぜでしょうか？

@<img>{java_thread_guard_page}に示す通り、実はJavaスレッドは独自のガード領域を別に準備しており、スタックオーバーフローエラーのハンドリングや事後処理などを自前で実装しています。
そのため、JavaスレッドではOSが用意するガード領域に意味がなく、確保されるメモリ領域がもったいないため、@<code>{pthread_attr_setguardsize()}にて@<code>{0}が指定されます。

//image[java_thread_guard_page][Javaスレッドはマシンスタックとして使える領域の底の直前を独自のガード領域として利用している。]

== Linuxのスレッド処理開始

Linux環境では一時停止状態でスレッド生成できないため、@<code>{java_start()}がすぐに実行されてしまいます。

//source[os/linux/vm/os_linux.cpp]{
807: static void *java_start(Thread *thread) {

        /* キャッシュラインをずらす処理 */

819:   OSThread* osthread = thread->osthread();
820:   Monitor* sync = osthread->startThread_lock();

       // 親スレッドとのハンドシェイク
847:   {
848:     MutexLockerEx ml(sync, Mutex::_no_safepoint_check_flag);
849: 
         // 待っている親スレッドを起こす
851:     osthread->set_state(INITIALIZED);
852:     sync->notify_all();

         // os::start_thread() の呼び出しを待つ
855:     while (osthread->get_state() == INITIALIZED) {
856:       sync->wait(Mutex::_no_safepoint_check_flag);
857:     }
858:   }

861:   thread->run();
862: 
863:   return 0;
864: }
//}

848〜852行目で、待ち状態にある親スレッドに対して、初期化が終わったことを通知します。
子スレッドはその後すぐに、855、856行目で@<code>{os::start_thread()}の呼び出し待ちに入ります。

親スレッドは851行目でスレッドの状態が変わったことを検知し、@<code>{os::create_thread()}を終了させ、@<code>{os::start_thread()}を呼び出します。
@<code>{os::start_thread()}はWindowsと共通の関数でした。もう一度見てみましょう。

//source[share/vm/runtime/os.cpp:再掲]{
695: void os::start_thread(Thread* thread) {

698:   OSThread* osthread = thread->osthread();
699:   osthread->set_state(RUNNABLE);
700:   pd_start_thread(thread);
701: }
//}

699行目でスレッドの状態を@<code>{RUNNABLE}に変更しています。
これで子スレッドはwhileループを抜けることができます。
待ちの状態の子スレッドを起こしているのが、@<code>{pd_start_thread()}の処理です。

//source[os/linux/vm/os_linux.cpp]{
1047: void os::pd_start_thread(Thread* thread) {
1048:   OSThread * osthread = thread->osthread();

1050:   Monitor* sync_with_child = osthread->startThread_lock();
1051:   MutexLockerEx ml(sync_with_child, Mutex::_no_safepoint_check_flag);
1052:   sync_with_child->notify();
1053: }
//}

1052行目で子スレッドを起こします。
子スレッドは待ちを抜けた後、@<code>{run()}を呼び出し、ユーザが定義したスレッドの処理を開始します。
