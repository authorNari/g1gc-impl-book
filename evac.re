= 退避

本章ではG1GCの退避の実装を解説します。
この章でも『アルゴリズム編』ですでに紹介した内容は省略していきます。

== 退避の全体像
退避の全体像をおさらいしていきましょう。

=== 実行ステップ
退避はおおまかに次の3ステップにわかれています。

 1. 回収集合選択
 2. ルート退避
 3. 退避

1.は退避対象のリージョンを選択するステップです。
並行マーキングで得た情報を参考に回収集合を選びます。

2.は回収集合内のルートから直接参照されているオブジェクトと、他リージョンから参照されているオブジェクトを空リージョンに退避するステップです。

3.は退避されたオブジェクトを起点として子オブジェクトを退避するステップです。
3. が終了した段階で、回収集合内にある生存オブジェクトはすべて退避済みとなります。

また退避は必ずセーフポイントで実行されます。
そのため、退避中はミューテータは停止した状態になっています。

=== 退避の実行タイミング
//comment[TODO: 『アルゴリズム編 5.8』]
VMが退避を実行する理由は「アロケーション時に空き領域が足りなくなった」というケースがほとんどです。
オブジェクトをVMヒープからアロケーションする際に空き領域が不足した場合、VMヒープを拡張するまえに退避を実行して空きを作り、そこに対してオブジェクトを割り当てます。

では、実際のソースコードを見てみましょう。
@<hd>{alloc|オブジェクトのアロケーション|G1GCのVMヒープへメモリ割り当て}で少し紹介した@<code>{attempt_allocation_slow()}メンバ関数の中で退避を呼び出しています。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
886: HeapWord* G1CollectedHeap::attempt_allocation_slow(size_t word_size,
887:                            unsigned int *gc_count_before_ret) {

906:     {
907:       MutexLockerEx x(Heap_lock);

           /* 省略: 空きリージョンの取得＆オブジェクト割り当て */

           /* 成功したらreturnする */
911:       if (result != NULL) {
912:         return result;
913:       }

933:     }

937:       result = do_collection_pause(word_size, gc_count_before, &succeeded);
//}

906行目から933行目に掛けて、新しい空きリージョンを確保しようとします。
成功した場合は912行目で@<code>{return}しますが、失敗した場合は937行目で@<code>{do_collection_pause()}を呼びます。

@<code>{do_collection_pause()}は以下のようにVMオペレーションを実行します。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
3025: HeapWord* G1CollectedHeap::do_collection_pause(size_t word_size,
3026:                                                unsigned int gc_count_before,
3027:                                                bool* succeeded) {

3030:   VM_G1IncCollectionPause op(gc_count_before,
3031:                              word_size,
3032:                              false,
3033:                              g1_policy()->max_pause_time_ms(),
3034:                              GCCause::_g1_inc_collection_pause);
3035:   VMThread::execute(&op);
3036: 
3037:   HeapWord* result = op.result();

3044:   return result;
3045: }
//}

そして、@<code>{VM_G1IncCollectionPause}の@<code>{doit()}で退避が実行されます。

//source[share/vm/gc_implementation/g1/vm_operations_g1.cpp]{
72: void VM_G1IncCollectionPause::doit() {
73:   G1CollectedHeap* g1h = G1CollectedHeap::heap();

104:   _pause_succeeded =
105:     g1h->do_collection_pause_at_safepoint(_target_pause_time_ms);
106:   if (_pause_succeeded && _word_size > 0) {

           /* 省略:新しくできた空き領域にメモリ割り当て */

112:   }
113: }
//}

105行目の@<code>{do_collection_pause_at_safepoint()}で退避は実行されます
成功すれば退避で空いた領域にメモリを割り当てて、@<code>{_result}メンバ変数にポインタを格納してVMオペレーションを終了します。

=== do_collection_pause_at_safepoint()
@<code>{do_collection_pause_at_safepoint()}の説明に必要な部分を抜き出すと次のようになります。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
3188: bool
3189: G1CollectedHeap::do_collection_pause_at_safepoint(
        double target_pause_time_ms) {

            /* 1. 回収集合選択 */
3336:       g1_policy()->choose_collection_set(target_pause_time_ms);

            /* 2.,3. 退避 */
3362:       evacuate_collection_set();

3494:   return true;
3495: }
//}

@<code>{do_collection_pause_at_safepoint()}は引数にGC停止時間の上限を受け取ります。
3336行目の@<code>{choose_collection_set()}は回収集合を選択するメンバ関数です。
この関数では、引数に受け取るGC停止時間上限を超えないような回収集合を選択します。

3362行目の@<code>{evacuate_collection_set()}で選択した回収集合の生存オブジェクトを退避していきます。

=== 退避用記憶集合維持スレッド
『アルゴリズム編 3.4』で紹介した退避用記憶集合維持スレッドは@<code>{ConcurrentG1RefineThread}というクラスに実装されています。

@<code>{ConcurrentG1RefineThread}クラスのインスタンスは@<code>{ConcurrentG1Refine}クラスのコンストラクタで生成されます。

//source[share/vm/gc_implementation/g1/concurrentG1Refine.cpp]{
48: ConcurrentG1Refine::ConcurrentG1Refine() :
60: {

77:   _n_worker_threads = thread_num();
79:   _n_threads = _n_worker_threads + 1;
80:   reset_threshold_step();
81: 
82:   _threads = NEW_C_HEAP_ARRAY(ConcurrentG1RefineThread*, _n_threads);
83:   int worker_id_offset = (int)DirtyCardQueueSet::num_par_ids();
84:   ConcurrentG1RefineThread *next = NULL;
85:   for (int i = _n_threads - 1; i >= 0; i--) {
86:     ConcurrentG1RefineThread* t =
           new ConcurrentG1RefineThread(this, next, worker_id_offset, i);
89:     _threads[i] = t;
90:     next = t;
91:   }
92: }
//}

77行目の@<code>{thread_num()}で生成する@<code>{ConcurrentG1RefineThread}インスタンスの数が決まります。
この@<code>{thread_num()}の値は@<code>{G1ConcRefinementThreads}というユーザが起動オプションに指定できる値でも変更可能です。

@<code>{ConcurrentG1RefineThread}クラスはインスタンスを作った時点でスレッドの生成・起動をおこないます。
86行目でインスタンスを生成すると同時に、退避用記憶集合維持スレッドは動き出します。

@<code>{ConcurrentG1Refine}のコンストラクタは@<code>{G1CollectedHeap}の@<code>{initialize()}で呼び出されます。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
1794: jint G1CollectedHeap::initialize() {

1816:   _cg1r = new ConcurrentG1Refine();
//}

そのため、VMヒープを生成するタイミングで退避用記憶集合維持スレッドも動きはじめることになります。

== ステップ1―回収集合選択
最初のステップである回収集合選択からみていきましょう。
回収集合選択は@<code>{G1CollectorPolicy_BestRegionsFirst}クラスの@<code>{choose_collection_set()}に実装されています。

回収集合選択で利用される予測時間の計算はほとんど『アルゴリズム編 4.2 退避時間の予測』で紹介した内容ですので、計算内容は省略します。

=== 新世代リージョン選択
『アルゴリズム編 5.1』の中で説明したとおり、すべての新世代リージョンは回収集合に選択されます。

//source[share/vm/gc_implementation/g1/g1CollectorPolicy.cpp:choose_collection_set():前半]{
2853: void
2854: G1CollectorPolicy_BestRegionsFirst::choose_collection_set(
2855:                              double target_pause_time_ms) {

2866:   double base_time_ms = predict_base_elapsed_time_ms(_pending_cards);
2867:   double predicted_pause_time_ms = base_time_ms;
2869:   double time_remaining_ms = target_pause_time_ms - base_time_ms;

2897:   if (in_young_gc_mode()) {

          /* 新世代リージョンを回収集合へ */
2932:     _collection_set = _inc_cset_head;
2933:     _collection_set_size = _inc_cset_size;
2934:     _collection_set_bytes_used_before = _inc_cset_bytes_used_before;

2940:     time_remaining_ms -= _inc_cset_predicted_elapsed_time_ms;
2941:     predicted_pause_time_ms += _inc_cset_predicted_elapsed_time_ms;

2974:   }

//}

2866行目の@<code>{predict_base_elapsed_time_ms()}はリージョンを退避する処理以外の部分の停止時間を予測します。
内部では過去の停止予測時間と実際の停止時間を保持しており、実行を繰り返すたびに予測時間の精度が上がるようになっています。
2867行目の@<code>{predicted_pause_time_ms}が予測停止時間を保持するローカル変数で、2869行目の@<code>{time_remaining_ms}が残りの停止可能時間です。

2897行目の@<code>{in_young_gc_mode()}はG1GCが世代別方式であるかを示すフラグを返します。
G1GCは必ず世代別G1GC方式で動作しますので、@<code>{in_young_gc_mode()}は@<code>{true}を必ず返します。

2932〜2934行目に掛けて新世代リージョンを回収集合に設定しています。
@<code>{_collection_set}メンバ変数が回収集合を表すものです。

2940〜2941行目であらかじめ計算していた停止予測時間を使って、@<code>{predicted_pause_time_ms}と@<code>{time_remaining_ms}を計算します。
『アルゴリズム編 5.6 新世代リージョン数上限決定』で説明したとおり、過去の実行履歴でこの停止予測時間は算出されます。

=== 旧世代リージョン選択
部分的新世代GCの場合は旧世代のリージョンも回収集合に追加します。

//source[share/vm/gc_implementation/g1/g1CollectorPolicy.cpp:choose_collection_set():後半]{
2976:   if (!in_young_gc_mode() || !full_young_gcs()) {

2981:     do {
2982:       hr = _collectionSetChooser->getNextMarkedRegion(time_remaining_ms,
2983:                                                       avg_prediction);
2984:       if (hr != NULL) {
2985:         double predicted_time_ms = predict_region_elapsed_time_ms(hr, false);
2986:         time_remaining_ms -= predicted_time_ms;
2987:         predicted_pause_time_ms += predicted_time_ms;
2988:         add_to_collection_set(hr);

              /* 省略: should_continueの値設定 */
2997:       }
3002:     } while (should_continue);

3007:   }

3019: }
//}

2976行目の@<code>{full_young_gc()}は全新世代GCを示すフラグを返すメンバ関数です。
もし@<code>{false}を返す場合は、部分的新世代GCのモードにあることを示します。

2982行目で残りの停止可能時間内で追加可能なリージョンを取得します。
もし存在しなければ@<code>{NULL}を返します。

存在する場合、2985行目の@<code>{predict_region_elapsed_time_ms()}で予測停止時間を算出して、2988行目で回収集合に追加します。
最終的に@<code>{time_remaining_ms}がマイナスになった場合は@<code>{should_continue}を@<code>{false}にして、whileループを抜けます。

== ステップ2―ルート退避
ルート退避はルートから直接参照可能な回収集合内のオブジェクトを別の空きリージョンに退避するステップです。

=== evacuate_collection_set()
@<hd>{退避の全体像|do_collection_pause_at_safepoint()}で説明したとおり、退避は@<code>{evacuate_collection_set()}で実行されます。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
4785: void G1CollectedHeap::evacuate_collection_set() {

4792:   int n_workers = (ParallelGCThreads > 0 ? workers()->total_workers() : 1);
4793:   set_par_threads(n_workers);
4794:   G1ParTask g1_par_task(this, n_workers, _task_queues);

4802:   if (G1CollectedHeap::use_parallel_gc_threads()) {
4806:     workers()->run_task(&g1_par_task);
4807:   } else {
4809:     g1_par_task.work(0);
4810:   }

//}

@<code>{evacuate_collection_set()}では@<hd>{mark|ステップ1―初期マークフェーズ|G1GCのルートスキャン}で登場した@<code>{G1ParTask}を生成し、（可能な場合は並列で）実行します。

@<code>{G1ParTask}はすでに説明した通り、@<code>{process_strong_roots()}を使ってすべてのルートを走査し、G1GCの場合は@<code>{G1ParScanAndMarkExtRootClosure}クラスの@<code>{do_oop()}を適用します。
この@<code>{do_oop()}にオブジェクトを退避する処理が実装されています。

=== オブジェクト退避
@<code>{do_oop()}は最終的に@<code>{G1ParCopyHelper}クラスの@<code>{copy_to_survivor_space()}を呼び出し、オブジェクトを空きリージョンに退避します。
この中の処理は『アルゴリズム編 3.8.1 オブジェクト退避』で詳細に述べてたため省略します。

=== コピー関数
とはいえ、全部見ないもの何か味気ないものですので、ここからは著者が読んでいて面白かった「オブジェクトのコピーに使う関数」を紹介しておきます。

以下はG1GCでオブジェクトをコピーする部分だけ抜き出したものです。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
4397:     Copy::aligned_disjoint_words((HeapWord*) old, obj_ptr, word_sz);
//}

@<code>{aligned_disjoint_words}は@<code>{Copy}クラスの静的メンバ関数です。

//source[share/vm/utilities/copy.hpp]{
114:   static void aligned_disjoint_words(HeapWord* from,
                                          HeapWord* to, size_t count) {
115:     assert_params_aligned(from, to);
116:     assert_disjoint(from, to, count);
117:     pd_aligned_disjoint_words(from, to, count);
118:   }
//}

引数には@<code>{from}、@<code>{to}、そしてコピーするデータのワード数を引数に取ります。
115行目の@<code>{assert_params_aligned()}は@<code>{from}と@<code>{to}がアラインメントされた値かをチェックします。
116行目は@<code>{from}から@<code>{to}へ、コピーする際にメモリ領域が重ならないことをチェックします。
117行目の@<code>{pd_aligned_disjoint_words()}は各OSごとに定義されている静的メンバ関数です。

ここではOSがLinuxでCPUがx86のものに対する@<code>{pd_aligned_disjoint_words()}を見てみます。

//source[os_cpu/linux_x86/vm/copy_linux_x86.inline.hpp]{
73:  static void pd_disjoint_words(HeapWord* from, HeapWord* to, size_t count) {
74:  #ifdef AMD64
75:    switch (count) {
76:    case 8:  to[7] = from[7];
77:    case 7:  to[6] = from[6];
78:    case 6:  to[5] = from[5];
79:    case 5:  to[4] = from[4];
80:    case 4:  to[3] = from[3];
81:    case 3:  to[2] = from[2];
82:    case 2:  to[1] = from[1];
83:    case 1:  to[0] = from[0];
84:    case 0:  break;
85:    default:
86:      (void)memcpy(to, from, count * HeapWordSize);
87:      break;
88:    }
89:  #else
         /* 省略: その他 */
108: #endif // AMD64
109: }

139: static void pd_aligned_disjoint_words(HeapWord* from,
                                           HeapWord* to, size_t count) {
140:   pd_disjoint_words(from, to, count);
141: }
//}

@<code>{pd_aligned_disjoint_words()}は@<code>{pd_disjoint_words()}をそのまま呼び出します。

@<code>{pd_disjoint_words()}はCPUがAMD64の場合と、そうでない場合で処理が異なります。
まず、AMD64だった場合を見てみましょう。
8ワード以下の場合、76〜84行目までのcase文に当てはまり、@<code>{=}演算子によるメモリコピーがおこなわれます。
それ以外は@<code>{memcpy()}を呼び出しています。
たぶん、コピーするデータがあまりにも小さいと関数呼び出しのコストの方がバカにならないので@<code>{memcpy()}の呼び出しをケチっているのでしょう。

//source[os_cpu/linux_x86/vm/copy_linux_x86.inline.hpp]{
73:   static void pd_disjoint_words(HeapWord* from, HeapWord* to, size_t count) {
74:   #ifdef AMD64
          /* 省略 */
89:   #else
91:    intx temp;
92:    __asm__ volatile("        testl   %6,%6       ;"
93:                     "        jz      3f          ;"
94:                     "        cmpl    $32,%6      ;"
95:                     "        ja      2f          ;"
96:                     "        subl    %4,%1       ;"
97:                     "1:      movl    (%4),%3     ;"
98:                     "        movl    %7,(%5,%4,1);"
99:                     "        addl    $4,%0       ;"
100:                    "        subl    $1,%2        ;"
101:                    "        jnz     1b          ;"
102:                    "        jmp     3f          ;"
103:                    "2:      rep;    smovl       ;"
104:                    "3:      nop                  "
105:                    : "=S" (from), "=D" (to), "=c" (count), "=r" (temp)
106:                    : "0"  (from), "1"  (to), "2"  (count), "3"  (temp)
107:                    : "memory", "cc");
108: #endif // AMD64
109: }
//}

AMD64以外はインラインアセンブラを使ってコピーを自前で実装しています。
簡単に概要だけ説明します。

92〜93行目は@<code>{count}が@<code>{0}でないことをチェックする処理です。
92行目の@<code>{testl}は@<code>{count}のANDをとっています。
もし@<code>{count}が0の場合は93行目の@<code>{jz}で104行目にジャンプします。

94〜95行目は@<code>{count}が@<code>{32}以下であることチェックする処理です。
もし@<code>{count}が@<code>{32}より大きければ、95行目の@<code>{ja}は103行目にジャンプします。

103行目は@<code>{rep}を使ったストリングス命令@<code>{smovl}の実行です。
@<code>{count}の分だけ@<code>{smovl}を繰り返し、@<code>{from}から@<code>{to}へデータをコピーします。

96〜102行目はジャンプを使ったループでコピーする処理です。
@<code>{count}が32以下であればこっちを使います。
96行目は@<code>{from}と@<code>{to}をオフセットを求め、@<code>{to}のレジスタに格納します。
97行目で@<code>{from}の1ワード分のデータを@<code>{temp}のレジスタに格納。
98行目で@<code>{temp}のデータを@<code>{from + オフセット}の位置（最初は@<code>{to}の先頭）へコピー。
99行目でオフセット加算。
100行目で@<code>{count}を@<code>{1}減らす。
101行目で@<code>{count}が@<code>{0}になってないか確認。
@<code>{0}じゃければ97行目にジャンプ。
もし0であれば103行目の@<code>{jmp}で104行目にジャンプ。

と、こんな感じです。
はたしてこれは@<code>{memcpy()}よりも速いのでしょうか。
卜部さんのかかれた以下の記事では（これは@<code>{memset64}についてですが）興味深い実験結果と考察が書かれています。
ぜひ読んでみてください。

 * @<href>{http://shyouhei.tumblr.com/post/2988488168/memset64-char-word, 卜部昌平のあまりreblogしないtumblr - 最速の memset64 を求めて}

@<code>{memset64}の用途では上記の結果になりますが、@<code>{memcpy()}の場合はどうなんでしょうね。
いろいろと変わってきそうです。

インラインアセンブラの読み方に関しては以下の記事を参考にしました。

 * @<href>{http://d.hatena.ne.jp/wocota/20090628/1246188338, GCCのインラインアセンブラの書き方 for x86 - OSのようなもの}

== ステップ3―退避
ルート退避で退避したオブジェクトのフィールドは退避キューに保持されています。
このステップではその退避キュー内に保持したフィールドが参照している子オブジェクトを次々に退避していきます。

@<code>{G1ParTask}の@<code>{work()}では、ルート退避完了後に@<code>{G1ParEvacuateFollowersClosure}の@<code>{do_void()}を呼び出します。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
4620:   void work(int i) {

          /* 省略: ルート退避 */

4666:     {

4668:       G1ParEvacuateFollowersClosure evac(_g1h, &pss, _queues, &_terminator);
4669:       evac.do_void();

4674:     }
//}

@<code>{do_void()}の中では退避キューが保持するオブジェクトを次々に退避していきます。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
4565: void G1ParEvacuateFollowersClosure::do_void() {
4566:   StarTask stolen_task;
4567:   G1ParScanThreadState* const pss = par_scan_state();
4568:   pss->trim_queue();

        /* 省略: タスクスティーリング */

4587: }
//}

4567行目の@<code>{G1ParScanThreadState}のインスタンス内部で退避キューを保持しています。
この退避キューはスレッドローカルなもので、他の退避スレッドと競合しません。
4568行目の@<code>{trim_queue()}で退避キューが空になるまでオブジェクト退避を続けます。

他のスレッドが退避対象が多すぎた場合は、タスクスティーリングを使って仕事量が偏り過ぎないように調整します。
