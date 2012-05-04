= GCスレッド（並行編）

本章では並行GCの説明とそれにまつわるトピックを紹介します。

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
ここまでに説明した関数の利用例を@<img>{suspend_and_interrupt}に示します。

//image[suspend_and_interrupt][@<code>{SuspendableThreadSet}を利用したスレッドの動作制御例。青い矢印上の処理は@<code>{suspend_all()}が成功し、スレッドA・Bが動いていない状態で実行できている。一方、集合外のスレッドCは@<code>{suspend_all()}完了後の@<code>{yield()}呼び出しで停止する。]

まず、メインスレッドは@<code>{suspend_all()}を呼び出し、集合に停止要求を出します。
その後、集合内のすべてのスレッドが停止終わった後で、処理を実行し、最終的に@<code>{resume_all()}を呼び出します。

スレッドAは@<code>{suspend_all()}呼び出し前に@<code>{join()}を呼び出している唯一のスレッドです。
そのため、@<code>{suspend_all()}呼び出し時に集合内のスレッドはAのみとなります。
スレッドAは定期的に@<code>{yield()}を呼び出しており、@<code>{suspend_all()}後の@<code>{yield()}で自スレッドを停止します。

スレッドBは@<code>{suspend_all()}呼び出し後に@<code>{join()}を呼び出しています。
集合は停止要求を受けていますので、@<code>{join()}を呼び出したスレッドBは停止します。

一方、スレッドCは集合とは関係ないスレッドにも関わらず、定期的に@<code>{yield()}を呼び出しています。
そして、@<code>{suspend_all()}呼び出し後の@<code>{yield()}で自スレッドを停止します。
スレッドCは一時的ではありますが、メインスレッドの青い矢印と同時に動く点に注意してください。
集合と関係のないスレッドが停止することは@<code>{suspend_all()}では保証していません。

まとめると、@<code>{SuspendibleThreadSet}は、集合に関わるスレッドが停止した状態で、何らかの処理を実行できる仕組みを提供します。
@<img>{suspend_and_interrupt}をみると、集合に関わるスレッドA・Bが動いていない状態でメインスレッドの青い部分の処理が動くことがわかると思います。
そして、各スレッドの停止位置は@<code>{join()}・@<code>{yield()}の呼び出しタイミングによって任意に決めることができます。
そのため、処理の区切りなどの安全な位置で停止することが可能です。

== セーフポイント
Hotspotには@<b>{セーフポイント}と呼ばれる謎な用語があります。
よく「システム全体の『安全な状態』を『セーフポイント』と呼ぶ」などと説明されますが、正直この説明では安全な状態が具体的に何であるか理解できません。
実は、セーフポイントはGCのルートとかなり密接な関係にあり、GCをよく知っていないと説明できない用語なのです。
そのため、上記の奥歯にものが挟まったような説明になりがちです。

=== セーフポイントとは？
セーフポイントとは、プロブラム実行中のすべてのルートを矛盾なく列挙できる状態のことを指します。
ルートとはマーキングやコピーなどでオブジェクトのポインタをたどる際の起点となる部分のことです。
そのため、ルートの「矛盾のない列挙」・「すべて列挙」という2点を満たせなければ、生存オブジェクトを見逃すおそれがあります。

ルートを矛盾なく列挙するのにもっとも簡単は方法は、列挙の際にルートを変更されなくすることです。
これについては、ミューテータなどのルートを変更するスレッドを停止する方法が最も簡単です。
そのため、HotspotVMではセーフポイントとしてすべてのJavaスレッドを停止します。

じゃあ単純に止めるだけか、というとそうでもありません。
スレッドを停止する前に、自分の抱えるルートをGCに見える位置に提供しなければならないのです。
そうしないと、GCはすべてのルートを見つけることができません。

具体的な問題としてJITコンパイラの例があります。
JITコンパイラでメソッドをコンパイルする際に、スタックやレジスタのどの部分がオブジェクトへの参照であるかを示す@<b>{スタックマップ}と呼ばれるもの生成します。
そして、GCはこのスタックマップを参考にルートを列挙するわけです。
生成したスタックマップの保持には容量的なコストがかかるので、特定のタイミングのスタックマップしか生成しません。
そのため、セーフポイントとしてスレッドを停止するタイミングは、スタックマップを保持しているタイミングでなければなりません。
スタックマップの詳細については、@<hd>{precise|スタックマップ|コンパイル済みフレーム}でまた詳しく説明します。

つまり、セーフポイントとはわかりやすく言ってしまえば、ミューテータのすべてのスレッドを安全に停止している状態です。
そして、ここで言う「安全に停止している状態」という意味は、「ルートを安全に列挙できる状態」という意味になります。

=== 並行GCスレッドのセーフポイント
Javaスレッドだけがルートを持っているわけではありません。
例えば『アルゴリズム編 3.8 ステップ2 ールート退避』では「並行マーキングで使用中のオブジェクト」をルートとしてあげています。
また、退避用記憶集合維持スレッドもミューテータと並行に走っているスレッドであり、退避用記憶集合もルートとして扱われます。
つまり、これらの並行GCスレッドでも、きちんとGCに見えるところにルートを提供してから停止する必要があるわけです。

ここで登場するのが@<hd>{SuspendableThreadSetクラス}で説明した内容です。
セーフポイントを開始する@<code>{SafepointSynchronize::begin()}の一部を見てみましょう。

//source[share/vm/runtime/safepoint.cpp]{
101: void SafepointSynchronize::begin() {

117:     ConcurrentGCThread::safepoint_synchronize();
//}

117行目で@<code>{ConcurrentGCThread}の@<code>{safepoint_synchronize()}を呼び出しているのがわかると思います。

//source[share/vm/gc_implementation/shared/concurrentGCThread.cpp]{
57: void ConcurrentGCThread::safepoint_synchronize() {
58:   _sts.suspend_all();
59: }
//}

58行目の@<code>{_sts}は@<code>{SuspendableThreadSet}のことでした。
@<code>{suspend_all()}を呼び出していますね。

次に、セーフポイントを終了する@<code>{SafepointSynchronize::end()}の一部を見てみましょう。

//source[share/vm/runtime/safepoint.cpp]{
397: void SafepointSynchronize::end() {

480:     ConcurrentGCThread::safepoint_desynchronize();
//}

今度は@<code>{safepoint_desynchronize()}を呼び出しています。
@<code>{safepoint_desynchronize()}の内部では@<code>{SuspendableThreadSet}の@<code>{resume_all()}を呼び出すだけです。


つまり、セーフポイントでは@<code>{SuspendableThreadSet}を使って並行GCスレッドの動作を制御しています。
並行GCスレッド群はセーフポイントになると、ルートを安全に列挙できる状態で@<code>{yield()}を呼び出し、自分自身を停止するわけです。

== VMスレッド
HotspotVMにはVMスレッドという特別なスレッドがたった1つだけ動いています。
VMスレッドの役割は「VMオペレーション」というVM全体に関わる処理の要求を受け取り、VMスレッド上で実行するという点です。

=== VMスレッドとは？
VMスレッドは@<code>{VMTread}クラスで定義されたスレッドです。
@<code>{VMThread}の祖先にはもちろん@<code>{Thread}クラスがいます。
VMスレッドはJavaを起動してすぐに生成・起動します。

//source[share/vm/runtime/vmThread.hpp]{
101: class VMThread: public NamedThread {

       // VMオペレーションの実行
128:   static void execute(VM_Operation* op);
//}

VMスレッドはVMオペレーションを受け付けるキューを内部に保持しています。
他スレッドは128行目の@<code>{execute()}静的メンバ関数をVMオペレーションを引数に呼び出し、内部のキューに追加させます。
VMスレッドはキューにVMオペレーションが追加されたことを検知して、自身のスレッドでVMオペレーションとして渡された処理を実施します。

=== VMオペレーション
VMオペレーションの代表的なものとしては、スタックトレースの取得や、VMの終了、VMヒープのダンプがあります。
GCにもっとも関係のあるオペレーションはいわゆる「Stop-the-World」で実行しなければならない停止処理です。
G1GCで言うところの退避や、並行マーキングの停止処理はVMオペレーションとしてVMスレッドに実行してもらいます。
また、Javaで明示的にフルGCを実行した場合も停止処理ですのでVMオペレーションとなります。

VMオペレーションはセーフポイントで実行する必要があるものがほとんどです。
そのためほとんどのVMオペレーション実行時には、VMスレッドは@<code>{SafepointSynchronize::begin()}を使ってセーフポイントの状態にもっていきます。

=== VM_Operationクラス
@<code>{VM_Operation}クラスがVMオペレーションのインターフェースを定義するクラスです。
@<code>{VM_Operation}クラスの継承関係を@<img>{vm_operation_hierarchy}に示します。

//image[vm_operation_hierarchy][@<code>{VM_Operation}クラスの継承関係]

@<code>{VM_Operation}クラスのインターフェースを見てみましょう。

//source[share/vm/runtime/vm_operations.hpp]{
98: class VM_Operation: public CHeapObj {

       // VMスレッドが呼び出すメソッド
135:   void evaluate();

144:   virtual void doit()                            = 0;
145:   virtual bool doit_prologue()                   { return true; };
146:   virtual void doit_epilogue()                   {};
//}

VMスレッドは135行目の@<code>{evaluate()}メンバ関数を呼び出し、要求されたオペレーションを実行します。
@<code>{evaluate()}内部では単純に144行目の@<code>{doit()}を呼び出すだけです。

144〜146行目には仮想関数が定義されています。
@<code>{doit()}はオペレーションとしてVMスレッド上で実行される関数です。
@<code>{doit_prologue()}は名前の通り、@<code>{doit()}を実行する前の準備として実行されます。
@<code>{doit_prologue()}は真偽値を返す決まりになっており、@<code>{false}を返した場合は@<code>{doit()}を実行しません。
@<code>{doit_epilogue()}は@<code>{doit()}が終わった後に実行される関数です。

@<code>{VM_Operation}を継承したクラスでは上記の3つのメンバ関数に対し、オペレーションとしての処理の内容を記述して行きます。

=== VMオペレーションの実行例
実際のVMオペレーション実行例を見てみましょう。
ここではG1GCの並行マーキングの初期マークフェーズを見たいと思います。
初期マークフェーズは停止処理ですので、VMオペレーションとして実行されます。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.cpp]{
134:         CMCheckpointRootsInitialClosure init_cl(_cm);
135:         strcpy(verbose_str, "GC initial-mark");
136:         VM_CGC_Operation op(&init_cl, verbose_str);
137:         VMThread::execute(&op);
//}

136行目で@<code>{VM_CGC_Operation}をスタック上に生成し、@<code>{execute()}に渡してします。
VMオペレーションのコンストラクタにはそれぞれのオペレーション内で利用するデータを渡します。
この場合は@<code>{CMCheckpointRootsInitialClosure}と文字列だったようですね。

@<code>{execute()}を呼び出したスレッドはVMオペレーションが終了するまでブロックされます。
VMオペレーションの種類によってはブロックされないこともありますが、それはとても稀な例と考えていいでしょう。
