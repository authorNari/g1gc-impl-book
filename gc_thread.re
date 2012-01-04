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

=== Windowsのスレッド処理開始

=== Linuxのスレッド生成

=== Linuxのスレッド処理開始

== TODO 排他制御
* Park
* Monitor
* Mutex

== TODO GC並列スレッド

== TODO GC並行スレッド
