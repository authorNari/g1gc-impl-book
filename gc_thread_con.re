= GCスレッド（並行編）

本章では並列GCがどのように実装されているかを説明します。

== ConcurrentGCThreadクラス
並列GCは@<code>{CuncurrentGCThread}クラスを継承したクラスで実装されます。
@<code>{CuncurrentGCThread}クラスの継承関係を@<img>{cuncurrent_gc_thread_hierarchy}に示します。

//image[cuncurrent_gc_thread_hierarchy][@<code>{CuncurrentGCThread}クラスの継承関係]

並列GCはミューテータとは別スレッドで動くGCのことを指していますので、もちろん@<code>{Thread}クラスを祖先に持っています。

@<code>{CuncurrentGCThread}に定義されている@<code>{create_and_start()}はスレッドの生成・起動をおこなうメンバ関数です。
@<code>{CuncurrentGCThread}を継承するすべての子クラスは、以下のように@<code>{create_and_start()}をコンストラクタで呼び出すため、インスタンスを作ったタイミングでGCスレッドが起動します。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.cpp]{
41: ConcurrentMarkThread::ConcurrentMarkThread(ConcurrentMark* cm) :
42:   ConcurrentGCThread(),
43:   _cm(cm),
44:   _started(false),
45:   _in_progress(false),
46:   _vtime_accum(0.0),
47:   _vtime_mark_accum(0.0),
48:   _vtime_count_accum(0.0)
49: {
50:   create_and_start();
51: }
//}

また、スレッドの処理を定義する@<code>{run()}はそれぞれの子クラスで定義されます。

== SuspendableThreadSetクラス

並行GCスレッド群は@<code>{SuspendableThreadSet}クラスによって停止・起動の制御をおこないます。
@<code>{SuspendableThreadSet}クラスとは、名前の通り、停止可能なスレッドの集合を管理するクラスです。

@<code>{CuncurrentGCThread}クラスは@<code>{SuspendableThreadSet}のインスタンスをクラスの静的メンバ変数として保持します。

//source[share/vm/gc_implementation/shared/concurrentGCThread.hpp]{
78: class ConcurrentGCThread: public NamedThread {

       // すべてのインスタンスで共有
101:   static SuspendibleThreadSet _sts;
//}

101行目で定義された@<code>{_sts}は@<code>{ConcurrentGCThread}を継承したすべてのクラスのインスタンスで共有されます。

=== 集合の操作
@<code>{SuspendableThreadSet}クラスをよく知るために、主要なメンバ関数を説明していきましょう。

まず、集合に対して加入・脱退できるメンバ関数が定義されています。

 * @<code>{join()} - 自スレッドを新たに集合に加わえる。
 * @<code>{leave()} - 集合内の自スレッドが集合から離れる。

@<code>{SuspendableThreadSet}は生成した段階で集合内に1つもスレッドを持っていません。
それぞれのスレッドは加入したい時に@<code>{join()}し、脱退したい時に@<code>{leave()}します。

次に、集合内の全スレッドに対し、停止・再起動するように要求するメンバ関数が定義されています。

 * @<code>{suspend_all()} - 集合内のスレッド全停止要求
 * @<code>{resume_all()} - 集合内のスレッド再起動要求

@<code>{suspend_all()}を呼び出したスレッドは集合内の全スレッドが停止するまで待ち状態になります。
また、もし停止要求中に@<code>{join()}しようとするスレッドがいた場合、そのスレッドも待ち状態になります。

その後、@<code>{resume_all()}を呼び出すと集合内の全スレッドは再起動し、@<code>{join()}待ちのスレッドは待ち状態が解けて集合内に追加されます。

つまり、@<code>{suspend_all()}を呼び出しが完了した後から、@<code>{resume_all()}を呼び出すまで、@<code>{SuspendableThreadSet}内の全スレッドは停止状態にあり、集合にあらたなスレッドが追加されることもありません。

=== 停止するタイミング
@<code>{suspend_all()}を呼び出したあと、集合内のスレッドがすぐに停止するわけではありません。
それぞれのスレッドはそれぞれに都合のよいタイミングで停止します。

@<code>{SuspendableThreadSet}には集合内の各スレッドが停止するため以下のメンバ関数が定義されています。

 * @<code>{should_yield()} - 集合が全停止を要求されているか？
 * @<code>{yield()} - 全停止要求中の場合は自スレッドを停止

各スレッドは、自分が受け持つ処理の節目など、停止してもよいタイミングで@<code>{yield()}を定期的に呼び出すように義務付けられています。

=== 集合外からのyield()呼び出し
実は集合外のスレッドからも@<code>{yield()}を呼ぶことが可能です。
こうなってくるともう集合とか関係ないですね（白目）。

集合外のスレッドから@<code>{yield()}を呼び出した場合の振る舞いは通常ものと同じです。
全停止要求中であれば自スレッドを停止し、@<code>{resume_all()}後に再起動します。

=== 利用イメージ
ここまでに説明した関数を使う利用例を@<img>{suspend_and_interrupt}に示します。

//image[suspend_and_interrupt][@<code>{SuspendableThreadSet}を利用したスレッドの動作制御例。青い矢印上の処理は@<code>{suspend_all()}が成功し、スレッドA・Bが動いていない状態で実行できている。一方、集合外のスレッドCは@<code>{suspend_all()}完了後の@<code>{yield()}呼び出しで停止する。]

まず、メインスレッドは@<code>{suspend_all()}を呼び出し、集合に停止要求を出します。
その後、集合内のすべてのスレッドが停止終わった後で、処理を実行し、最終的に@<code>{resume_all()}を呼び出します。

スレッドAに注目すると、これは@<code>{suspend_all()}呼び出し前に@<code>{join()}を呼び出している唯一のスレッドです。
そのため、@<code>{suspend_all()}呼び出し時には集合内のスレッドはAのみとなります。
スレッドAは定期的に@<code>{yield()}を呼び出しており、@<code>{suspend_all()}後の@<code>{yield()}で自スレッドを停止します。

スレッドBは@<code>{suspend_all()}呼び出し後に@<code>{join()}を呼び出しています。
集合は停止要求を受けていますので、@<code>{join()}を呼び出したスレッドBは停止します。

一方、スレッドCは集合とは関係ないスレッドにも関わらず、定期的に@<code>{yield()}を呼び出しています。
そして、@<code>{suspend_all()}呼び出し後の@<code>{yield()}で自スレッドを停止します。
スレッドCは一時的ではありますが、メインスレッドの青い矢印と同時に動く点に注意してください。
集合と関係のないスレッドが停止することは@<code>{suspend_all()}では保証していません。

まとめると、@<code>{SuspendibleThreadSet}は、集合に関わるスレッドが停止した状態で、何らかの処理を実行できる仕組みを提供します。
@<img>{suspend_and_interrupt}をみると、集合に関わるスレッドA・Bが動いていない状態でメインスレッドの青い部分の処理が動くことがわかると思います。
そして、各スレッドの停止位置は@<code>{yield()}の呼び出しタイミングによって任意に決めることができます。
そのため、処理の区切りなどの安全な位置で停止することが可能です。

== セーフポイント
  * XXOperationの説明
  * safe_point()とは

