= GCスレッド（並列編）

本章ではHotspotVMに実装されている複数のスレッドによる並列なタスク実行の枠組みを説明し、それが並列GCがどのように利用されているかを見ていきます。

== 並列実行の流れ

HotspotVMには複数のスレッドで並列に「何かのタスク」を実行する仕組みが実装されています。
それを構成する主な登場人物は次のとおりです。

  * @<code>{AbstractWorkGang} - ワーカーの集団
  * @<code>{AbstractGangTask} - ワーカーに実行させるタスク
  * @<code>{GangWorker} - 与えられたタスクを実行するワーカー

では、上記の登場人物が並列にタスクを実行する際の一連の流れを説明しましょう。

まず、@<code>{AbstractWorkGang}は1つだけモニタを持っており、モニタの待合室には@<code>{AbstractWorkGang}に所属する@<code>{GangWorker}を待たせいます（@<img>{work_gang_do_task_1}）。

//image[work_gang_do_task_1][1. @<code>{AbstractWorkGang}はモニタを1つだけ持ち、@<code>{GangWorker}を待合室に待たせている。]

@<code>{AbstractWorkGang}は1つだけモニタを持っており、モニタの待合室には@<code>{AbstractWorkGang}に所属する@<code>{GangWorker}を待たせておきます。
モニタが排他制御する共有リソースはタスク情報の掲示板です。
掲示板には次の情報が書きこまれます。

 * タスクの場所
 * タスクの通し番号
 * 実行ワーカー総数
 * 実行完了ワーカー総数

次に、クライアントがタスクを並列実行して欲しいタスクの情報を書き込みに来ます（@<img>{work_gang_do_task_2}）。

//image[work_gang_do_task_2][2. クライアントがモニタのロックを取り、タスクの情報を書き込む。]

クライアントが持ってくる実際のタスクは@<code>{AbstractGangTask}を継承した任意のクラスのインスタンスです。
クライアントはタスクの場所としてインスタンスのアドレスを書き込みます。

タスクの通し番号は前回のものから@<code>{+1}したものを書き込みます。
この場合は@<code>{1}になります。
実行ワーカー総数・実行完了ワーカー総数は@<code>{0}に初期化します。

次に、クライアントは待っているワーカーをすべて呼び出して、自身は待合室に入ります（@<img>{work_gang_do_task_3}）。

//image[work_gang_do_task_3][3. ワーカーは1つずつモニタに入り、掲示板の情報を手持ちの紙に書き写し、出ていく。]

呼び出されたワーカーは1つずつモニタに入り、掲示板の情報を確認します。
ワーカーは自分が前回に実行したタスクの通し番号を記録しており、もし掲示板の通し番号と記録した番号が同じだった場合は、重複した実行を避けるためにタスクを無視して待合室に入ります。
通し番号が異なる場合は新しいタスクとみなし、手持ちの紙に掲示板の情報（タスクの場所・通し番号）を書き込みます。
その後、掲示板の実行ワーカー総数を@<code>{+1}して、モニタの外に出てタスクを実行します。

次に、タスクの実行が終わるとワーカーは再度モニタに入り、タスクが終了したことを伝えるため、掲示板の実行完了ワーカー総数を@<code>{+1}します。（@<img>{work_gang_do_task_4}）。

//image[work_gang_do_task_4][4. タスクを終えたワーカーは再度モニタに入り、掲示板の情報を書き換えた後で待合室に入る。]

そして、待合室の全員を呼び出し（クライアントを含む）、自分は待合室に入ります。
ワーカーのタスク実行がすべて終了すると、実行ワーカー総数は実行完了ワーカー総数と同じになります。

クライアントはモニタに入ると、掲示板ですべてのワーカーが終了したか確認します（@<img>{work_gang_do_task_5}）。

//image[work_gang_do_task_5][5. クライアントはモニタに入った後、すべてのワーカーがタスクを終えたことを確認し、モニタを出ていく。]

もし、終えていないタスクがあれば、待合室でワーカーがタスクを終えるまで待ちます。
すべてのワーカーがタスクを終えていれば、クライアントは満足してモニタから出て行きます。

以上が並列実行の流れです。

== AbstractWorkGangクラス
ここからはそれぞれの登場人物の詳細を述べていきます。

@<code>{AbstractWorkGang}クラスの継承関係を@<img>{abstract_worker_gang_hierarchy}に示します。

//image[abstract_worker_gang_hierarchy][@<code>{AbstractWorkGang}クラスの継承関係]

@<code>{AbstractWorkGang}クラスは@<code>{WorkGang}に必要なインタフェースを定義するクラスです。

//source[share/vm/utilities/workgroup.hpp]{
119: class AbstractWorkGang: public CHeapObj {

127:   virtual void run_task(AbstractGangTask* task) = 0;

139:   // 以降に定義されたデータを保護、
140:   // また変更を通知するモニタ
141:   Monitor*  _monitor;

146:   // この集団に属するワーカーの配列。
148:   GangWorker** _gang_workers;
149:   // この集団に与えるタスク
150:   AbstractGangTask* _task;
151:   // 現在のタスクの通し番号
152:   int _sequence_number;
153:   // 実行ワーカー総数
154:   int _started_workers;
155:   // 実行完了ワーカー総数
156:   int _finished_workers;
//}

127行目に定義されている仮想関数の@<code>{run_task()}は、タスクを@<code>{worker}に渡して実行させる処理です。
@<code>{run_task()}の実体は子クラスの@<code>{WorkGang}クラスに定義されています。

139〜156行目までは@<code>{WorkGang}に必要な属性が定義されています。
これは@<hd>{並列実行の流れ}で説明した「タスク情報の掲示板」のデータに相当します。

@<img>{abstract_worker_gang_hierarchy}で示した@<code>{FlexibleWorkGang}クラスは実行可能なワーカー数を後から柔軟に（Flexible）変更できる機能を持つクラスです。
並列GCにはこの@<code>{FlexibleWorkGang}クラスがよく利用されます。

== AbstractGangTaskクラス

@<code>{AbstractGangTask}クラスの継承関係を@<img>{abstract_gang_task_hierarchy}に示します。

//image[abstract_gang_task_hierarchy][@<code>{AbstractGangTask}クラスの継承関係]

@<code>{AbstractGangTask}は並列実行されるタスクとして必要なインタフェースを定義するクラスです。

//source[share/vm/utilities/workgroup.hpp]{
64: class AbstractGangTask VALUE_OBJ_CLASS_SPEC {
65: public:

68:   virtual void work(int i) = 0;
//}

もっとも重要なメンバ関数は68行目の@<code>{work()}です。
@<code>{work()}はワーカーの識別番号（連番）を受け取り、タスクを実行する関数です。

タスクの詳細な処理は、@<code>{G1ParTask}のようなそれぞれの子クラスで@<code>{work()}として定義します。
クライアントは@<code>{AbstractGangTask}の子クラスのインスタンスを@<code>{AbstractWorkGang}に渡して、並列実行してもらうわけです。

== GangWorkerクラス
@<code>{GangWorker}クラスはタスクを実際に実行するクラスで、@<code>{Thread}クラスを祖先に持ちます。

//image[gang_worker_hierarchy][@<code>{GangWorker}クラスの継承関係]

@<code>{GangWorker}のインスタンスは1つのスレッドと対応しているため、ワーカースレッドと呼ばれます。

//source[share/vm/utilities/workgroup.hpp]{
264: class GangWorker: public WorkerThread {

278:   AbstractWorkGang* _gang;
//}

@<code>{GangWorker}は自分が所属する@<code>{AbstractWorkGang}をメンバ変数に持っています。

== 並列GCの実行例
では、実際のコードを読みながら@<hd>{並列実行の流れ}の内容を振り返りましょう。

@<list>{par_mark_sample_code}にクライアントとなるメインスレッドが実行する並列GCの実行サンプルを示しました。

//listnum[par_mark_sample_code][並列GC実行のサンプルコード]{
/* 1. ワーカーの準備 */
workers = new FlexibleWorkGang("Parallel GC Threads", 8, true, false);
workers->initialize_workers();

/* 2. タスクの生成 */
CMConcurrentMarkingTask marking_task(cm, cmt);

/* 3. タスクの並列実行 */
workers->run_task(&marking_task);
//}

=== 1. ワーカの準備
まず、@<list>{par_mark_sample_code}の1.に示した部分で@<code>{FlexibleWorkGang}のインスタンスを生成・初期化し、@<img>{work_gang_do_task_1}の状態にします。

@<code>{FlexibleWorkGang}の生成・初期化のシーケンス図は次のとおりです。

//image[workgang_initialize_sequence][@<code>{WorkGang}の生成・初期化のシーケンス図]

上から順番に見ていきましょう。
最初は@<code>{AbstractWorkGang}のコンストラクタです。

//source[share/vm/utilities/workgroup.cpp]{
33: AbstractWorkGang::AbstractWorkGang(const char* name,
34:                                    bool  are_GC_task_threads,
35:                                    bool  are_ConcurrentGC_threads) :
36:   _name(name),
37:   _are_GC_task_threads(are_GC_task_threads),
38:   _are_ConcurrentGC_threads(are_ConcurrentGC_threads) {


     _monitor = new Monitor(Mutex::leaf,
                            "WorkGroup monitor",
                            are_GC_task_threads);

48:   _terminate = false;
49:   _task = NULL;
50:   _sequence_number = 0;
51:   _started_workers = 0;
52:   _finished_workers = 0;
53: }
//}

上記のコードから、モニタの初期化とデータの初期化がおこなわれることがわかれば充分です。
それ以外の箇所はあまり関係ないので無視しましょう。

@<code>{AbstractWorkGang}クラスのインスタンスが生成された後、@<code>{initialize_workers()}メンバ関数でワーカーの初期化をします。

//source[share/vm/utilities/workgroup.cpp]{
74: bool WorkGang::initialize_workers() {

81:   _gang_workers = NEW_C_HEAP_ARRAY(GangWorker*, total_workers());

92:   for (int worker = 0; worker < total_workers(); worker += 1) {
93:     GangWorker* new_worker = allocate_worker(worker);
95:     _gang_workers[worker] = new_worker;
96:     if (new_worker == NULL || !os::create_thread(new_worker, worker_type)) {
          /* 省略: エラー処理 */
98:       return false;
99:     }
101:       os::start_thread(new_worker);
103:   }
104:   return true;
105: }
//}

81行目で生成したいワーカー分の配列を作成し、92〜103行目でワーカーを生成します。

93行目で@<code>{allocate_worker()}を使って@<code>{GangWorker}を生成します。
96行目でワーカースレッドを生成し、101行目でワーカースレッドの処理を開始します。

93行目の@<code>{allocate_worker()}のソースコードは次の通りです。

//source[share/vm/utilities/workgroup.cpp]{
64: GangWorker* WorkGang::allocate_worker(int which) {
65:   GangWorker* new_worker = new GangWorker(this, which);
66:   return new_worker;
67: }
//}

@<code>{this}（自分の所属する@<code>{AbstractWorkGang}）と、ワーカーの識別番号を引数にして、@<code>{GangWorker}のインスタンスを作っています。

さて、@<code>{initialize_workers()}内の@<code>{os::start_thread()}によって、スレッドの処理は実行しています。
@<code>{GangWorker}は@<code>{Thread}を継承したクラスです。
スレッドは処理を開始すると子クラスの@<code>{run()}メソッドを呼び出すのでしたね。
この場合は@<code>{GangWorker}クラスの@<code>{run()}が呼び出されます。

//source[share/vm/utilities/workgroup.cpp]{
222: void GangWorker::run() {

224:   loop();
225: }
//}

@<code>{run()}では@<code>{loop()}を呼び出しています。
ここでは@<code>{GangWorker}がモニタの待合室に入る部分のみに絞って解説したいと思います。

//source[share/vm/utilities/workgroup.cpp]{
241: void GangWorker::loop() {
243:   Monitor* gang_monitor = gang()->monitor();
247:     {

249:       MutexLocker ml(gang_monitor);

268:       for ( ; /* break or return */; ) {
             /* 
              * 省略: タスクがあるかチェック
              *       あれば break でループを抜ける
              */

283:         // ロックを解除して待合室に入る
284:         gang_monitor->wait(/* no_safepoint_check */ true);

             /* 省略: 待合室から出た後の処理 */
300:       }

302:     }

323: }
//}

まず、243行目で自分の所属する@<code>{AbstractWorkGang}のモニタを取得します。
249行目でロックを掛けてモニタに入ります。
その後、268行目のループのはじめの方でタスクがあるかチェックします。
スレッド起動時にはタスクがないことが多いので、このタイミングではたいていが284行目の@<code>{wait()}を呼び出します。

=== 2. タスクの生成
ワーカーの準備ができたら、次に実行させるタスクを生成します。
@<list>{par_mark_sample_code}の2.の部分を参照してください。
ここでは@<code>{AbstractGangTask}を継承した@<code>{CMConcurrentMarkingTask}という、G1GCのマーキングタスクを実例として取り上げています。

//source[share/vm/gc_implementation/g1/concurrentMark.cpp]{
1089: class CMConcurrentMarkingTask: public AbstractGangTask {
1090: private:
1091:   ConcurrentMark*       _cm;
1092:   ConcurrentMarkThread* _cmt;

1094: public:
1095:   void work(int worker_i) {

        /* 省略: マーキング処理 */

1153:   }

1155:   CMConcurrentMarkingTask(ConcurrentMark* cm,
1156:                           ConcurrentMarkThread* cmt) :
1157:       AbstractGangTask("Concurrent Mark"), _cm(cm), _cmt(cmt) { }
//}

1155〜1157行目に定義されている@<code>{CMConcurrentMarkingTask}のコンストラクタでは、@<code>{work()}を実行するのに必要な変数を引数として受け取るようにしています。
@<code>{work()}の引数は決められているので、それぞれのタスク実行に必要な情報はタスクインスタンスのメンバ変数として保持しなければなりません。

1095〜1153行目が@<code>{CMConcurrentMarkingTask}が実行するタスクの内容です。
生成されたそれぞれの@<code>{GangWorker}はこの@<code>{work()}を呼び出すことになります。

=== 3. タスクの並列実行
最後にタスクをワーカーに渡します。

@<list>{par_mark_sample_code}の3.の部分では@<code>{FlexibleWorkGang}の@<code>{run_task()}を呼び出しています。

//source[share/vm/utilities/workgroup.cpp:run_task()前半]{
129: void WorkGang::run_task(AbstractGangTask* task) {

132:   MutexLockerEx ml(monitor(), Mutex::_no_safepoint_check_flag);

139:   _task = task;
140:   _sequence_number += 1;
141:   _started_workers = 0;
142:   _finished_workers = 0;

//}

タスクを引数に受け取った@<code>{run_task()}は、まず132行目でモニタのロックを取ります。
その後、139行目でタスク情報を書き込み、140〜142行目でその他の情報も更新します。
これは@<img>{work_gang_do_task_2}と対応する部分です。

//source[share/vm/utilities/workgroup.cpp:run_task()後半]{

144:   monitor()->notify_all();

146:   while (finished_workers() < total_workers()) {

152:     monitor()->wait(/* no_safepoint_check */ true);
153:   }
154:   _task = NULL;

160: }
//}

その後、144行目でモニタの待合室にいるワーカーを呼び出します。
146〜153行目のwhileループの終了条件は「すべてのワーカーがタスクを終了すること」です。
条件に合わない場合、152行目でクライアントは@<code>{wait()}し続けます。
この部分は@<img>{work_gang_do_task_5}と対応しています。

それぞれのワーカーは@<code>{GangWorker}の@<code>{loop()}で@<code>{wait()}を呼び出し、実行可能なタスクが与えられることを待っていました。
もう少し@<code>{loop()}を詳細に見ていきましょう。

//source[share/vm/utilities/workgroup.cpp:loop()前半]{
241: void GangWorker::loop() {
242:   int previous_sequence_number = 0;
243:   Monitor* gang_monitor = gang()->monitor();
244:   for ( ; /* タスク実行ループ */; ) {
245:     WorkData data;
246:     int part;
247:     {
249:       MutexLocker ml(gang_monitor);

268:       for ( ; /* タスク取得ループ */; ) {

276:         if ((data.task() != NULL) &&
277:             (data.sequence_number() != previous_sequence_number)) {
278:           gang()->internal_note_start();
279:           gang_monitor->notify_all();
280:           part = gang()->started_workers() - 1;
281:           break;
282:         }

284:         gang_monitor->wait(/* no_safepoint_check */ true);
285:         gang()->internal_worker_poll(&data);
300:       }

302:     }

308:     data.task()->work(part);
//}

242行目の@<code>{previous_sequence_number}は名前の通り、以前のタスクの通し番号を記録するローカル変数です。

244行目からの@<code>{for}ループが一度回るたびにワーカーは1つのタスク実行をこなします。
245行目の@<code>{WorkData}は@<code>{WorkerGang}にあるタスクの情報（掲示板の情報）を記録するローカル変数です。
また、246行目の@<code>{park}はワーカーの順番を記録するローカル変数です。
これらはタスク実行ループのスコープに定義されたローカル変数ですので、タスク実行ループが一回終了するたびに破棄されます。

268行目からの@<code>{for}ループは@<code>{WorkerGang}からタスクを取得するループです。
通常は284行目で待っている状態で止まっていて、@<code>{notify_all()}によって動き出します。
動き出すと、285行目の@<code>{internal_worker_poll()}でローカル変数にタスクの情報をコピーします。
情報を取得したら、276,277行目の条件分岐で実行すべきタスクがあるかどうかチェックします。
チェックに合格したら、278行目で自分が起動したことを@<code>{GangWorker}に書きこみ、279行目で@<code>{notify_all()}して、ワーカーの順番を@<code>{part}に格納してからループを脱出します。
ループを抜けるときに一緒にモニタをアンロックしていることに注意してください。

その後、308行目でタスクの@<code>{work()}を@<code>{part}を引数として呼び出しています。
ここで実際にタスクを実行します。

ここまで部分は@<img>{work_gang_do_task_3}と対応しています。

//source[share/vm/utilities/workgroup.cpp:loop()後半]{
309:     {

314:       // ロック
315:       MutexLocker ml(gang_monitor);
316:       gang()->internal_note_finish();
317:       // 終了したことを伝える
318:       gang_monitor->notify_all();

320:     }
321:     previous_sequence_number = data.sequence_number();
322:   }
323: }
//}

次にタスクの実行が終わると、再度ロックを取って実行完了したことを@<code>{GangWorker}に書き込みます。
その後、@<code>{notify_all()}して、@<code>{previous_sequence_number}に完了したタスクの通し番号をコピーし、@<code>{for}ループの先頭に戻ります。
ここまで部分は@<img>{work_gang_do_task_4}と対応しています。

これでワーカーのタスク実行は終了です。
すべてのワーカーのタスク実行が完了すると、クライアントは@<code>{GangWorker}の情報を見て完了を検知し、@<code>{run_task()}の実行が完了します。
