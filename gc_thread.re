= GCスレッド

本章ではGCに利用されるスレッドのクラスについて説明します。

== 並列GC

並列GCは複数のスレッドで同時にGCを実行します。
この節では並列GCがどのように実装されているか見ていきます。

=== 並列実行の流れ

HotspotVMには複数のスレッドで並列に「何かのタスク」を実行する仕組みが実装されています。
それを構成する主な登場人物は次のとおりです。

  * @<code>{AbstractWorkGang} - ワーカーの集団
  * @<code>{AbstractGangTask} - ワーカーに実行させるタスク
  * @<code>{GangWorker} - 与えられたタスクを実行するワーカー

上記の登場人物が並列にタスクを実行する一連の流れをこれから示します。

まず、@<code>{AbstractWorkGang}は1つだけモニタを持っており、モニタの待合室には@<code>{AbstractWorkGang}に所属する@<code>{GangWorker}を待たせいます（@<img>{work_gang_do_task_1}）。

//image[work_gang_do_task_1][1. @<code>{AbstractWorkGang}はモニタを1つだけ持ち、@<code>{GangWorker}を待合室に待たせている。]

@<code>{AbstractWorkGang}は1つだけモニタを持っており、モニタの待合室には@<code>{AbstractWorkGang}に所属する@<code>{GangWorker}を待たせておきます。
モニタが排他制御する共有リソースはタスク情報の掲示板です。
掲示板には以下の情報が書きこまれます。

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

次に、タスクの実行が終わるとワーカーは再度モニタに入り、タスクが終了したことを伝えます（@<img>{work_gang_do_task_4}）。

//image[work_gang_do_task_4][4. タスクを終えたワーカーは再度モニタに入り、掲示板の情報を書き換えた後で待合室に入る。]

ワーカーはモニタに入り、掲示板の実行完了ワーカー総数を@<code>{+1}します。
そして、待合室の全員を呼び出し（クライアントを含む）、自分は待合室に入ります。
ワーカーのタスク実行がすべて終了すると、実行ワーカー総数は実行完了ワーカー総数と同じになります。

クライアントはモニタに入ると、掲示板ですべてのワーカーが終了したか確認します（@<img>{work_gang_do_task_5}）。

//image[work_gang_do_task_5][5. クライアントはモニタに入った後、すべてのワーカーがタスクを終えたことを確認し、モニタを出ていく。]

もし、終えていないタスクがあれば、待合室でワーカーがタスクを終えるまで待ちます。
すべてのワーカーがタスクを終えていれば、クライアントは満足してモニタから出て行きます。

以上が並列実行の流れです。

=== AbstractWorkGangクラス
ここからはそれぞれの登場人物の詳細を述べています。

@<code>{AbstractWorkGang}クラスの継承関係を@<img>{abstract_worker_gang_hierarchy}に示します。

//image[abstract_worker_gang_hierarchy][@<code>{AbstractWorkGang}クラスの継承関係]

@<code>{AbstractWorkGang}クラスは@<code>{WorkGang}に必要なインタフェースを定義するクラスです。
モニタ・所属するワーカー等の@<code>{WorkGang}に必要な属性を初期化・解放する処理も実装されています。

@<code>{WorkGang}クラスには実際にタスクを受け取って処理を@<code>{worker}に渡す処理が定義されています。

@<code>{FlexibleWorkGang}クラスは実行可能なワーカーを後から柔軟に（Flexible）変更できる機能を持つクラスです。
並列GCにはこの@<code>{FlexibleWorkGang}クラスがよく利用されます。

=== AbstractGangTaskクラス

@<code>{AbstractGangTask}クラスの継承関係を@<img>{abstract_gang_task_hierarchy}に示します。

//image[abstract_gang_task_hierarchy][@<code>{AbstractGangTask}クラスの継承関係]

@<code>{AbstractGangTask}は並列実行するタスクとして必要なインタフェースの仮想関数を定義しています。

//source[share/vm/utilities/workgroup.hpp]{
64: class AbstractGangTask VALUE_OBJ_CLASS_SPEC {
65: public:

68:   virtual void work(int i) = 0;
//}

もっとも重要なメンバ関数は68行目の@<code>{work()}です。
@<code>{work()}はワーカーの識別番号（連番）を受け取り、タスクを実行する関数です。

タスクの詳細な処理は、@<code>{G1ParTask}のようなそれぞれの子クラスで@<code>{work()}として定義します。
クライアントは子クラスのインスタンスを@<code>{AbstractWorkGang}に渡して、並列実行してもらうわけです。

=== GangWorkerクラス
@<code>{GangWorker}クラスはタスクを実際に実行するクラスで、@<code>{Thread}クラスを祖先に持ちます。

//image[gang_worker_hierarchy][@<code>{GangWorker}クラスの継承関係]

そのため、@<code>{GangWorker}のインスタンスは1つのスレッドと対応しています。

== 並行GC
  * ConcurrentGCThread

== セーフポイント
  * XXOperationの説明
