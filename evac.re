= 退避

本章ではG1GCの退避の実装を解説します。
この章でも『アルゴリズム編』ですでに紹介した内容は省略していきます。

== 退避の全体像
退避の全体像をおさらいしていきましょう。

=== 実行ステップ
退避は大まかに分けて次の3ステップにわかれています。

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

== ステップ2―ルート退避

== ステップ3―退避
