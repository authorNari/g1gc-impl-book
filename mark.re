= 並行マーキング

本章ではG1GCの並行マーキングの実装を解説していきます。
アルゴリズム編で紹介した部分の実装をなぞってもあまり面白くないので、アルゴリズム編であまり触れていない内容を特に取り上げながら説明していきます。
ですので、本章を読む前に『アルゴリズム編 2.並行マーキング』を軽く読むことをオススメします。

== 並行マーキングの全体像

まずは、並行マーキングの全体像を把握していきましょう。

=== 実行ステップ
『アルゴリズム編』の振り返りにはなりますが、はじめに並行マーキングの実行ステップについて簡単に説明しておきます。

並行マーキングは大まかに分けて次の5つのステップに別れています。

 1. 初期マークフェーズ
 2. 並行マークフェーズ
 3. 最終マークフェーズ
 4. 生存オブジェクトカウント
 5. 後始末

1.はルートスキャンをおこなうステップです。
このステップはセーフポイントで実行されます。

2.は1.によってマークされたオブジェクトをスキャンするステップです。
このステップはミューテータと並行、かつ複数のスレッドで並列に実行されます。

3.は2.でマークしきれなかったオブジェクトをスキャンするステップです。
このステップもセーフポイントで実行され、かつ複数のスレッドで並列に実行されます。

4.は各リージョンのマークが付いているオブジェクトのバイト数をカウントするステップです。
このステップはミューテータと並行、かつ複数のスレッドで並列に実行されます。

5.はマークフェーズの後始末をして次回のマークフェーズに備えるステップです。
このステップもセーフポイントで実行され、かつ複数のスレッドで並列に実行されます。

並行マーキングは上記の5つのステップを1サイクルとして、必要なときに実行しています。

=== ConcurrentMarkクラス
並行マーキングの各処理は@<code>{ConcurrentMark}というクラスに実装されています。
@<code>{ConcurrentMark}クラスの定義を簡単に見てみましょう。

//source[share/vm/gc_implementation/g1/concurrentMark.hpp]{
359: class ConcurrentMark: public CHeapObj {

375:   ConcurrentMarkThread* _cmThread;
376:   G1CollectedHeap*      _g1h;
377:   size_t                _parallel_marking_threads;

392:   CMBitMap                _markBitMap1;
393:   CMBitMap                _markBitMap2;
394:   CMBitMapRO*             _prevMarkBitMap;
395:   CMBitMap*               _nextMarkBitMap;
//}

375行目の@<code>{_cmThread}には並行マーキングスレッドを保持し、376行目の@<code>{_g1h}はG1GC用のVMヒープを保持します。

377行目の@<code>{_parallel_marking_threads}には並列マーキングで使用するスレッド数が格納されます。

392・393行目にはVMヒープに対応したビットマップの実体が割り当てられます。
@<code>{CMBitMap}クラスについては@<hd>{ステップ1―初期マークフェーズ|ビットマップの並列化対応}で詳しく説明します。

394行目の@<code>{_prevMarkBitMap}は@<code>{_markBitMap1}、もしくは@<code>{_markBitMap2}のいずれかを指しています。
395行目の@<code>{_nextMarkBitMap}も同じです。
そして、@<code>{_prevMarkBitMap}が指す方がVMヒープ全体の@<code>{prev}ビットマップ、@<code>{_nextMarkBitMap}が指す方が@<code>{next}ビットマップになります。

=== ConcurrentMarkThreadクラス
並行マーキングスレッドは@<code>{ConcurrentMarkThread}クラスに実装されています。
このクラスは@<code>{CuncurrentGCThread}クラスを親に持っており、インスタンスを作った段階でスレッドが起動します。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.hpp]{
36: class ConcurrentMarkThread: public ConcurrentGCThread {

49:   ConcurrentMark*                  _cm;
50:   volatile bool                    _started;
51:   volatile bool                    _in_progress;
//}

@<code>{ConcurrentMarkThread}は49行目にあるように、メンバ変数として@<code>{ConcurrentMark}を持ちます。

50行目の@<code>{_started}は並行マーキングに対して実行開始要求があるかを示すフラグです。
51行目の@<code>{_in_progress}は並行マーキングが実際に実行中であるかを示すフラグです。

=== 並行マーキングの実行開始
並行マーキングスレッドは以下のように、起動してすぐに@<code>{sleepBeforeNextCycle()}を呼び出します。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.cpp]{
93: void ConcurrentMarkThread::run() {

103:   while (!_should_terminate) {

105:     sleepBeforeNextCycle();
//}

@<code>{sleepBeforeNextCycle()}は名前の通り次回のサイクルの前まで待ち状態にするメンバ関数です。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.cpp]{
329: void ConcurrentMarkThread::sleepBeforeNextCycle() {

334:   MutexLockerEx x(CGC_lock, Mutex::_no_safepoint_check_flag);
335:   while (!started()) {
336:     CGC_lock->wait(Mutex::_no_safepoint_check_flag);
337:   }
338:   set_in_progress();
339:   clear_started();
340: }
//}

334行目で@<code>{CGC_lock}というグローバルなミューテータをロックし、336行目で待ち状態になります。
もし、並行マーキングスレッドの待ち状態が解かれると、338行目で@<code>{_in_progress}を@<code>{true}にし、339行目で@<code>{_started}を@<code>{false}にします。

さて、並行マーキングに次のサイクルを実行させたければ、@<code>{_started}を@<code>{true}にして、@<code>{CGC_lock}に@<code>{notify()}しないといけません。
それをやるのが@<code>{G1CollectedHeap}の@<code>{doConcurrentMark()}です。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
3047: void
3048: G1CollectedHeap::doConcurrentMark() {
3049:   MutexLockerEx x(CGC_lock, Mutex::_no_safepoint_check_flag);
3050:   if (!_cmThread->in_progress()) {
3051:     _cmThread->set_started();
3052:     CGC_lock->notify();
3053:   }
3054: }
//}

3051行目で@<code>{_started}を@<code>{true}にし、すぐ後に@<code>{CGC_lock}に対して@<code>{notify()}しています。
また、3050行目のif文によって、並行マーキングの実行中に@<code>{set_started()}が呼ばれることはありません。

並行マーキングの実行開始タイミングは退避の直後のみです。
そのため、@<code>{doConcurrentMark()}の呼び出しは「退避の直後」にしかありません。

=== 並行マーキングのサイクル
@<code>{ConcurrentMarkThread}の@<code>{run()}の@<code>{while}ループに並行マーキングの1サイクルが実装されています。
@<code>{run()}は200行以上もある大きな関数であるため、説明に必要な部分だけを抜き出しました。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.cpp]{
93: void ConcurrentMarkThread::run() {

103:   while (!_should_terminate) {

105:     sleepBeforeNextCycle();
106:     {

             /* 2. 並行マークフェーズ */
143:         if (!cm()->has_aborted()) {
144:           _cm->markFromRoots();
145:         }

             /* 3. 最終マークフェーズ */
150:         if (!cm()->has_aborted()) {

165:           CMCheckpointRootsFinalClosure final_cl(_cm);
166:           sprintf(verbose_str, "GC remark");
167:           VM_CGC_Operation op(&final_cl, verbose_str);
168:           VMThread::execute(&op);
169:         }

           /* 4. 生存オブジェクトカウント */
190:       if (!cm()->has_aborted()) {

198:         _sts.join();
199:         _cm->calcDesiredRegions();
200:         _sts.leave();

211:       }

           /* 5. 後始末 */
218:       if (!cm()->has_aborted()) {

226:         CMCleanUp cl_cl(_cm);
227:         sprintf(verbose_str, "GC cleanup");
228:         VM_CGC_Operation op(&cl_cl, verbose_str);
229:         VMThread::execute(&op);
230:       }

           /* nextビットマップクリア */
287:       _sts.join();
288:       _cm->clearNextBitmap();
289:       _sts.leave();
290:     }

299:   }

302:   terminate();
303: }
//}

上記の中にステップ1の初期マークフェーズはありません。
なぜかというと、初期マークフェーズは実は退避と一緒にやってしまう処理だからです。
退避でもオブジェクトコピーのためにルートスキャンしないといけません。
同じ処理を並行マーキングでやるのも無駄ですので、退避のルートスキャンと一緒にマークも付けてしまいます。

143〜145行目がステップ2の並行マーキングフェーズにあたる部分です。
144行目の@<code>{markFromRoots()}が初期マークフェーズでマークしたオブジェクトをスキャンするメンバ関数になります。

150〜169行目がステップ3の最終マークフェーズです。
VMオペレーションを使って処理を実行します。

190〜211行目がステップ4の生存オブジェクトカウントです。
こちらは@<code>{SuspendableThreadSet}を使っていることがわかります。

218〜230行目までがステップ5の後始末です。
こちらもVMオペレーションを使っています。
内部で@<code>{prev}ビットマップと@<code>{next}ビットマップをスワップします。

287〜289行目までは@<code>{next}ビットマップをクリアする処理です。
次回の並行マーキングに利用するビットマップの準備ですね。

各ステップのif文の条件である@<code>{has_aborted()}は、並行マーキングが何らかの理由で中断したい場合に@<code>{true}を返します。
abortになるほとんどの理由は並行マーキングのサイクル実行中に退避が発生することです。
退避がおこるとオブジェクト自体が移動するため、ビットマップのマークを付け直さないといけません。
そのため、中断時には各ステップをスキップし、ビットマップをクリアする処理のみが実行されます。

== ステップ1―初期マークフェーズ
初期マークフェーズはルートから直接参照可能なオブジェクトにマークを付ける処理です。
このステップは前節で述べたとおり、退避のルートスキャンと同じタイミングで実施されます。
退避については TODO:退避の章 で後述しますので、ここではマーキングに関連する部分のみを取り上げます。

=== ルート
HotspotVMでGCのルートとなるものを大まかにリストアップしました。

 * 各スレッド固有の情報（スタックフレームなど）
 * 組み込みクラス
 * JNIのハンドラ
 * パーマネント領域から他の領域に対する参照
 * 退避用記憶集合
 * etc...

初期マークフェーズでは上記をルートとして処理を進めます。

=== ルートスキャンの枠組み
HotspotVMにはルート走査をおこなう@<code>{process_strong_roots()}メンバ関数が@<code>{SharedHeap}クラスに準備されています。

//source[share/vm/memory/sharedHeap.hpp]{
219:   void process_strong_roots(bool activate_scope,
220:                             bool collecting_perm_gen,
221:                             ScanningOption so,
222:                             OopClosure* roots,
223:                             CodeBlobClosure* code_roots,
224:                             OopsInGenClosure* perm_blk);
//}

このメソッドの説明すべき役割は以下の2つです。

 1. ルートを引数に@<code>{roots}の@<code>{do_oop()}を呼び出す
 2. 複数のスレッドで処理する場合はタスクを分割する

まず、1.から説明していきましょう。
@<code>{OopClosure}クラスはルートのイテレーションに利用されるクラスです。

//source[share/vm/memory/iterator.hpp]{
56: class OopClosure : public Closure {

61:   virtual void do_oop(oop* o) = 0;
//}

クラスには@<code>{do_oop()}という仮想関数が定義されています。
この@<code>{do_oop()}はHotspotVM上のさまざまなルートを引数にして呼び出されます。
@<code>{do_oop()}の実体は@<code>{OopClosure}のサブクラスで実装します。

では、実際にどのように@<code>{process_strong_roots()}で利用されているか見てみましょう。

//source[share/vm/memory/sharedHeap.cpp]{
138: void SharedHeap::process_strong_roots(bool activate_scope,
                                           ...) {
148:     Universe::oops_do(roots);
149:     ReferenceProcessor::oops_do(roots);

155:     JNIHandles::oops_do(roots);

158:     Threads::possibly_parallel_oops_do(roots, code_roots);

163:     ObjectSynchronizer::oops_do(roots);

165:     FlatProfiler::oops_do(roots);

167:     Management::oops_do(roots);

169:     JvmtiExport::oops_do(roots);

         /* ... 以下略 ... */
//}

各クラスの@<code>{oops_do()}という静的メンバ関数に@<code>{roots}を渡しているのがわかりますね。
@<code>{oops_do()}は自クラスが管理するオブジェクトに対する参照（ルート）に対して、@<code>{do_oop()}を呼び出すものです。

@<img>{roots_iteration}に処理のイメージを示します。

//image[roots_iteration][各クラスが管理するルートに対して、@<code>{OopClosure}のサブクラスに実装された@<code>{do_oop()}を呼び出す]

上記のようにルートをスキャンする枠組みだけが用意されており、実際に何をやるかは呼び出し側で定義できるわけですね。

次に2.ついて説明しましょう。
@<code>{process_strong_roots()}はルートスキャンを適当な大きさのタスクに分割して、各スレッドが早いもの勝ちでそれぞれのタスクを実行させることで並列実行時に性能がでるようにしています。
再度、@<code>{process_strong_roots()}内を見てみましょう。

//source[share/vm/memory/sharedHeap.cpp]{
138: void SharedHeap::process_strong_roots(bool activate_scope,
                                           ...) {
147:   if (!_process_strong_tasks->is_task_claimed(SH_PS_Universe_oops_do)) {
148:     Universe::oops_do(roots);
149:     ReferenceProcessor::oops_do(roots);
151:     perm_gen()->ref_processor()->weak_oops_do(roots);
152:   }
154:   if (!_process_strong_tasks->is_task_claimed(SH_PS_JNIHandles_oops_do))
155:     JNIHandles::oops_do(roots);

162:   if (!_process_strong_tasks->is_task_claimed(
             SH_PS_ObjectSynchronizer_oops_do))
163:     ObjectSynchronizer::oops_do(roots);
164:   if (!_process_strong_tasks->is_task_claimed(SH_PS_FlatProfiler_oops_do))
165:     FlatProfiler::oops_do(roots);
166:   if (!_process_strong_tasks->is_task_claimed(SH_PS_Management_oops_do))
167:     Management::oops_do(roots);
168:   if (!_process_strong_tasks->is_task_claimed(SH_PS_jvmti_oops_do))
169:     JvmtiExport::oops_do(roots);

         /* ... 他のルートスキャン ... */

221:   _process_strong_tasks->all_tasks_completed();
222: }
//}

@<code>{oops_do()}を呼び出す前に、@<code>{_process_strong_tasks}メンバ変数の@<code>{is_task_claimed()}を呼び出しているのがわかります。
@<code>{_process_strong_tasks}は@<code>{SubTasksDone}クラスのインスタンスです。

この@<code>{is_task_claimed()}の役割は、引数に受けっとた識別子に対応するタスクが他スレッドのものになってないか確認することです。
もし、他スレッドのものであれば@<code>{true}を返し、そのタスクは実行しないようにします。
誰のものでなければ、呼び出したスレッドのタスクにしてから@<code>{false}を返します。
@<code>{is_task_claimed()}は内部でCAS命令を使って不可分に実行されるため、複数のスレッドで同時に呼び出されても問題ありません。

上記の仕組みがあるため、@<code>{process_strong_roots()}を並列実行した場合はスレッドがif文の中にあるタスクを早いもの勝ちで実行していきます。

=== G1GCのルートスキャン
では、G1GCのルートスキャンを見ていきましょう。
以下はマーキングに関連する部分だけを抜き出しています。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
4589: class G1ParTask : public AbstractGangTask {

4620:   void work(int i) {

4643:     G1ParScanAndMarkExtRootClosure  scan_mark_root_cl(_g1h, &pss);

4651:       scan_root_cl = &scan_mark_root_cl;

4659:     _g1h->g1_process_strong_roots(/* not collecting perm */ false,
4660:                                   SharedHeap::SO_AllClasses,
4661:                                   scan_root_cl,
4662:                                   &push_heap_rs_cl,
4663:                                   scan_perm_cl,
4664:                                   i);
//}

ルートスキャンは@<code>{G1ParTask}の@<code>{work()}で実行します。
「@<hd>{gc_thread_par|AbstractGangTaskクラス}」で説明したクラスを継承していますね。
つまり、この@<code>{work()}は並列で動作可能です。
この辺の詳しい内容は TODO:退避の章 で後述します。

4643行目で@<code>{G1ParScanAndMarkExtRootClosure}クラスのインスタンスを作っています。
このクラスではルートスキャン時にコピーとマークをおこないます。

4659行目でそのインスタンスを@<code>{g1_process_strong_roots()}の引数に渡します。
このメンバ関数は内部で@<code>{process_strong_roots()}を呼びます。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
4696: void
4697: G1CollectedHeap::
4698: g1_process_strong_roots(bool collecting_perm_gen,
4699:                         SharedHeap::ScanningOption so,
4700:                         OopClosure* scan_non_heap_roots,
4701:                         OopsInHeapRegionClosure* scan_rs,
4702:                         OopsInGenClosure* scan_perm,
4703:                         int worker_i) {

4708:   BufferingOopClosure buf_scan_non_heap_roots(scan_non_heap_roots);

4716:   process_strong_roots(false,
4717:                        collecting_perm_gen, so,
4718:                        &buf_scan_non_heap_roots,
4719:                        &eager_scan_code_roots,
4720:                        &buf_scan_perm);

        /* G1GC特有のルートをスキャン */

4757:   _process_strong_tasks->all_tasks_completed();
4758: }
//}

@<code>{g1_process_strong_roots()}の主な役割はG1GC特有のルートもあわせてスキャンすることにあります。
退避用記憶集合や並行マーキングのマークスタックなどが代表的な例です。

また、4708行目で@<code>{OopClosure}を更に@<code>{BufferingOopClosure}でラップしています。
このクラスの@<code>{do_oop()}は、引数に受け取った@<code>{oop}を一定量バッファに貯めこみ、満タンになったあとに一気に処理します。
これには、ルートを探索するコストとルートをスキャンするコストを分離して計測するという狙いがあります。
詳しくは TODO:計測の章 で後述します。

=== ルートスキャン時のマーク
ルートスキャンで利用する@<code>{G1ParScanAndMarkExtRootClosure}クラスの@<code>{do_oop()}は、最終的に@<code>{ConcurrentMark}の@<code>{grayRoot()}メンバ関数を呼び出します。

//source[share/vm/gc_implementation/g1/concurrentMark.cpp]{
1005: void ConcurrentMark::grayRoot(oop p) {
1006:   HeapWord* addr = (HeapWord*) p;

1015:   if (!_nextMarkBitMap->isMarked(addr))
1016:     _nextMarkBitMap->parMark(addr);
1017: }
//}

@<code>{grayRoot()}の引数@<code>{p}はコピー先のアドレスを受け取ります。
1015行目でマークがついていないことをチェックし、1016行目で@<code>{next}ビットマップにマークします。

上記の@<code>{grayRoot()}がすべてのルートに対して呼び出されれば、初期マークフェーズは終了です。

=== ビットマップの並列化対応
@<code>{next}ビットマップを表す@<code>{CMBitMap}というクラスがあります。
このクラスはコンストラクタで@<code>{BitMap}クラスのインスタンスを生成し、メンバ変数@<code>{_bm}として保持します。
この@<code>{_bm}がビットマップの実体であり、@<code>{CMBitMap}は@<code>{_bm}へのデリゲータでしかありません。

//source[share/vm/gc_implementation/g1/concurrentMark.hpp]{
123: class CMBitMap : public CMBitMapRO {

131:   void mark(HeapWord* addr) {
134:     _bm.at_put(heapWordToOffset(addr), true);
135:   }
136:   void clear(HeapWord* addr) {
139:     _bm.at_put(heapWordToOffset(addr), false);
140:   }
141:   bool parMark(HeapWord* addr) {
144:     return _bm.par_at_put(heapWordToOffset(addr), true);
145:   }
146:   bool parClear(HeapWord* addr) {
149:     return _bm.par_at_put(heapWordToOffset(addr), false);
150:   }
//}

@<code>{CMBitMap}は@<code>{CMBitMapRO}を継承します。
@<code>{CMBitMapRO}の@<code>{RO}はReadOnlyの略で、マークとクリアができないクラスのことです。
@<code>{CMBitMapRO}を継承して、書き込みできるように拡張したのが、上記の@<code>{CMBitMap}クラスです。

@<code>{CMBitMap}は並列対応版とそうでないものの2種類の書き込み方法を持っています。
131〜140行目に定義されている@<code>{mark()}・@<code>{clear()}が並列化に対応していないもので、141行目から定義されている@<code>{parMark()}・@<code>{parClear()}が対応したものです。
そして、初期マークフェーズは並列で実行されることがあるため、@<code>{parMark()}を使うことになります。

144・149行目で呼び出している、@<code>{BitMap}の@<code>{par_at_put()}が処理の実体になりますのでそちらを見ていきましょう。

//source[share/vm/utilities/bitMap.cpp]{
260: bool BitMap::par_at_put(idx_t bit, bool value) {
261:   return value ? par_set_bit(bit) : par_clear_bit(bit);
262: }

53: inline bool BitMap::par_set_bit(idx_t bit) {
55:   volatile idx_t* const addr = word_addr(bit);
56:   const idx_t mask = bit_mask(bit);
57:   idx_t old_val = *addr;
58: 
59:   do {
60:     const idx_t new_val = old_val | mask;
61:     if (new_val == old_val) {
62:       return false;
63:     }
64:     const idx_t cur_val = (idx_t) Atomic::cmpxchg_ptr((void*) new_val,
65:                                                       (volatile void*) addr,
66:                                                       (void*) old_val);
67:     if (cur_val == old_val) {
68:       return true;
69:     }
70:     old_val = cur_val;
71:   } while (true);
72: }
//}

@<code>{par_at_put()}は内部で@<code>{par_set_bit()}か@<code>{par_clear_bit()}を呼び出します。
どちらも似たような処理になるため、@<code>{par_set_bit()}だけ見れば十分でしょう。

引数に受けとった@<code>{bit}はマーク対象オブジェクトのアドレスです。
55行目でアドレスに対応するビットを持つ1ワードの領域へのアドレス（@<code>{addr}）を取得します。
56行目で対象のビットのみが1になった1ワードのビットマスク（@<code>{mask}）を生成します。
57行目で@<code>{addr}の1ワードをローカル変数に持ってきます。

60行目で対象のビットを立てた新しい値（@<code>{new_val}）を生成し、64行目でCAS命令を実行します。
@<code>{cmpxchg_ptr()}は@<code>{*addr}の値と@<code>{old_val}を比較し、同じであれば@<code>{new_val}を書き込み、違えば何もしません。
@<code>{cmpxchg_ptr()}の戻り値は@<code>{*addr}の値になりますので、書き込みが成功したかどうかは67行目のように@<code>{old_val}と比較することで確認できます。
成功していれば@<code>{true}を返し、失敗なら@<code>{old_val}を最新の値に更新して、ループを繰り返します。

このように@<code>{par_at_put()}はデータの不整合が起きないように、CAS命令を利用してビットを書き込むため、複数のスレッドで同時にマークを実行しても問題ないわけです。

== ステップ2―並行マークフェーズ
次のステップは初期マークフェーズでマークされたオブジェクトをミューテータと並行してスキャンしていくステップです。
@<code>{ConcurrentMarkThread}の@<code>{run()}にて、@<code>{markFromRoots()}を呼び出して並行マークフェーズが開始されます。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.cpp:再掲]{
93: void ConcurrentMarkThread::run() {

             /* 2. 並行マークフェーズ */
143:         if (!cm()->has_aborted()) {
144:           _cm->markFromRoots();
145:         }
//}

//source[share/vm/gc_implementation/g1/concurrentMark.cpp]{
1162: void ConcurrentMark::markFromRoots() {

1176:   CMConcurrentMarkingTask markingTask(this, cmThread());
1177:   if (parallel_marking_threads() > 0)
1178:     _parallel_workers->run_task(&markingTask);
1179:   else
1180:     markingTask.work(0);

1182: }
//}

@<code>{ConcurrentMark}の@<code>{markFromRoots()}は@<code>{CMConcurrentMarkingTask}を生成して、そのタスクを1178行目のように並列に実行させます。
もし並列マーキングで使用するスレッド数が0であれば、並列実行をやめます。
どちらにせよ、この処理は並行マークスレッド上の処理になりますので、ミューテータを並行に動きます。

//source[share/vm/gc_implementation/g1/concurrentMark.cpp]{
1089: class CMConcurrentMarkingTask: public AbstractGangTask {

1095:   void work(int worker_i) {

1105:     CMTask* the_task = _cm->task(worker_i);

1113:         the_task->do_marking_step(mark_step_duration_ms,
1114:                                   true /* do_stealing    */,
1115:                                   true /* do_termination */);

1153:   }
//}

タスクの@<code>{work()}では、1105行目で@<code>{ConcurrentMark}にスレッド数分用意しておいた@<code>{CMTask}を取り出し、その@<code>{do_marking_step()}を呼び出します。

この@<code>{do_marking_step()}では、『アルゴリズム編 2.5.1 SATB』で述べた、SATB集合のキューと初期マークフェーズでマークしたオブジェクトのスキャンを淡々とおこなっていきます。
ここは複雑な上にあまり面白い箇所でもないので、『アルゴリズム編』で書いたくらいの情報がわかっていれば問題ないでしょう。

==[column] タスクスティーリング
並行マークフェーズは複数のスレッドを使ってオブジェクトのスキャンというタスクをこなします。
ここで気になるのがタスク量の問題です。
もしスレッドAがこなすタスク量が多ければ、他のスレッドBはスレッドAのタスクが終了するのを待たなければなりません。
この場合、スレッドBはタスクをこなさない状態で待つことになります。
これは無駄ですよね。
機械にサボらせてはいけません。

じゃあ、同じ量のタスクをあらかじめ振り分ければいいじゃないか、と思うかもしれませんが、多くのケースでそれはうまくいきません。
なぜなら正確なタスクの量を計測するのはコストがかかるからです。
このケースだと、オブジェクトのスキャンというタスクの量は、オブジェクトがもつ参照関係の深さに依存しています。
オブジェクトの参照関係を調べるためにはかなりコストがかかります。

そのため、ここの並行マークフェーズではタスクスティーリングというアルゴリズムを使って、複数のスレッドの仕事量をバランスしています。
スレッドBは自分のタスクが終わったただ待っているのではなく、スレッドAのタスクを盗んで意欲的にタスクをこなすというアルゴリズムです。

HotspotVMではタスクスティーリングを簡単に利用できるユーティリティが実装されており、主に並列マーキングにタスクスティーリングが活用されています。
なかなか面白いアルゴリズムですので、もし興味がある方は以下の記事や発表（手前味噌ではありますが）を読んでみてください。

 * @<href>{http://www.nminoru.jp/~nminoru/programming/arora_dequeue.html, Arora's Task Stealing Deque}
 * @<href>{http://www.slideshare.net/authorNari/crubygc, CRubyGCの並列世界}

== ステップ3―最終マークフェーズ
次のステップはマークしきれなかったオブジェクトをミューテータを止めてスキャンするステップです。
@<code>{ConcurrentMarkThread}の@<code>{run()}にて、@<code>{VM_CGC_Operation}を使って最終マークフェーズが開始されます。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.cpp:再掲]{
             /* 3. 最終マークフェーズ */
150:         if (!cm()->has_aborted()) {

165:           CMCheckpointRootsFinalClosure final_cl(_cm);
166:           sprintf(verbose_str, "GC remark");
167:           VM_CGC_Operation op(&final_cl, verbose_str);
168:           VMThread::execute(&op);
169:         }
//}

@<code>{VM_CGC_Operation}の定義は次のとおりです。

//source[share/vm/gc_implementation/g1/vm_operations_g1.hpp]{
98:  class VM_CGC_Operation: public VM_Operation {
99:    VoidClosure* _cl;
100:   const char* _printGCMessage;

105:   virtual void doit();

111: };
//}

@<code>{VM_Operation}クラスを継承したクラスで、コンストラクタに@<code>{VoidClosure}等を取ります。

//source[share/vm/gc_implementation/g1/vm_operations_g1.cpp]{
156: void VM_CGC_Operation::doit() {

164:     _cl->do_void();

168: }
//}

VMオペレーションの核の部分である@<code>{doit()}では、@<code>{_cl}の@<code>{do_void()}を呼び出しているだけです。
@<code>{doit()}が実行されるときはすでにセーフポイント上にあり、ミューテータは動いていません。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.cpp]{
66: class CMCheckpointRootsFinalClosure: public VoidClosure {
68:   ConcurrentMark* _cm;
69: public:
70: 
71:   CMCheckpointRootsFinalClosure(ConcurrentMark* cm) :
72:     _cm(cm) {}
73: 
74:   void do_void(){
75:     _cm->checkpointRootsFinal(false);
76:   }
77: };
//}

@<code>{CMCheckpointRootsFinalClosure}クラスの@<code>{do_void()}は、@<code>{ConcurrentMark}の@<code>{checkpointRootsFinal()}を呼び出すだけです。

この@<code>{checkpointRootsFinal()}では、並行マークフェーズでスキャンしきれなかったオブジェクトを複数スレッドで並列にスキャンしていきます。
この辺りも『アルゴリズム編 2.6 ステップ3―最終マークフェーズ』の内容がわかっていれば問題ないでしょう。

== ステップ4―生存オブジェクトカウント

== ステップ5―後始末
